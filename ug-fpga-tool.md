# `FPGA-Tool` 使用手册

## `Build`

    sudo mkdir build
    cd build
    sudo cmake .. -DCMAKE_TOOLCHAIN_FILE=../rk3568_toolchain.cmake
    sudo make

## `Command`
  
`FPGA-Tool` 提供 `AXI-Lite` 和 `AXI-Full` 总线的快速测试接口, 命令列表如下:  
  
1. `--rw`: 指定读写方向, `--rw r` 表示从 `FPGA` 读取数据, `--rw w` 表示向 `FPGA` 写入数据;  
2. `--bus`: 指定目标总线, `--bus lite` 表示访问 `AXI-Lite` 总线, `--bus full` 表示访问 `AXI-Full` 总线;  
3. `--cfg`: 指定配置 `JSON` 文件, 例如 `--cfg ./cfg.json`. 配置文件的格式参考模板: [fpga_config_template.json](./fpga_config_template.json);  
4. `--base`: (访问 `AXI-Lite` 时使用) 指定访问的基地址, `--base 0` 表示访问 `DMA` 控制器地址空间, `--base 0x10000` 表示访问 `ADC` 控制器地址空间;  
5. `--offset`: 访问 `AXI-Lite` 时, 是指定访问的地址偏移, 访问 `AXI-Full` 时, 是指定 `DDR` 中的偏移地址;  
6. `--bytes`: (访问 `AXI-Full` 时使用) 指定传输的数据字节数量;  
7. `--io`: 传输的数据, 当写 `AXI-Lite` 时, 该参数就是需要写入的数据, 当读写 `AXI-Full` 时, 该参数就是需要读出/写入的二进制文件名称;  
  
## `Example`

### 获取帮助信息

使用 `-h` 或者 `--help` 参数即可打印帮助信息.  

    ./fpga_tool -h
    ./fpga_tool --help

### 访问 `AXI-Lite` 总线

读 `AXI-Lite` 总线上的寄存器 (`32` 位地址为 `0x0000_0004`):  

    ./fpga_tool --rw r --bus lite --cfg ./fpga_config.json --base 0 --offset 0x04

写 `AXI-Lite` 总线上的寄存器 (`32` 位地址为 `0x0001_000C`), 写入的数据为 `0x1234`:  

    ./fpga_tool --rw w --bus lite --cfg ./fpga_config.json --base 0x10000 --offset 0x0C --io 0x1234

### 访问 `AXI-Full` 总线

读 `AXI-Full` 总线 `DDR` 的数据, 读取的 `32` 位起始地址为 `0x0000_0008`, 读取 `65536` 字节:  

    ./fpga_tool --rw r --bus lite --cfg ./fpga_config.json --offset 0x08 --bytes 65536 --io ./read_data.bin

写 `AXI-Full` 总线 `DDR` 的数据, 写入的 `32` 位起始地址为 `0x0000_0008`, 写入 `2048` 字节:  

    ./fpga_tool --rw w --bus lite --cfg ./fpga_config.json --offset 0x08 --bytes 2048 --io ./read_data.bin

