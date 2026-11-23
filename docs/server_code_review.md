# VUPRS Server 代码审查记录

> 日期: 2026-11-23  
> 范围: `vuprs-server` 当前 `main` 分支源码（静态审查，未连接真实板卡）  
> 目的: 在开发上位机前确认服务器行为、协议边界和已知缺陷。  
> 说明: 审查后已按本记录修复 P0/P1/P2 中的代码问题；P0-6（SG-DMA 描述符顺序）按需求保留原实现，不修改。  
> 后续变更: 旧 `[HEADER]/[TAILER]` 网络层已删除，服务器已切换到 `server/net` 的 Asio 单协议实现；本文保留为切换前的审查记录。

## 修复状态（2026-11-23）

- P0-1 ~ P0-5、P0-7：已修复。
- P0-6：按要求**不修改**，保持现有 SG-DMA 操作顺序。
- P1-1 ~ P1-11：已修复。
- P2 表中的代码问题：已修复；`CMake` 已加入 `-Wall -Wextra`，sanitizer/单元测试属于后续质量工作，未在本次最小改动中加入。
- 协议新增：数据帧 `sequence`、`timestamp_ns`、`sample_rate`、`sample_count`（BF）、`data_points`（扫描）、`inference_valid`、`sample_format`。
- 未新增：独立 `get_status` 命令、请求 ID、机器可读 `error_code`；这些属于协议扩展建议，不是本次缺陷修复范围。

## 优先级说明

| 级别 | 含义 |
| :--- | :--- |
| P0 | 可能导致服务器崩溃、协议数据错位、启动失败或明显错误结果，建议优先修复 |
| P1 | 在特定时序/输入下会出问题，或影响可维护性、可诊断性 |
| P2 | 代码质量、健壮性、文档和测试方面的改进 |

---

## P0 级问题

### P0-1 客户端断开时可能触发 SIGPIPE，导致服务器进程退出

**位置**

- `server/socket_io_manager.cpp:18-52` — `SendAllWithRetry()`
- `server/socket_io_manager.cpp:74-93` — `SendMessage()` / `SendBuffer()`

**现象**

`send()` 使用 `flags = 0`。在 Linux 上，如果对端已经关闭连接，`send()` 除了返回 `EPIPE`，还会向进程发送 `SIGPIPE`。进程没有忽略该信号时，默认行为是直接终止。

**影响**

客户端在 `get_bf` 大帧传输过程中断开，或客户端异常退出，可能导致整个服务器被杀死。对于需要长期运行的板端服务，这是高优先级稳定性问题。

**建议**

- 使用 `send(..., MSG_NOSIGNAL)`；或
- 在 `main()` 启动时执行 `signal(SIGPIPE, SIG_IGN)`，并在发送失败时主动关闭对应会话。
- `SendAllWithRetry()` 对 `EPIPE/ECONNRESET` 应返回失败并触发会话关闭。

**验证**

客户端发起 `get_bf` 后立即断开，重复多次，服务器应保持运行并能接受新客户端。

---

### P0-2 多段发送不是原子操作，数据帧可能被其他响应插入

**位置**

- `server/socket_io_manager.cpp:74-93`
- `server/linux_server.cpp:397-424`

**现象**

一个二进制数据帧由多次独立发送组成：

```cpp
manager->SendMessage(header);
manager->SendWord<uint32_t>(send_data_size);
manager->SendBuffer<uint32_t>(...);
manager->SendMessage(tailer);
```

`SocketIOManager` 的互斥锁只在单次 `SendMessage()` / `SendWord()` / `SendBuffer()` 内部生效。另一个线程（例如会话线程回复 `invalid` 或普通命令响应）可以在这几个调用之间插入一条 JSON 响应，导致客户端收到的字节流变成：

```text
[HEADER] + uint32 size + [HEADER]{"response_cmd":...}[TAILER] + payload + [TAILER]
```

**影响**

客户端解析器会看到长度字段与后续内容不匹配，轻则丢帧，重则协议解析永久错位。即使用户遵守“一次一个请求”，异常客户端、超时重发、服务器自己的错误响应都可能触发。

**建议**

- 新增 `SendFrame()` / `SendBinaryFrame()`，在一次加锁内写完整个帧（信息帧或二进制帧）。
- 或者为每个客户端引入独立发送队列，由唯一发送线程串行写出。
- 只在整帧写成功后继续后续帧；失败时立即关闭会话。
- 建议同时给数据帧加序列号，便于客户端检测错位。

**验证**

在 `get_bf` 传输 32 KB 数据的同时发送 `ack` 或格式错误命令，客户端应只看到完整、成对的帧，或服务器明确断开连接。

---

### P0-3 `sending_format` 在发送过程中可被覆盖，信息帧与负载可能不匹配

**位置**

- `server/linux_server.cpp:320-435` — `THREAD__Send()`
- `server/linux_server.cpp:614-643` — 控制线程设置 `sending_format`

**现象**

`THREAD__Send()` 在一个发送周期内多次读取 `this->sending_format`：

```cpp
if (IS_BINARY_DATA_SENDING_CMD(this->sending_format)) { ... }
switch (this->sending_format) { ... }  // 生成信息帧
switch (this->sending_format) { ... }  // 选择负载
```

控制线程可以在这些读取之间把 `sending_format` 从 `GET_BF_DATA` 改成 `GET_SCAN_DATA`。同时 `sending_irq` 是布尔值，连续两个数据请求可能被合并，只有最后一次格式生效。

**影响**

可能出现“BF 信息帧 + 扫描负载”或“扫描信息帧 + BF 负载”的错配，客户端按信息帧解析负载会得到错误长度/类型。

**建议**

- 每轮发送开始时把 `sending_format`、`sending_irq` 捕获到局部变量，整轮只使用局部值。
- 用请求队列替代布尔标志；每个请求带 `request_id` 和期望的响应类型。
- 在数据发送完成前暂停处理新的数据请求，或允许排队但不覆盖当前请求。
- 如果暂不重构，至少保证 `get_bf/get_scan` 在途时拒绝/排队新的数据请求。

**验证**

快速交替发送 `get_bf` 和 `get_scan`，检查每个信息帧对应的 `payload_bytes` 和负载类型始终一致。

---

### P0-4 扫描线程在开启后忙循环，持续占用 CPU 并重复入队

**位置**

- `hybrid/hybrid_bf.cpp:445-516` — `THREAD__ScanPowerCalculation()`
- `hybrid/hybrid_bf.cpp:770` — 算法线程 `scan_cv.notify_all()`

**现象**

扫描线程顶部等待条件为：

```cpp
this->scan_cv.wait(lock, [this]()
                   { return this->scan_enable || !this->system_run; });
```

只要 `scan_enable == true`，条件立即为真，线程不会等待“新的协方差数据”或“扫描参数变化”。随后每一轮都重新 `SnapshotScanCovariance()` 并完整计算一次扫描、推入队列。算法线程的 `notify_all()` 实际上没有起到“新数据到达”的作用。

**影响**

- 扫描开启后持续占满一个 CPU 核心（以及扫描线程池），影响波束形成和推理。
- `scan_result_queue` 被重复结果灌满，旧结果不断被丢弃。
- `get_scan` 取到的数据可能是同一协方差快照的多次重复计算。

**建议**

- 增加“新的算法结果/协方差版本”计数或条件变量条件，例如 `scan_generation`，算法线程更新协方差后递增。
- 扫描线程只在以下情况计算：
  - 新的算法数据到达；
  - 扫描参数变化；
  - 第一次开启。
- 计算完成后等待下一次条件，而不是立即进入下一轮。
- `ScanSwitch(true)` 后显式 `scan_cv.notify_all()`。
- 可选：限制扫描计算频率（例如每 N 个 BF 帧一次）。

**验证**

开启扫描后观察 CPU 占用和 `get_scan` 更新频率；扫描帧率应与配置/BF 帧率相关，而不是无限快。

---

### P0-5 推理模型缺失会导致服务器无法启动

**位置**

- `config.h:11` — `#define ENABLE_INFERENCE true`
- `server/linux_server.cpp:78-109` — `InitSystemConfigFiles()`
- `hybrid/hybrid_bf.cpp:79-87` — `InitInference()`
- `fault_detect/fault_detector.cpp:9-74` — `InitDetector()`

**现象**

`InitInference()` 在 `ENABLE_INFERENCE` 为真时直接调用 `InitDetector()`，后者要求模型文件存在且可加载。`InitSystemConfigFiles()` 使用：

```cpp
config_result &= this->beamformer.InitInference(...);
```

模型缺失时 `config_result` 为 false，`main()` 随后 `RUNTIME_CHECK(server.ConfigDone(), ...)` 终止进程。

**影响**

在故障识别模型尚未完成/未部署时，服务器无法启动，连波束形成和扫描功能也无法使用。这与当前“故障功能暂未做好”的开发阶段冲突。

**建议**

- 增加运行时配置项（如 `enable-inference`），而不是只依赖编译宏。
- 模型加载失败时记录 warning，设置 `inference_valid=false`，继续提供 BF/扫描服务；`inference_identity` 返回 `-1`。
- 将推理初始化从“系统启动必需条件”中拆出，或者提供 `--disable-inference` 命令参数。
- 客户端应能处理 `-1` 和没有故障数据的场景（已在协议文档中说明）。

**验证**

删除/重命名模型文件启动服务器，服务器应能正常提供 `get_bf/get_scan`，`inference_identity` 为 `-1`。

---

### P0-6 DMA SG 描述符在启动 DMA 之后才写入 SG BRAM（按要求不修改）

> 该操作是当前规范的 SG-DMA 流程，按需求保留原实现。

**位置**

- `fpga/fpga_api.cpp:283-337` — `FPGA_API__DMA__StartScatterGatherDMA_S2MM()`

**现象**

当前顺序大致为：

1. 写 `S2MM_CURDESC` 指向描述符 0；
2. 启动 DMA（`S2MM_DMACR.RS = 1`）；
3. 等待 `Halted = 0`；
4. 才把描述符链写入 SG BRAM；
5. 最后写 `S2MM_TAILDESC`。

DMA 启动时 SG BRAM 中还没有有效描述符，硬件可能读取到未初始化数据。

**影响**

冷启动/重启时可能出现 DMA 读错误、丢中断、数据错位或 DMA 挂起。也可能是现场偶发“无法匹配描述符”的来源之一。

**建议**

- 先把完整描述符链写入 SG BRAM，并确保写入完成；
- 再写 `S2MM_CURDESC` / `S2MM_CURDESC_MSB`；
- 再写 tail descriptor；
- 最后启动 DMA。
- 建议对照 AXI DMA 手册确认 cyclic 模式下 `S2MM_TAILDESC` 的写入值（当前 `0x50 << 6` 需要硬件确认）。
- 建议复位后轮询 `S2MM_DMASR` 的 error 位，出错时明确上报。

**验证**

反复冷启动/重启，统计 DMA 中断和结果帧是否稳定；用 FPGA-Tool 读取 SG BRAM 检查描述符内容。

---

### P0-7 会话生命周期存在阻塞、竞态和未捕获异常

**位置**

- `server/linux_server.cpp:174-223` — `THREAD__AcceptClient()`
- `server/linux_server.cpp:276-305` — `Stop()`
- `server/linux_session.cpp:108-151` — `ReceiveLoop()`
- `server/socket_io_manager.cpp:95-143` — `ReceiveMessage()`

**现象与影响**

1. `THREAD__AcceptClient()` 中 `server_session->Start()` 是**阻塞调用**，它会一直执行 `ReceiveLoop()` 直到客户端断开。因此：
   - 日志“successfully connect client”和 `ConnectCallback(true)` 实际发生在会话结束之后，而不是连接建立时；
   - 该线程在连接期间无法继续 `accept()`（单客户端场景可接受，但语义错误）。
2. `server_session` 是裸 `std::unique_ptr`，在接收线程和 `Stop()` 之间没有互斥保护，`Stop()` 可能在接收线程使用它时 `reset()`。
3. `ReceiveLoop()` 没有检查 `data.is_error`。如果 `recv()` 返回非超时错误，`is_connect` 仍为 true，循环可能持续空转而不退出。
4. `ReceiveMessage()` 和管理器为空时会通过 `PARAM_CHECK` 抛异常；`ReceiveLoop()` 没有捕获，线程函数抛出未处理异常会调用 `std::terminate()`。

**建议**

- 为每个会话使用独立线程，`Start()` 不阻塞 accept 线程；或者明确 `accept` 线程只负责接收并在结束后回调。
- 使用互斥锁或共享所有权保护 `server_session`。
- `ReceiveLoop()` 增加 `try/catch`，处理 `is_error/is_timeout/!is_connect`，并确保关闭 fd。
- 连接/断开回调在正确时机触发。
- 对线程函数统一加异常边界，避免任何 `RUNTIME_CHECK/PARAM_CHECK` 直接终止进程。

**验证**

反复连接/断开、服务器停止时客户端仍连接、会话中途重置；进程不应崩溃或卡死。

---

## P1 级问题

### P1-1 扫描结果队列未清空，信息帧与负载可能不是同一套参数

**位置**

- `hybrid/hybrid_bf.cpp:106-126` — `ScanOptions()`
- `hybrid/hybrid_bf.cpp:415-434` — `ReadScanPower()`
- `server/linux_server.cpp:363-395` — 发送扫描信息帧

**现象**

修改 `points/alt_min` 时只更新配置和网格，没有清除 `scan_result_queue`。`get_scan` 的 JSON 信息帧使用当前 `scan_config`，但二进制负载可能还是旧网格的结果。

**影响**

客户端热力图点位与能量值错位；点数不一致时可能越界或显示错误位置。

**建议**

- 参数变化时清空 `scan_result_queue`，并同步 `new_scan_points_input`。
- 每个扫描结果附带配置标识（points/alt_min/网格版本）。
- 响应中增加 `data_points`（实际点数）和 `grid_id`。
- 只返回与当前配置匹配的结果。

---

### P1-2 `MatchDescriptor()` 使用未初始化变量；`GetCurrentDescriptor()` 永远失败

**位置**

- `fpga/fpga_device.cpp:85-123` — `MatchDescriptor()`
- `fpga/fpga_api.cpp:373-393` — `FPGA_API__DMA__GetCurrentDescriptor()`

**现象**

`MatchDescriptor()` 中 `next_addr` / `previous_addr` 在找到 current 描述符后才赋值；如果 current 未找到，第二个循环仍会读取未初始化变量（未定义行为）。

`FPGA_API__DMA__GetCurrentDescriptor()` 中：

```cpp
bool operate_status = true, found = false;
...
operate_status &= MatchDescriptor(...);
RUNTIME_CHECK(found, ...);
```

`found` 从未被赋值，因此该函数无论匹配是否成功都会抛异常。当前代码中该函数似乎未被使用，但属于明确的缺陷。

**建议**

- current 未找到时立即 `return false`，不要继续使用未初始化地址。
- 初始化 `next_addr/previous_addr`。
- `GetCurrentDescriptor()` 直接使用 `MatchDescriptor()` 的返回值，或移除这个未使用函数。

---

### P1-3 帧格式解析会删除 JSON 字符串内部空白，且只支持单帧/1024 字节

**位置**

- `server/socket_io_manager.cpp:95-143` — `ReceiveMessage()`
- `server/socket_io_manager.cpp:164-185` — `CheckFrameFormat()`

**现象**

- `CheckFrameFormat()` 对整段数据执行 `remove_if`，删除空格、换行、回车、制表符。这会破坏 JSON 字符串值中的空格（例如未来如果加入带空格的 `beamformer` 名称或描述字段）。
- 接收缓冲区固定 1024 字节；超过 1024 字节的命令会被截断。
- 如果客户端把两帧命令合并到一次 TCP 发送中，`CheckFrameFormat()` 会把整段当成一帧，导致 JSON 解析失败。

**建议**

- 只去除帧边界外的空白，不要修改 JSON 内容；或者直接按长度/边界解析。
- 改用增量缓冲/动态长度，并支持一帧或多帧。
- 对超过限制或格式错误的请求返回明确错误。
- 文档已经建议客户端保持单请求、紧凑 JSON；服务器侧仍应增强健壮性。

---

### P1-4 参数校验缺少有限性和上限保护

**位置**

- `server/protocol.cpp:90-111` — `enable_scan` 参数
- `hybrid/hybrid_bf.cpp:89-104` — `CheckScanningConfigValid()`
- `fpga/fpga_device.cpp:169-202` — `GetSCIValueForSamplingFrequency()`

**现象**

- `alt_min = NaN` 时，`alt_min < 0.0 || alt_min > 90.0` 均为 false，NaN 会通过校验。
- `points` 只有 `> 0` 限制，没有上限；极大值会让 `FibonacciGrid` 分配巨大内存并做海量计算。
- `fs` 只限制 `> 1e-2`，极低采样率会让 `50MHz / (2*fs)` 超出 `uint32_t`，`SCI` 溢出；`SCI_l = SCI - 1` 也可能下溢。
- `GetSCIValueForSamplingFrequency()` 对非法值返回 `0xFFFFFFFF`，调用方没有检查就写入寄存器。

**建议**

- 所有浮点参数使用 `std::isfinite()` 校验。
- 为 `points` 设置合理上限（例如 10000 或更小，取决于算法内存/实时性）。
- 为 `fs` 设置实际可支持的下限，并检查 `SCI` 溢出。
- 检查 `0xFFFFFFFF` 哨兵，非法时返回失败而不是写寄存器。
- 对扫描网格内存占用做上限检查。

---

### P1-5 数据响应缺少序号、时间戳、采样点数和实际扫描点数

**位置**

- `server/protocol.cpp:189-232`
- `server/linux_server.cpp:397-435`

**现象**

`get_bf` 信息帧只有 `data_format`、`quantization`、`inference_identity`；`get_scan` 信息帧没有实际负载点数。客户端只能从二进制长度反推，无法直接检测丢帧、乱序或扫描网格不匹配。

**建议**

在信息帧中增加向后兼容字段：

```json
{
  "sample_count": "8192",
  "sample_rate": "10000.000000",
  "sequence": "12345",
  "timestamp_ns": "1700000000000000000",
  "inference_valid": "1",
  "scan_data_points": "75"
}
```

- 序号由服务器递增，客户端可检测丢帧。
- 时间戳建议使用服务器单调时钟或 UTC 时间。
- 扫描帧增加实际点数、网格版本/参数。

---

### P1-6 `quantization` 字段与实际数据格式不一致

**位置**

- `server/protocol.cpp:189-201` — `get_bf` 响应
- `hybrid/hybrid_bf.cpp:613` — `buffer.to_vector<uint32_t>()`
- `fpga/fpga_data_type.h:15-16` — BF result 为 Q16.16

**现象**

`get_bf` 信息帧硬编码 `"quantization":"q31"`，但发送的 `signal` 是 FPGA DDR 中的原始 Q16.16 数据。Q31 只用于服务器内部的推理结果向量，且没有通过网络发送。

**影响**

客户端如果按 Q31 解码，幅值会相差 65536 倍。

**建议**

- 将字段改为 `"q16.16"`，或增加明确的 `"sample_format":"int32_q16.16"`。
- 在协议文档/README 中同步说明。
- 增加一个已知幅值的测试帧，验证客户端解码。

---

### P1-7 扫描功率转 dB 使用 `20 * log10`，功率应为 `10 * log10`

**位置**

- `algorithm/bf/beam_former_template.cpp:468`

**现象**

```cpp
total_power = total_power.array().log10() * 20.0; /* Convert to dB */
```

`total_power` 由 `w.H * R * w / M^2` 得到，语义是功率。功率转 dB 通常使用 `10 * log10`；`20 * log10` 对应幅度。若这里确实是功率，绝对 dB 值会整体偏高 6 dB。

**影响**

`get_scan` 返回的 `min_power/max_power` 绝对 dB 值可能不正确；由于所有点同乘一个常数，热力图相对分布不变，但显示/比较绝对能量会误导。

**建议**

- 确认 `total_power` 的定义；若为功率，改为 `10 * log10`。
- 补充一个已知功率的单元测试。
- 如果暂时不修复，在协议文档/UI 中标注为“相对对数能量”，不要当作严格 dBm/dB SPL。

---

### P1-8 停止流程可能允许控制线程在关机过程中重启波束形成器

**位置**

- `server/linux_server.cpp:276-305` — `Stop()`

**现象**

`Stop()` 先调用 `beamformer.stop()`，之后才设置 `server_running = false` 并唤醒/关闭服务器线程。控制线程在此期间仍可能处理客户端命令并调用 `beamformer.run()`，导致关机过程中重新启动算法线程。

**建议**

停止顺序调整为：

1. `server_running = false`；
2. 通知所有条件变量；
3. 关闭 listen/client socket；
4. join 服务器线程；
5. 最后 `beamformer.stop()`。

并确保 `Stop()` 可重复调用、不会重复 join。

---

### P1-9 `main()` 阻塞在 stdin，缺少 SIGINT/SIGTERM 优雅退出

**位置**

- `main.cpp:50-53`

**现象**

```cpp
server.Run();
int val;
std::cin >> val;
server.Stop();
```

服务器启动后必须从标准输入读一个值才会停止。作为后台服务/开机自启运行时，通常没有 stdin；`SIGTERM` 也没有处理函数，进程被直接杀死，硬件/日志可能未清理。

**建议**

- 使用 `SIGINT/SIGTERM` 信号处理设置退出标志，主线程等待条件变量/信号。
- 保留可选的控制台退出命令，但不要作为唯一退出方式。
- 在退出路径中按 P1-8 的顺序停止。

---

### P1-10 算法线程长时间持有 `mut_alg`，阻塞扫描快照

**位置**

- `hybrid/hybrid_bf.cpp:760-787` — `THREAD__AlgorithmCalculation()`
- `hybrid/hybrid_bf.cpp:465-471` — 扫描线程 `SnapshotScanCovariance()`

**现象**

算法线程在 `mut_alg` 锁内完成 `InputSignal`、`UpdateCovarianceMatrix`、`CalculateBeamforming`、FIR 频响求解和系数提取，时间可能较长。扫描线程需要同一把锁复制协方差快照，因此扫描更新会被阻塞。

**建议**

- 在锁内只做协方差/权重的快速快照或交换（double buffer），把重计算移到锁外。
- 为扫描提供独立的一致性快照，避免每次 `SnapshotScanCovariance` 等待整个 BF 周期。
- 结合 P0-4，扫描线程只需要在“新快照可用”时计算一次。

---

### P1-11 `THREAD__ReadResult()` 可能漏读多个已完成的 DMA 缓冲区

**位置**

- `hybrid/hybrid_bf.cpp:555-668`

**现象**

每次 DMA 描述符变化时，只读取 `prev_desc` 对应的一个缓冲区。如果推理/处理耗时导致 DMA 指针一次前进多个描述符，中间的缓冲区不会被读取。

**影响**

高采样率/推理较慢时会丢帧，且没有丢帧计数/序号，客户端无法知道。

**建议**

- 记录上一次读取的描述符，循环读取到 `prev_desc` 为止所有已完成缓冲区。
- 增加溢出/丢帧计数并通过协议暴露。
- 如果无法全部处理，至少保留最新数据并明确上报丢弃数量。

---

## P2 级问题

| 位置 | 问题 | 建议 |
| :--- | :--- | :--- |
| `hybrid/hybrid_bf.cpp:671-689` | `r_val` 在 `ReadSingleRegisterBIT` 抛异常后未初始化，随后仍参与 `if (r_val == 0x01)` | 初始化为 0；异常时直接 `continue` |
| `hybrid/hybrid_bf.cpp:764` | `array_signal_queue.front()` 前未检查空队列 | 增加空队列检查；线程函数外层捕获异常 |
| `fpga/fpga_api.cpp:373-393` | `GetCurrentDescriptor()` 的 `found` 永远为 false | 删除或改用 `MatchDescriptor()` 返回值 |
| `fpga/fpga_io_manager.cpp:123-184` | 寄存器绝对地址没有 mmap 范围检查；`WriteSingleRegisterBIT` 的读-改-写不是原子事务 | 增加边界检查；对需要 RMW 的寄存器加设备级锁 |
| `fpga/fpga_api.cpp:283-337` | cyclic 模式 tail descriptor 写入 `0x50 << 6`，需要对照硬件手册确认 | 增加注释/常量，现场验证 |
| `fpga/fpga_device.cpp:30-38` | `s2mm_transfer_register_length` 解析后未使用 | 用于校验 buffer size 是否超出 26-bit 长度 |
| `server/linux_server.cpp:116-172` | 端口范围未校验；`initialize_port > maximum_port` 时直接无服务 | 启动时校验并给出错误 |
| `main.cpp:8-21` | 命令行必须恰好 15 个参数，无 `--help`/usage | 增加 usage 和清晰错误信息 |
| `system_tools/aligned_buffer.cpp:168-206` | `from_file()` 在短读/seek 失败时未 `release()`，存在泄漏到析构前 | 失败路径释放资源 |
| `server/socket_io_manager.cpp:18-52` | 发送失败时未区分 `EPIPE/ECONNRESET`，也未主动关闭会话 | 返回错误类型，通知会话层关闭 |
| `CMakeLists.txt` | 没有 `-Wall -Wextra`、sanitizer、单元测试目标 | 增加编译告警、ASan/UBSan 和协议解析测试 |
| `server/linux_server.cpp:78-109` | 推理初始化是全局启动必需条件 | 与 P0-5 一并改为可选 |
| `server/protocol.cpp:189-232` | 响应字段缺少数据长度/采样率/序号 | 与 P1-5 一并扩展 |

---

## 建议的协议扩展

这些扩展对客户端很有价值，且可以保持向后兼容：

1. **请求/响应序列号**  
   每个客户端请求带 `request_id`，响应原样返回；数据帧也带序列号。

2. **数据帧元数据**  
   `get_bf` 增加 `sample_count`、`sample_rate`、`sequence`、`timestamp_ns`、`inference_valid`。  
   `get_scan` 增加 `data_points`、`grid_id`/`points`/`alt_min` 的明确版本。

3. **正确的格式字段**  
   `sample_format: "int32_q16.16"`、`scan_format: "uint16_minmax_normalized"`。

4. **健康/状态查询**  
   增加 `get_status`：返回波束形成器运行状态、当前 beamformer、fs、队列深度、丢帧计数、推理是否可用、扫描是否开启等。

5. **错误码**  
   当前只有 `"invalid"/"failed"` 和自由文本 `info`；建议增加机器可读的 `error_code`。

6. **可选推理**  
   运行时开关/配置项；模型缺失时降级运行，返回 `inference_identity=-1`。

---

## 建议的回归测试

1. **协议流测试**：分片、合并、二进制负载中包含 `[HEADER]/[TAILER]`、失败信息帧后没有二进制帧。
2. **断连测试**：`get_bf` 传输中客户端断开，服务器不因 SIGPIPE 退出，并能接受新客户端。
3. **并发命令测试**：交替 `get_bf/get_scan/ack`，验证信息帧与负载类型/长度匹配。
4. **扫描参数切换测试**：快速修改 `points/alt_min`，验证负载点数与信息帧一致。
5. **参数边界测试**：NaN/Inf、极大 `points`、极小/极大 `fs`、非法 JSON 数字。
6. **启动降级测试**：无模型文件启动，服务器仍可提供 BF/扫描。
7. **DMA 冷启动测试**：反复重启，检查描述符、DMA 中断和结果帧。
8. **长时间运行测试**：扫描开启 + 高频 `get_bf`，观察 CPU、内存、队列和丢帧。

---

## 本次未修改的代码

本次仅更新了 `docs/protocol_cmd.md` 和新增本审查记录，未修改任何服务器业务代码。P0/P1 修复建议需要单独提交并由硬件/真实板卡验证。
