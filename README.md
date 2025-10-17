# VUPRS Server - 车下故障识别与定位系统板端服务器.

## Build

在项目根目录输入以下指令:  

    sudo mkdir build
    cd build
    sudo cmake .. -DCMAKE_TOOLCHAIN_FILE=../rk3568_toolchain.cmake
    sudo make

## Overview

<img src="docs/server_structure.jpg" alt="server" style="width:1000px; height:auto;" />  

### 服务器设置

通过 `PC` 端可以访问此服务器, 服务器的 `IP` 地址和端口号为:

    IP: 169.254.100.138
    Port: 8080 - 10000

客户端从 `8080` 开始遍历寻找服务器即可.

## `FPGA` 读写测试工具 `FPGA-Tool`  

使用手册 [`FPGA-Tool`使用手册](./ug-fpga-tool.md)  
