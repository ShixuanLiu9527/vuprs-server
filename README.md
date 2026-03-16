# VUPRS Server - 车下故障声学定位与识别系统 `Linux` 服务器.

<div align="center">
  <img src="./docs/server_structure.jpg" alt="Server" style="width:800px; height:auto;" />
</div>

[[ 回到主仓库: vuprs-support ]](https://github.com/ShixuanLiu9527/vuprs-support.git)

## 项目结构

本项目结构如下:  

    root/
    ├── algorithm      # 项目算法部分代码, 包括信号处理, 波束形成算法等.
    ├── eigen          # Eigen 线性代数库
    ├── fftw3          # FFTW 信号处理库
    ├── nolhmann       # JSON 文件解析库
    ├── fpga           # FPGA 控制相关代码
    ├── fpga_tool      # FPGA 测试工具代码
    ├── logger         # 系统日志管理代码
    ├── server         # 系统服务器代码
    ├── system_tools   # 系统其他函数工具代码
    ├── xdma_driver    # XDMA 驱动源代码
    ├── configs        # 项目配置文件模板.
    └── docs           # 项目相关文档.

## Build

为了正常编译, 请按照以下步骤设置相关参数:  

### Step 1: 在 `rk3568_toolchain.cmake` 指定交叉编译器路径:

    set(CMAKE_C_COMPILER "/usr/local/arm/gcc-linaro-7.5.0-2019.12-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-gcc")
    set(CMAKE_CXX_COMPILER "/usr/local/arm/gcc-linaro-7.5.0-2019.12-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-g++")

[[ 查看 `rk3568_toolchain.cmake` ]](./rk3568_toolchain.cmake)  

### Step 2: 在 `build.sh` 中指定 `XDMA` 交叉编译相关参数:  
```bash
    XDMA__ARCH="arm64"
    XDMA__CROSS_COMPILE="/home/lsx/source/linux/rk356x_linux/prebuilts/gcc/linux-x86/aarch64/gcc-linaro-6.3.1-2017.05-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-"
    XDMA__KERNEL_DIR="/home/lsx/source/linux/rk356x_linux/kernel"
```
[[ 查看 `build.sh` ]](./build.sh)  

### Step 3: 编译:  

在项目根目录运行 `build.sh` 脚本:  
```bash
sudo sh ./build.sh all  # 编译全部 (XDMA 驱动, Server 和 FPGA-Tool)
```
其他使用方法请查看 `build.sh` 的帮助信息:  
```bash
sudo sh ./build.sh help
```
编译完成后, 在 `./build` 目录下将会出现如下可执行文件:  

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

### Step 1: 组织运行目录

编译完成后, 需要按照如下方式组织文件: 

    your/run_dir/
    ├── system_run.sh           # 本仓库提供的脚本文件
    ├── xdma.ko                 # XDMA 驱动
    ├── server                  # 编译得到的服务器可执行文件
    └── fftw3/                  # FFTW3 动态链接库
        ├── libfftw3.so
        ├── libfftw3.so.3
        ├── libfftw3.so.3.6.9
        ├── libfftw3_threads.so
        ├── libfftw3_threads.so.3
        └── libfftw3_threads.so.3.6.9

上述文件目录可以通过 `create_run_dir.sh` 创建:  
```bash
sudo sh ./create_run_dir.sh
```
运行脚本 `create_run_dir.sh` 后, 目录 `run_dir` 将会自动在项目根目录生成.  

### Step 2: 配置启动脚本文件

设置脚本 `run_server.sh` 中的 `4` 个配置文件在板子系统中的位置:  
```bash
FPGA_CONFIG="./fpga_config.json"
SERVER_CONFIG="./server_config.json"
ARRAY_CONFIG="./array_config.json"
FIR_CONFIG="./fir_config.json"
```
[[ 查看 `run_server.sh` ]](./run_server.sh) 

### Step 3: 配置以太网静态 `IP` 地址

在 `config_eth0.sh` 中设置以太网: 
```bash
INTERFACE="eth0"
STATIC_IP="192.168.1.100"
NETMASK="255.255.255.0"
GATEWAY="192.168.1.1"
DNS="114.114.114.114"
```
[[ 查看 `config_eth0.sh` ]](./config_eth0.sh)  
完成后, 在板端运行脚本文件 `config_eth0.sh`.

### Step 4: 启动服务器

将 `run_dir` 目录下载到板端任意位置, 进入目录, 执行脚本文件 `run_server.sh` 即可运行服务器:
```bash
cd run_dir/
sh ./run_server.sh
```
注: 脚本文件 `run_server.sh` 的执行必须在 `root` 下.

## 其他信息

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

### FPGA 读写测试工具 `FPGA-Tool`  

为了方便调试, 设计了 `FPGA` 调试工具, 可以通过终端控制 `FPGA` 中的寄存器.  
为了正确使用 `FPGA-Tool` 请先使用脚本 `load_xdma_driver.sh` 挂载 `xdma` 驱动, 挂载之前请检查脚本同级目录下是否有驱动文件 `xdma.ko`.  
  
[[ 查看 `load_xdma_driver.sh` ]](./load_xdma_driver.sh)  
[[ `FPGA-Tool`使用手册 ]](./docs/ug-fpga-tool.md)  

### 网络通信协议

[网络通信协议V1.0](./docs/PROTOCOL.md)

---
_Shixuan Liu 2025_
