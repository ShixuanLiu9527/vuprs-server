# VUPRS Server - 车下故障声学定位与识别系统 `Linux` 服务器.

<div align="center">
  <img src="./docs/server_structure.jpg" alt="Server" style="width:800px; height:auto;" />
</div>

[[ 回到主仓库: vuprs-support ]](https://github.com/ShixuanLiu9527/vuprs-support.git)

## 网络通信协议

[网络通信协议V1.0](./PROTOCOL.md)

## Build

在项目根目录输入以下指令:  

    sudo mkdir build
    cd build
    sudo cmake .. -DCMAKE_TOOLCHAIN_FILE=../rk3568_toolchain.cmake
    sudo make

## Usage

### 相关配置文件

为了方便系统运行和调整, 设置了 `2` 个配置文件, 分别为: `fpga_config.json` 和 `beam_forming_array.json`.  
  
[[ 查看 `fpga_config.json` 模板 ]](./fpga_config_template.json)  
[[ 查看 `beam_forming_array.json` 模板 ]](./beam_forming_array_template.json)  
  
`fpga_config.json` 用于配置 `FPGA` 侧各个模块, 方便 `ARM` 端与 `FPGA` 通信.  
`beam_forming_array.json` 用于配置麦克风阵列, 通过配置该文件, 系统可以适配不同的麦克风阵列.  

## FPGA 读写测试工具 `FPGA-Tool`  

为了方便调试, 设计了 `FPGA` 调试工具, 可以通过终端手动控制 `FPGA`.  
  
[[ `FPGA-Tool`使用手册 ]](./ug-fpga-tool.md)  
