# LogCfg (日志配置文件)

LogCfg 是一个 INI 格式的日志配置文件，用于配置应用程序的日志系统参数。

## 功能特点

- INI 格式配置文件
- 支持日志级别配置
- 支持日志文件输出控制
- 支持日期时间格式配置

## 配置项说明

### [log] 部分
- `log_level` - 日志级别 (数字表示，数值越高日志越详细)
- `log_level_debug` - 调试日志级别
- `use_log_file` - 是否使用日志文件 (true/false)
- `log_file` - 日志文件路径

### [log_format] 部分
- `log_datatime_format` - 日期时间格式 (strftime 格式)
- `log_time_format` - 时间格式 (strftime 格式)

## 默认配置

```ini
[log]
log_level = 3
log_level_debug = 1
use_log_file = false
log_file = /log/log.txt
[log_format]
log_datatime_format=%Y-%m-%d %H:%M:%S
log_time_format=%H:%M:%S
```

## 配置示例

```ini
[log]
log_level = 5
log_level_debug = 1
use_log_file = true
log_file = ./logs/application.log
[log_format]
log_datatime_format=%Y/%m/%d %H:%M:%S
log_time_format=%H:%M:%S
```

## 使用方法

1. 将配置文件放置在应用程序可访问的位置
2. 应用程序启动时读取此配置文件
3. 根据配置参数初始化日志系统

## 注意事项

- 确保日志文件路径存在且应用程序有写入权限
- 根据需要调整日志级别，避免产生过多日志影响性能
- 日期时间格式遵循 strftime 标准格式
- use_log_file 设置为 true 时，需确保日志文件路径有效