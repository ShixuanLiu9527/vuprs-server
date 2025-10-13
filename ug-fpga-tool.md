# FPGA-Tool 使用手册

## 构建说明

```bash
sudo mkdir build
cd build
sudo cmake .. -DCMAKE_TOOLCHAIN_FILE=../rk3568_toolchain.cmake
sudo make
```

## 工具简介

FPGA-Tool 是一个功能强大的 FPGA 通信测试工具，支持多种总线接口和访问方式，包括 AXI-Lite、AXI-Full 以及 XDMA 控制寄存器访问。

## 命令参数详解

### 基本参数模式

| 参数 | 说明 | 取值示例 |
|------|------|----------|
| `--rw` | 读写方向 | `r`(读), `w`(写) |
| `--bus` | 总线选择 | `lite`(AXI-Lite), `full`(AXI-Full) |
| `--cfg` | 配置文件路径 | `./fpga_config.json` |
| `--base` | 基地址(AXI-Lite专用) | `0`, `0x10000` |
| `--offset` | 地址偏移 | `0x04`, `0x00` |
| `--bytes` | 传输字节数(AXI-Full专用) | `1024`, `65536` |
| `--io` | 输入输出数据 | 数值或文件名 |

### 快捷参数模式

| 参数 | 说明 | 对应功能 |
|------|------|----------|
| `--def-opt r-lite` | AXI-Lite 读操作 | 读取 AXI-Lite 寄存器 |
| `--def-opt w-lite` | AXI-Lite 写操作 | 写入 AXI-Lite 寄存器 |
| `--def-opt r-full-reg` | AXI-Full 寄存器读 | 内存映射方式读取 |
| `--def-opt w-full-reg` | AXI-Full 寄存器写 | 内存映射方式写入 |
| `--def-opt r-full-buf` | AXI-Full DMA 读 | DMA 方式读取数据 |
| `--def-opt w-full-buf` | AXI-Full DMA 写 | DMA 方式写入数据 |
| `--def-opt r-ctrl` | XDMA 控制寄存器读 | 读取 XDMA 控制寄存器 |
| `--def-opt w-ctrl` | XDMA 控制寄存器写 | 写入 XDMA 控制寄存器 |

## 使用方法

### 获取帮助信息

```bash
./fpga_tool -h
./fpga_tool --help
```

### AXI-Lite 总线访问

#### 读取 AXI-Lite 寄存器
```bash
# 基本参数方式
./fpga_tool --rw r --bus lite --cfg ./fpga_config.json --base 0 --offset 0x04

# 快捷方式
./fpga_tool --def-opt r-lite --base 0 --offset 0x04
```

#### 写入 AXI-Lite 寄存器
```bash
# 基本参数方式
./fpga_tool --rw w --bus lite --cfg ./fpga_config.json --base 0 --offset 0x00 --io 0xFF

# 快捷方式
./fpga_tool --def-opt w-lite --base 0 --offset 0x00 --io 0x12
```

### AXI-Full 总线访问

#### 寄存器映射方式访问
```bash
# 读取 AXI-Full 寄存器
./fpga_tool --rw r --bus full --cfg ./fpga_config.json --offset 0
./fpga_tool --def-opt r-full-reg --offset 0

# 写入 AXI-Full 寄存器
./fpga_tool --rw w --bus full --cfg ./fpga_config.json --offset 0 --io 0x1234
./fpga_tool --def-opt w-full-reg --offset 0 --io 0x1234
```

#### DMA 方式批量数据传输
```bash
# DMA 读取数据到文件
./fpga_tool --rw r --bus full --cfg ./fpga_config.json --offset 0 --bytes 65536 --io ./r_data.bin
./fpga_tool --def-opt r-full-buf --offset 0 --bytes 65536 --io ./r.bin

# DMA 从文件写入数据
./fpga_tool --rw w --bus full --cfg ./fpga_config.json --offset 0 --bytes 2048 --io ./w_data.bin
./fpga_tool --def-opt w-full-buf --offset 0 --bytes 2048 --io ./w.bin
```

### XDMA 控制寄存器访问

```bash
# 读取 XDMA 控制寄存器
./fpga_tool --def-opt r-ctrl --offset 0

# 写入 XDMA 控制寄存器
./fpga_tool --def-opt w-ctrl --offset 0 --io 0x1234
```

## 参数说明

### 总线选项详解

- **AXI-Lite**: 轻量级总线，适合寄存器访问
  - `r-lite`: 读取 AXI-Lite 寄存器
  - `w-lite`: 写入 AXI-Lite 寄存器

- **AXI-Full**: 高性能总线，支持大数据传输
  - `r-full-reg`: 内存映射方式读取
  - `w-full-reg`: 内存映射方式写入
  - `r-full-buf`: DMA 方式读取（推荐大数据传输）
  - `w-full-buf`: DMA 方式写入（推荐大数据传输）

- **XDMA 控制寄存器**
  - `r-ctrl`: 读取 XDMA 控制寄存器
  - `w-ctrl`: 写入 XDMA 控制寄存器

### 配置文件要求

使用快捷参数模式时，工具会在当前目录查找 `fpga_config.json` 配置文件。配置文件格式请参考：[fpga_config_template.json](./fpga_config_template.json)

### 注意事项

1. AXI-Full 总线的 DMA 访问默认使用通道 0
2. AXI-Lite 的读写操作固定为 4 字节
3. AXI-Full 的寄存器访问方式适合小数据量操作
4. DMA 方式适合大数据块的传输，性能更优
5. 使用前请确保 FPGA 设备已正确连接和配置

## 示例总结

| 功能 | 命令示例 |
|------|----------|
| AXI-Lite 读 | `fpga_tool --def-opt r-lite --base 0 --offset 0x04` |
| AXI-Lite 写 | `fpga_tool --def-opt w-lite --base 0 --offset 0x00 --io 0x12` |
| AXI-Full DMA 读 | `fpga_tool --def-opt r-full-buf --offset 0 --bytes 65536 --io ./r.bin` |
| AXI-Full DMA 写 | `fpga_tool --def-opt w-full-buf --offset 0 --bytes 2048 --io ./w.bin` |
| AXI-Full 寄存器读 | `fpga_tool --def-opt r-full-reg --offset 0` |
| AXI-Full 寄存器写 | `fpga_tool --def-opt w-full-reg --offset 0 --io 0x1234` |
| XDMA 控制寄存器读 | `fpga_tool --def-opt r-ctrl --offset 0` |
| XDMA 控制寄存器写 | `fpga_tool --def-opt w-ctrl --offset 0 --io 0x1234` |