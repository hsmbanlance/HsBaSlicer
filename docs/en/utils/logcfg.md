# LogCfg (Log Configuration File)

LogCfg is an INI format log configuration file used to configure application log system parameters.

## Features

- INI format configuration file
- Supports log level configuration
- Supports log file output control
- Supports date/time format configuration

## Configuration Options

### [log] Section
- `log_level` - Log level (numeric value, higher numbers mean more detailed logs)
- `log_level_debug` - Debug log level
- `use_log_file` - Whether to use log file (true/false)
- `log_file` - Log file path

### [log_format] Section
- `log_datatime_format` - Date/time format (strftime format)
- `log_time_format` - Time format (strftime format)

## Default Configuration

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

## Configuration Example

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

## Usage

1. Place the configuration file in a location accessible to the application
2. The application reads this configuration file on startup
3. Initialize the log system based on the configuration parameters

## Notes

- Ensure the log file path exists and the application has write permissions
- Adjust log levels as needed to avoid generating excessive logs that impact performance
- Date/time formats follow the strftime standard format
- When use_log_file is set to true, ensure the log file path is valid