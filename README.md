# VUPRS Server - 车下故障识别与定位系统板端服务器.

## Build

在项目根目录输入以下指令:  

    sudo mkdir build
    cd build
    sudo cmake .. -DCMAKE_TOOLCHAIN_FILE=../rk3568_toolchain.cmake
    sudo make

## Usage

# 读写测试工具 `FPGA-Tool`  

使用手册 [`FPGA-Tool`使用手册](./ug-fpga-tool.md)  
