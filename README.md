# VUPRS Server - 车下故障声学定位与识别系统 `Linux` 服务器.

<div align="center">
  <img src="./docs/server_structure.jpg" alt="Server" style="width:800px; height:auto;" />
</div>

[[ 回到主仓库: vuprs-support ]](https://github.com/ShixuanLiu9527/vuprs-support.git)

## Build

为了正常编译, 请按照以下步骤设置相关参数:  

### Step 1: 在 `rk3568_toolchain.cmake` 指定交叉编译器路径:

    set(CMAKE_C_COMPILER "/usr/local/arm/gcc-linaro-7.5.0-2019.12-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-gcc")
    set(CMAKE_CXX_COMPILER "/usr/local/arm/gcc-linaro-7.5.0-2019.12-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-g++")

[[ 查看 `rk3568_toolchain.cmake` ]](./rk3568_toolchain.cmake)  

### Step 2: 在 `./xdma_driver/xdma/Makefile` 中指定相关参数:  

    export ARCH:=arm64  # 平台架构, ARM-64
    export CROSS_COMPILE:=<交叉编译器路径>
    BUILDSYSTEM_DIR:=<内核源代码根目录> # Linux kernel path

[[ 查看 `./xdma_driver/xdma/Makefile` ]](./xdma_driver/xdma/Makefile)  

### Step 3: 编译:  

在项目根目录输入以下指令:  

```bash
sudo sh ./build.sh all  # 编译全部 (XDMA 驱动, Server 和 FPGA-Tool)
sudo sh ./build.sh xdma  # 仅编译 XDMA 驱动
sudo sh ./build.sh server  # 仅编译 Server 和 FPGA-Tool 工具
```

或者查看帮助信息:  

```bash
sudo sh ./build.sh help
```

编译完成后, 在 `/build` 目录下将会出现如下可执行文件:  

    build/
    ├── server                    # 服务器可执行文件
    ├── fpga_tool/
    │   └── tool                  # FPGA Tool 工具
    ├── xdma/
    │   └── xdma.ko               # XDMA 驱动
    └── fftw3/                    # FFTW 动态库
        ├── libfftw3.so
        ├── libfftw3.so.3
        ├── libfftw3.so.3.6.9
        ├── libfftw3_threads.so
        ├── libfftw3_threads.so.3
        └── libfftw3_threads.so.3.6.9

## Run Server

编译完成后, 按照如下方式组织文件: 

    your/run_dir/
    ├── system_run.sh           # 本仓库提供的脚本文件
    ├── xdma.ko                 # XDMA 驱动
    ├── server                  # 编译得到的服务器可执行文件
    └── fftw3/                  # FFTW3 动态链接库
        ├── libfftw3.so
        ├── libfftw3.so
        ├── libfftw3.so.3
        ├── libfftw3.so.3.6.9
        ├── libfftw3_threads.so
        ├── libfftw3_threads.so.3
        └── libfftw3_threads.so.3.6.9

上述文件目录可以通过 `./make_run_dir.sh` 创建:  

```bash
sudo sh ./make_run_dir.sh
```

目录 `run_dir` 将会在项目根目录创建.  

执行脚本文件以运行服务器:

```bash
cd run_dir/
sh ./system_run.sh
```

注: 必须在 `root` 下运行上述指令.

## Usage

### 相关配置文件

为了方便系统运行和调整, 设置了 `4` 个配置文件, 分别为: `fpga_config.json`, `beam_forming_array.json`, `fir_config.json`, `server_config.json`.  
  
[[ 查看 `fpga_config.json` 模板 ]](./configs/fpga_config_template.json)  
[[ 查看 `beam_forming_array.json` 模板 ]](./configs/beam_forming_array_template.json)  
[[ 查看 `fir_config.json` 模板 ]](./configs/fir_config_template.json)  
[[ 查看 `server_config.json` 模板 ]](./configs/server_config_template.json)  
  
`fpga_config.json` 用于配置 `FPGA` 侧各个模块, 方便 `ARM` 端与 `FPGA` 通信.  
`beam_forming_array.json` 用于配置麦克风阵列, 通过配置该文件, 系统可以适配不同的麦克风阵列.  
`fir_config.json` 用于配置 `FIR` 滤波器模块参数.  
`server_config.json` 用于配置服务器信息, 包括服务器端口号, 命令流数据头数据尾等.  

## FPGA 读写测试工具 `FPGA-Tool`  

为了方便调试, 设计了 `FPGA` 调试工具, 可以通过终端手动控制 `FPGA`.  
  
[[ `FPGA-Tool`使用手册 ]](./docs/ug-fpga-tool.md)  

## 网络通信协议

[网络通信协议V1.0](./docs/PROTOCOL.md)

---
_Shixuan Liu 2025_
