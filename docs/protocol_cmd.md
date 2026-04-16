# VUPRS 网络命令格式

## Data Header & Data Tailer

```bash
data header: [HEADER],
data tailer: [TAILER]
```

## Certain commands

- 客户端数据格式采用 `[HEADER]``{json}``[TAILER]` 格式, 推荐在发送时去除空格和换行.  
- 服务端协议解析函数只处理去除 `[HEADER]` 和 `[TAILER]` 后的JSON字符串.  
- 服务端响应构造函数返回的也是纯JSON字符串, 由会话层统一拼接 `[HEADER]` 和 `[TAILER]`.    

### 1. ACK

*client to server*  

```json
[HEADER]
{
    "cmd": "ack",
    "params": {}
}
[TAILER]
```

*server response*  

```json
[HEADER]
{
    "response_cmd": "ack",
    "operation_status": "done"
}
[TAILER]
```

### 2. Reset (or Restart)

*client to server*  

```json
[HEADER]
{
    "cmd": "reset",
    "params": {}
}
[TAILER]
```

*server response*  

```json
[HEADER]
{
    "response_cmd": "reset",
    "operation_status": "done"
}
[TAILER]
```

### 3. Change beamformer

*client to server*  

```json
[HEADER]
{
    "cmd": "change_beam_former",
    "params": {
        "beamformer": "mvdr"
    }
}
[TAILER]
```

*server response*  

```json
[HEADER]
{
    "response_cmd": "change_beam_former",
    "operation_status": "done"
}
[TAILER]
```

可选值: `mvdr`, `cbf`, `dcrcb` (后期可拓展).  

### 4. Redirect

*client to server*  

```json
[HEADER]
{
    "cmd": "redirect",
    "params": {
        "alt": "44.32",
        "az": "-121.34"
    }
}
[TAILER]
```

*server response*  

```json
[HEADER]
{
    "response_cmd": "redirect",
    "operation_status": "done"
}
[TAILER]
```

*注: `alt` 和 `az` 的单位是 `degree`, 必须以浮点数字符串形式发送*.  

### 5. Start

*client to server*  

```json
[HEADER]
{
    "cmd": "start",
    "params": {}
}
[TAILER]
```

*server response*  

```json
[HEADER]
{
    "response_cmd": "start",
    "operation_status": "done"
}
[TAILER]
```

### 6. Stop

*client to server*  

```json
[HEADER]
{
    "cmd": "stop",
    "params": {}
}
[TAILER]
```

*server response*  

```json
[HEADER]
{
    "response_cmd": "stop",
    "operation_status": "done"
}
[TAILER]
```

### 7. Get data (beam forming result)

*client to server*  

```json
[HEADER]
{
    "cmd": "get_data",
    "params": {}
}
[TAILER]
```

*server response*  

首先会发送一条配置细节.  

```json
[HEADER]
{
    "response_cmd": "get_data",
    "operation_status": "done",
    "params": {
        "data_format": "uint32_t"
    }
}
[TAILER]
```

然后发送数据:  

```json
[HEADER]data[TAILER]
```

说明: `get_data`是特殊指令, 控制通道不返回JSON状态包, 服务端直接发送数据帧:
```text
[HEADER] + binary(double array) + [TAILER]
```
`binary` 中, 前 `4` 字节描述了该数据的大小 (单位是 `byte`), 然后紧接着是数据.  

### 8. Change Algorithm Parameters

*client to server*  

```json
[HEADER]
{
    "cmd": "change_algorithm_parameters",
    "params": {
        "fs": "40000.0",
        "wave_velocity": "346.0", 
        "lower_frequency": "100.0",
        "upper_frequency": "4000.0",
        "snapshot_window_size": "100",
        "covariance_average_index": "0.8"
    }
}
[TAILER]
```

*server response*  

```json
[HEADER]
{
    "response_cmd": "change_algorithm_parameters",
    "operation_status": "done"
}
[TAILER]
```

*注: 必须以浮点数字符串形式发送*.  
*注: 可修改参数均按照上述所示.*.  
可以只发送一部分, 代表只修改当前参数, 其他参数与当前一致, 例如:  
```json
[HEADER]
{
    "cmd": "change_algorithm_parameters",
    "params": {
        "fs": "40000.0",
    }
}
[TAILER]
```
代表只修改采样频率, 其他参数保证现状.  

### 9. Read Current Algorithm Parameters

*client to server*  

```json
[HEADER]
{
    "cmd": "read_algorithm_parameters",
    "params": {}
}
[TAILER]
```

*server response*  

```json
[HEADER]
{
    "response_cmd": "read_algorithm_parameters",
    "operation_status": "done",
    "params": {
        "fs": "40000.0",
        "wave_velocity": "346.0", 
        "lower_frequency": "100.0",
        "upper_frequency": "4000.0",
        "snapshot_window_size": "100",
        "covariance_average_index": "0.8"
    }
}
[TAILER]
```

### 10. Power Scan

*client to server*  

```json
[HEADER]
{
    "cmd": "power_scan",
    "params": {
        "points": "70",
        "min_alt": "15.0"
    }
}
[TAILER]
```

*server response*  

首先会发送一条配置细节.  

```json
[HEADER]
{
    "response_cmd": "power_scan",
    "operation_status": "done",
    "params": {
        "points": "70",
        "min_alt": "15.0",
        "max_power": "12.04046",
        "min_power": "-15.00231",
        "data_format": "uint16_t"
    }
}
[TAILER]
```
然后会发送数据: 
```text
[HEADER] + binary(double array) + [TAILER]
```
`binary` 中, 前 `4` 字节描述了该数据的大小 (单位是 `byte`), 然后紧接着是数据.  
