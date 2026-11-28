# 构建与使用指南

本文描述 VUPRS Server 的编译、部署和运行流程。协议细节见 [`protocol_cmd.md`](protocol_cmd.md)，FPGA 调试工具见 [`ug-fpga-tool.md`](ug-fpga-tool.md)。

## 1. 环境准备

- **开发机**：Linux x86_64，CMake >= 3.10，支持 C++17 的 aarch64 交叉工具链。
- **目标板**：RK3568（ARM64），可运行的 Linux 系统，具备 XDMA 所需的 PCIe 环境。
- **内核**：目标板内核源码及构建环境，用于编译 `pcie/xdma` 驱动。
- **依赖**：Asio、Eigen、FFTW3、nlohmann/json、RKNN Runtime、spdlog 均已内置于 `3rdparty/`，无需额外安装。

仅构建网络层测试时不需要目标板工具链和内核源码，可直接在开发机上使用主机编译器。

## 2. 配置交叉编译工具链

编辑 [`cmake/rk3568_toolchain.cmake`](../cmake/rk3568_toolchain.cmake)，填写实际的交叉编译器路径：

```cmake
set(CMAKE_C_COMPILER   "/usr/local/arm/gcc-linaro-7.5.0-2019.12-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-gcc")
set(CMAKE_CXX_COMPILER "/usr/local/arm/gcc-linaro-7.5.0-2019.12-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-g++")
```

## 3. 配置 XDMA 驱动构建参数

编辑 [`build.sh`](../build.sh) 顶部的三个变量：

```shell
XDMA__ARCH="arm64"
XDMA__CROSS_COMPILE="/path/to/aarch64-linux-gnu-"
XDMA__KERNEL_DIR="/path/to/rk356x/kernel"
```

## 4. 构建

在仓库根目录执行：

```bash
sudo sh ./build.sh all       # XDMA 驱动 + 服务器 + FPGA 工具
sudo sh ./build.sh xdma      # 仅 XDMA 驱动
sudo sh ./build.sh server    # 仅服务器 + FPGA 工具
```

清理命令：

```bash
sudo sh ./build.sh clean-all
sudo sh ./build.sh clean-xdma
sudo sh ./build.sh clean-server
```

构建产物：

```text
build/
├── server                    # 服务器可执行文件
├── fpga/cli/fpga_tool        # FPGA 调试工具
├── xdma/xdma.ko              # XDMA 内核驱动
└── 3rdparty/fftw3/           # FFTW 动态库
```

`build.sh` 还会把 `net_config.sh`、`mount_xdma.sh` 复制到仓库根目录，并在构建产物齐全时自动调用 `create_run_dir.sh` 生成运行目录。

## 5. 生成运行目录

```bash
sudo sh ./scripts/create_run_dir.sh
```

该脚本会生成 `vuprs/` 目录，包含：

```text
vuprs/
├── run_server.sh             # 启动脚本
├── server                    # 服务器
├── xdma.ko                   # XDMA 驱动
├── configs/                  # 7 个 JSON 配置
├── fftw3/                    # FFTW 动态库
├── rknpu2/                   # RKNN Runtime 动态库
└── model/                    # 推理模型目录
```

将 RKNN 模型放到 `vuprs/model/model.rknn`（或修改 `configs/rknn_config.json` 中的 `rknn-model.path`）。

如果开发机挂载了 VMware 共享目录 `/mnt/hgfs/share-win`，脚本会额外把 `vuprs/` 和 `fpga_tool` 拷贝到该目录，方便传输到板端。

## 6. 配置板端网络（可选）

编辑 [`scripts/net_config.sh`](../scripts/net_config.sh) 中的网络参数：

```shell
INTERFACE="eth0"
STATIC_IP="192.168.1.100"
NETMASK="255.255.255.0"
GATEWAY="192.168.1.1"
DNS="114.114.114.114"
```

在板端执行：

```bash
sudo sh ./net_config.sh
```

脚本会把原配置备份为 `/etc/network/interfaces.bak_vuprs`。

## 7. 启动服务器

将 `vuprs/` 目录拷贝到板端任意位置，进入目录后以 **root** 权限执行：

```bash
cd vuprs/
sudo sh ./run_server.sh
```

启动脚本会：

1. 卸载已加载的 `xdma` 模块；
2. 以 `interrupt_mode=3`（MSI-X）加载 `xdma.ko`；
3. 设置 FFTW/RKNN 动态库路径；
4. 将 7 个配置文件路径传给 `server` 并启动。

停止服务器时在运行终端按 `Ctrl+C` 即可；驱动模块可用 `rmmod xdma` 卸载。

## 8. 配置文件

| 文件 | 说明 |
| :--- | :--- |
| `fpga_config.json` | FPGA 总线地址、DMA 与寄存器布局 |
| `server_config.json` | TCP 端口、最大负载、数据/控制队列长度 |
| `array_config.json` | 麦克风阵列坐标与 ADC 通道分配 |
| `fir_config.json` | FIR 滤波器参数 |
| `rknn_config.json` | 推理频率范围、帧时长、模型路径 |
| `hybrid_default_config.json` | 采样率、目标指向、声速、DMA 缓冲等默认参数 |
| `logger_config.json` | 日志输出目录 |

## 9. 主机侧网络测试

网络层（`server/net/`）不依赖 FPGA/RKNN，可在开发机上单独构建和测试：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target vuprs_mock_server vuprs_net_tests
ctest --test-dir build -R vuprs_net_tests --output-on-failure
```

启动 mock server：

```bash
./build/vuprs_mock_server --bind 127.0.0.1 --port 18080 --rate 2
```

## 10. FPGA 调试工具

先加载 XDMA 驱动，再运行 `fpga_tool`：

```bash
sudo sh ./mount_xdma.sh
./build/fpga/cli/fpga_tool
```

详细命令见 [`ug-fpga-tool.md`](ug-fpga-tool.md)。

## 11. 常见问题

- **`build.sh` 报内核路径错误**：确认 `XDMA__KERNEL_DIR` 指向已配置、已编译的目标内核源码树。
- **驱动加载失败**：确认以 root 执行，且 PCIe 设备已被系统识别；可先执行 `rmmod xdma` 再重试。
- **启动时找不到动态库**：确认 `vuprs/fftw3/` 与 `vuprs/rknpu2/` 存在；`run_server.sh` 会自动设置 `LD_LIBRARY_PATH`。
- **推理始终未启动**：检查 `configs/rknn_config.json` 中的模型路径，以及 DMA buffer 长度是否足够容纳一帧推理数据。
- **配置文件路径**：`run_server.sh` 默认使用当前目录下的 `./configs/`，可通过修改脚本中的 7 个路径变量自定义。
