# VUPRS Server - 车下故障声学定位与识别系统 `Linux` 服务器.

<div align="center">
  <img src="./docs/server_structure.jpg" alt="Server" style="width:800px; height:auto;" />
</div>

[[ 回到主仓库: vuprs-support ]](https://github.com/ShixuanLiu9527/vuprs-support.git)

## Build

在项目根目录输入以下指令:  

```bash
sudo mkdir build
cd build
sudo cmake .. -DCMAKE_TOOLCHAIN_FILE=../rk3568_toolchain.cmake
sudo make
```

为了正常编译, 请在 `rk3568_toolchain.cmake` 指定交叉编译器路径:

    set(CMAKE_C_COMPILER "/usr/local/arm/gcc-linaro-7.5.0-2019.12-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-gcc")
    set(CMAKE_CXX_COMPILER "/usr/local/arm/gcc-linaro-7.5.0-2019.12-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-g++")

在 `/build` 目录下将会出现如下可执行文件:  

    /build/vuprs_server  # 服务器
    /build/fpga_tool/tool  # FPGA Tool 工具

    # FFTW3 动态库

    /build/fftw3/libfftw3.so
    /build/fftw3/libfftw3.so.3
    /build/fftw3/libfftw3.so.3.6.9
    /build/fftw3/libfftw3_threads.so
    /build/fftw3/libfftw3_threads.so.3
    /build/fftw3/libfftw3_threads.so.3.6.9

## Run Server

编译完成后, 按照如下方式组织文件: 

    /run_dir
        system_run.sh    # 本仓库提供的脚本文件
        xdma.ko          # XDMA 驱动
        vuprs_server     # 编译得到的服务器可执行文件
        /fftw3           # FFTW3 动态链接库
            libfftw3.so
            libfftw3.so
            libfftw3.so.3
            libfftw3.so.3.6.9
            libfftw3_threads.so
            libfftw3_threads.so.3
            libfftw3_threads.so.3.6.9

执行脚本文件以运行服务器:

```bash
cd run_dir/
chmod +x ./system_run.sh
./system_run.sh
```

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
