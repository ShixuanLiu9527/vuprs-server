# VUPRS 网络命令格式

## 1. Data Header & Data Tailer

无论数据类型, 传输的命令/数据都是具有数据头和数据尾部的, 如下所示.  

```json
data header: "[HEADER]"
data tailer: "[TAILER]"
```

## 2. Certain commands

- 客户端数据格式采用 `[HEADER]``{json}``[TAILER]` 格式, 推荐在发送时去除空格和换行.  
- 服务端协议解析函数只处理去除 `[HEADER]` 和 `[TAILER]` 后的JSON字符串.  
- 服务端响应构造函数返回的也是纯JSON字符串, 由会话层统一拼接 `[HEADER]` 和 `[TAILER]`.  

接下来的协议描述都没有数据头部和数据尾部, 但是在实际传输时必须添加数据头和数据尾.  

### 2.1 服务器应答指令
| Command | description | client to server | server response |
| :--- | :--- | :--- | :--- |
| Ack | 用于客户端与主机端的应答测试 | {"cmd":"ack","params":{}} | {"response_cmd":"ack","operation_status":"done"} |

### 2.2 波束形成算法指令
| Command | description | client to server | server response |
| :--- | :--- | :--- | :--- |
| Change beamformer | 修改波束形成算法 | {"cmd":"change_beam_former","params":{"beamformer":"mvdr"}} | {"response_cmd":"change_beam_former","operation_status":"done"} |

### 2.3 波束指向指令
| Command | description | client to server | server response |
| :--- | :--- | :--- | :--- |
| Get direction | 得到当前波束形成指向方位 | {"cmd":"get_direction","params":{}} | {"response_cmd":"direction","operation_status":"done","params":{"alt":"15.0","az":0.0}} |
| Redirect | 修改当前波束形成指向位置 | {"cmd":"redirect","params":{"alt":"44.32","az":"-121.34"}} | {"response_cmd":"redirect","operation_status":"done"} |

*Note: "alt" 和 "az" 是相对于麦克风阵列的.*  

### 2.4 波束形成器使能控制指令
| Command | description | client to server | server response |
| :--- | :--- | :--- | :--- |
| Start beamformer | 开启波束形成器 | {"cmd":"start_beamformer","params":{}} | {"response_cmd":"start_beamformer","operation_status":"done"} |
| Stop beamformer | 关闭波束形成器 | {"cmd":"stop_beamformer","params":{}} | {"response_cmd":"stop","operation_status":"done"} |
| Restart beamformer | 重启波束形成器 | {"cmd":"restart_beamformer","params":{}} | {"response_cmd":"restart_beamformer","operation_status":"done"} |

### 2.5 算法参数控制指令
| Command | description | client to server | server response |
| :--- | :--- | :--- | :--- |
| Change algorithm parameters | 修改算法参数 | {"cmd": "change_algorithm_parameters","params":{"fs": "40000.0","wave_velocity": "346.0","lower_frequency": "100.0","upper_frequency": "4000.0","snapshot_window_size": "100","covariance_average_index": "0.8"}} | {"response_cmd":"change_algorithm_parameters","operation_status":"done"} |
| Read algorithm parameters | 获取当前算法参数 | {"cmd": "read_algorithm_parameters","params":{}} | {"response_cmd":"read_algorithm_parameters","operation_status":"done","params":{"fs": "40000.0","wave_velocity": "346.0","lower_frequency": "100.0","upper_frequency":"4000.0","snapshot_window_size":"100","covariance_average_index":"0.8"}} |

*Note 1: 使用 Change algorithm parameters 时参数可以不给全, 表示只修改当前参数.*  
*Note 2: 参数意义如下:*  
| 名称 | 意义 | 典型值 | 单位 |
| :--- | :--- | :--- | :--- |
| `fs` | ADC 采样频率 | 10000.0 | Hz |
| `wave_velocity` | 音速 | 346.0 | m/s |
| `lower_frequency` | 波束形成器通带最低频率 | 100.0 | Hz |
| `upper_frequency` | 波束形成器通带最高频率 | 4000.0 | Hz |
| `snapshot_window_size` | 协方差矩阵快照窗口大小 | 100 | 1 |
| `covariance_average_index` | 协方差矩阵拟合系数 | 0.8 | 1 |


### 2.6 获取波束形成输出
| Command | description | client to server | server response |
| :--- | :--- | :--- | :--- |
| Get data | 获取波束形成器输出 | {"cmd":"get_data","params":{}} | `response 1st`: {"response_cmd":"get_data","operation_status":"done","params":{"data_format": "uint32_t"}}<br>`response 2nd`: binary data |

### 2.7 获取空间能量扫描数据
| Command | description | client to server | server response |
| :--- | :--- | :--- | :--- |
| Scan control | 开启/关闭空间能量扫描 | {"cmd":"scan_control","params":{"status":"on"}} | {"response_cmd":"scan_control","operation_status":"done", "params": {"status": "on"}} |
| Get scan data | 获取空间能量扫描结果 | {"cmd":"power_scan","params":{"points":"70","alt_min":"15.0"}} | `response 1st`: {"response_cmd":"power_scan","operation_status":"done","params":{"points":"70","alt_min":"15.0","max_power":"12.04046","min_power": "-15.00231","data_format":"uint16_t"}}<br>`response 2nd`: binary data |

*Note 1: "alt_min" 代表扫描的最低高度角.*  
*Note 2: "points" 代表半球上斐波那契点的一半, 客户端和服务端必须使用相同的扫描点生成函数, 否则数据无法对齐.*  
函数如下:  
```cpp
void FibonacciGrid(int points, std::vector<double> *alt, std::vector<double> *az, double alt_min = 15.0)
{
    double phi = (std::sqrt(5.0) - 1.0) / 2.0;  /* Golden ratio */
    double xn, yn, zn, _alt, _az;
    int N = 2 * points + 1, n;
    int start_n = (N + 1) / 2;  /* Number of points in upper hemisphere */
    int resultSize = N - start_n + 1;
    Eigen::Matrix<double, 3, 1> vec;
    alt->clear();
    az->clear();
    alt->reserve(resultSize);
    az->reserve(resultSize);
    for (int i = 0; i < resultSize; i++)
    {
        n = start_n + i;
        zn = 2.0 * (double)n / (double)N - 1.0;
        xn = std::sqrt(1.0 - zn * zn) * std::cos(2.0 * PI * (double)n * phi);
        yn = std::sqrt(1.0 - zn * zn) * std::sin(2.0 * PI * (double)n * phi);
        vec << xn, yn, zn;
        vuprs::PointingVector2AltAz(vec, &_alt, &_az);
        if (_alt < alt_min) continue;
        alt->push_back(_alt);
        az->push_back(_az);
    }
}
```
