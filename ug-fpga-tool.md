# *Usage of FPGA Controller Command Tool*

## Command keys

| Command | Description | Choices |
| :--- | :--- | :--- |
| `-p` | operation | `r`: read, `w`: write, `l`: list all registers |
| `-m` | module name | In device reading/writing - `adc`: ADC Controller, `dma`: AXI DMA, `cbuf`: Circular Buffer, `fir`: FIR Filter Bank, `pdly`: Pre-delay Unit.  In memory reading/writing - `ddr`: DDR RAM, `fir-bram`: FIR BRAM, `sg-bram`: SG BRAM, `cbuf-bram`: Circular Buffer BRAM |
| `-f` | offset | address offset (register offset in devices/memory offset in memories). Note: must be 4 bytes alignment |
| `-v` | value | value to write. Note: unsigned int 32 bits |
| `-s` | transfer size | Note unsigned int 32 bits |
| `-i`, `-o` | input/output file name | a valid file name |

# Usage 1 - Read & Write device register

### *Read registers of `ADC Controller`*

register offset = `0x04`  
module name = `adc`  

```bash
controller -p r -m adc -f 0x04  # argc = 7
```

### *List all register of `DMA`*

module name = `dma`  

```bash
controller -p l -m dma  # argc = 5
controller -p r -m dma  # argc = 5
```

### *Write value to registers of `ADC Controller`*

register offset = `0x0c`  
module name = `adc`  
write value = `0x00`  

```bash
controller -p w -m adc -f 0x0c -v 0x00  # argc = 9
```

# Usage 2 - Read & Write word in memory

### *Read word from DDR*

memory offset = `0x0c`  
module name = `ddr`  

```bash
controller -p r -m ddr -f 0x0c  # argc = 7
```

### *Write word to DDR*

memory offset = `0x0c`  
module name = `ddr`  
write value = `0xFF`  

```bash
controller -p w -m ddr -f 0x0c -v 0xFF  # argc = 9
```

# Usage 3 - Read & Write data in memory

### *Read data from memory to file*

memory offset = `0x00`  
module name = `sg_bram`  
transfer size = `65536`  
output file = `r_data.bin`  

```bash
controller -p r -m sg_bram -f 0x00 -s 65536 -o r_data.bin  # argc = 11
```

if `memory offset = 0`, then `-f` can be ignored:  

```bash
controller -p r -m sg_bram -s 65536 -o r_data.bin  # argc = 9
```

### *Write file data to memory*

memory offset = `0x04`  
module name = `sg_bram`  
transfer size = `1024`  
input file = `w_data.bin`  

```bash
controller -p w -m sg_bram -f 0x04 -s 1024 -i w_data.bin  # argc = 11
```

if `memory offset = 0`, then `-f` can be ignored.  

```bash
controller -p w -m sg_bram -s 1024 -i w_data.bin  # argc = 9
```

if `memory offset = 0` and `transfer size = size of write data file`, then `-f` and `-s` can be ignored:  

```bash
controller -p w -m sg_bram -i w_data.bin  # argc = 7
```
