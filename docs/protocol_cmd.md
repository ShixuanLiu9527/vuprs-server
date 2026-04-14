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

### Reset (or Restart)

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

### Change beamformer

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

### Redirect

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

*注: `alt` 和 `az` 的单位是 `degree`*.  

### Start

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

### Stop

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

### Get data (beam forming result)

```json
[HEADER]
{
    "cmd": "get_data",
    "params": {}
}
[TAILER]
```

*server response*  

```json
[HEADER]data[TAILER]
```

说明: `get_data`是特殊指令, 控制通道不返回JSON状态包, 服务端直接发送数据帧:

```text
[HEADER] + binary(double array) + [TAILER]
```
