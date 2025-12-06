# VUPRS Server - 车下故障声学定位与识别系统 `Linux` 服务器.

<img src="./docs/server_structure.jpg" alt="Server" style="width:600px; height:auto;" /> 

## 网络通信协议

[网络通信协议V1.0](./PROTOCOL.md)

## Build

在项目根目录输入以下指令:  

    sudo mkdir build
    cd build
    sudo cmake .. -DCMAKE_TOOLCHAIN_FILE=../rk3568_toolchain.cmake
    sudo make

## Usage

# 读写测试工具 `FPGA-Tool`  

使用手册 [`FPGA-Tool`使用手册](./ug-fpga-tool.md)  
