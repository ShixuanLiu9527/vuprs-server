# VUPRS 网络通信协议

> 协议版本: 1  
> 传输: TCP，单端口，little-endian  
> 实现: `server/net/`（Asio 1.30.2，Standalone）  
> 说明: 旧版 `[HEADER]/[TAILER]` 轮询协议已移除，本文是唯一协议。

## 1. 传输模型

- TCP 单端口，默认 `8080`。
- 同时只维护一个客户端；新连接替换旧连接。
- 每帧 = 40 字节固定头 + `payload_bytes` 字节 payload。
- 客户端发送控制请求；服务端返回控制响应并主动推送 BF/扫描数据。
- 不再有 `get_bf/get_scan` 轮询。
- 控制响应优先于数据帧发送；同一时刻只有一个写者，帧不会穿插。

## 2. 帧格式

| 偏移 | 大小 | 字段 | 说明 |
| ---: | ---: | :--- | :--- |
| 0 | 4 | `magic` | 字节 `"VUPR"` |
| 4 | 2 | `version` | `1` |
| 6 | 2 | `type` | 消息类型 |
| 8 | 4 | `flags` | 保留，当前 0 |
| 12 | 4 | `payload_bytes` | payload 长度，最大 8 MB（可配置） |
| 16 | 4 | `request_id` | 控制请求/响应配对；数据帧为 0 |
| 20 | 4 | `reserved` | 0 |
| 24 | 8 | `sequence` | 数据帧序号；控制帧为 0 |
| 32 | 8 | `timestamp_ns` | 服务器发送时间（UTC 纳秒） |

服务端和客户端都先读满 40 字节，再按 `payload_bytes` 读取 payload；不搜索尾标记。

## 3. 消息类型

| 值 | 类型 | 方向 | payload |
| ---: | :--- | :--- | :--- |
| `0x0001` | `CONTROL_REQUEST` | 客户端 → 服务端 | JSON |
| `0x0002` | `CONTROL_RESPONSE` | 服务端 → 客户端 | JSON |
| `0x0003` | `BF_DATA` | 服务端 → 客户端 | 二进制 |
| `0x0004` | `SCAN_DATA` | 服务端 → 客户端 | 二进制 |

magic/version 错误、未知类型、方向错误、payload 超限时会关闭连接。

## 4. 控制协议

请求：

```json
{"cmd":"start","params":{}}
```

响应：

```json
{"ok":true,"cmd":"start","error":0,"info":"","params":{}}
```

错误码：

| code | 含义 |
| ---: | :--- |
| 0 | 成功 |
| 1 | JSON 格式错误 |
| 2 | 未知命令 |
| 3 | 参数非法 |
| 4 | 设备/算法未就绪 |
| 5 | 忙（控制队列满） |
| 6 | 内部错误 |

命令：

| 命令 | params | 说明 |
| :--- | :--- | :--- |
| `ack` | `{}` | 协议版本、服务器版本、能力 |
| `start` | `{}` | 启动波束形成器 |
| `stop` | `{}` | 停止波束形成器 |
| `reset` | `{}` | 停止后按当前配置重启 |
| `change_beamformer` | `{"beamformer":"mvdr"}` | `mvdr/cbf/dcrcb` |
| `redirect` | `{"alt":"44.32","az":"-121.34"}` | 修改指向 |
| `change_algorithm_parameters` | 部分字段 | 修改并重启 |
| `read_algorithm_parameters` | `{}` | 读取参数 |
| `enable_scan` | `{"points":"70","alt_min":"15.0"}` | 开启扫描并推送扫描帧 |
| `disable_scan` | `{}` | 关闭扫描 |
| `get_status` | `{}` | 运行/连接/推理/扫描状态 |

所有数值参数仍使用 JSON 字符串。

## 5. BF_DATA

payload 前 32 字节为数据头，随后是 `sample_count` 个 little-endian `int32` Q16.16 采样：

| 偏移 | 大小 | 字段 |
| ---: | ---: | :--- |
| 0 | 4 | `sample_rate_millihz`（Hz × 1000） |
| 4 | 4 | `sample_count` |
| 8 | 4 | `sample_format`：`1 = int32 Q16.16` |
| 12 | 4 | `inference_identity`（int32，-1 表示无效） |
| 16 | 4 | `inference_valid`（0/1） |
| 20 | 12 | reserved |

- `sequence` 每个 BF 帧递增，客户端据此检测丢帧。
- 故障类别直接从 BF 头读取，不再有独立故障流。
- 服务端在波束形成器运行且数据客户端存在时推送。

## 6. SCAN_DATA

payload 前 32 字节为数据头，随后是 `data_points` 个 little-endian `uint16` 能量值：

| 偏移 | 大小 | 字段 |
| ---: | ---: | :--- |
| 0 | 4 | `points_half`（斐波那契点半数量） |
| 4 | 4 | `data_points`（实际点数） |
| 8 | 4 | `sample_rate_millihz` |
| 12 | 4 | `alt_min_mdeg`（度 × 1000） |
| 16 | 4 | `min_power_mdb`（dB × 1000） |
| 20 | 4 | `max_power_mdb`（dB × 1000） |
| 24 | 4 | `scan_format`：`1 = uint16 min-max normalized` |
| 28 | 4 | reserved |

客户端用 `points_half/alt_min_mdeg` 生成同一套斐波那契网格，用 `data_points` 校验长度，用 `min/max_power_mdb` 还原 dB：

```text
power_dB = min_power_mdb/1000 + (raw / 65535.0) * (max_power_mdb - min_power_mdb)/1000
```

扫描帧只在 `enable_scan` 后推送；`disable_scan` 后停止。

## 7. 发送与背压

- 单连接单写者；控制响应优先，数据帧其次。
- 数据队列默认 64 帧；满时丢最旧帧，客户端通过 `sequence` 缺口检测。
- 控制响应队列默认 16；满时关闭连接。
- 新客户端连接替换旧客户端。
- `get_status` 返回运行状态、连接状态、推理是否可用等；队列深度/丢帧统计可按需扩展。
- 控制命令在独立 worker 线程串行执行，`start/stop` 不会阻塞网络读写。

## 8. 服务端实现位置

```text
server/net/frame.h/.cpp            # 帧头、BF/扫描数据头、OutFrame、编解码
server/net/control_worker.h/.cpp   # IControlService + 单线程命令执行
server/net/session.h/.cpp          # 单连接读写、优先级写队列、JSON 控制
server/net/frame_publisher.h/.cpp  # DSP/扫描线程 -> Session 的线程安全桥
server/net/asio_server.h/.cpp      # 单端口 acceptor、io_context、生命周期
server/server_core.h/.cpp          # 算法/配置/命令执行，发布 BF/扫描帧
server/test/mock_control_service.h  # mock 控制服务
server/test/mock_server.cpp         # 无板卡 mock server
server/test/net_tests.cpp           # 帧编解码 + loopback 控制/数据测试
```

Asio: `3rdparty/asio`，Standalone Asio 1.30.2，Boost Software License。

## 9. 构建与联调

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target vuprs_mock_server vuprs_net_tests
ctest --test-dir build -R vuprs_net_tests --output-on-failure
./build/vuprs_mock_server --bind 127.0.0.1 --port 18080 --rate 2
```

mock server 支持控制命令和合成 BF/扫描数据，不依赖 FPGA/RKNN，可在 Windows/Linux 主机运行，便于客户端联调。
