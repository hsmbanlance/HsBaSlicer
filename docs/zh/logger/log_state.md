# LogState (日志状态类)

LogState 类提供了自定义字面量操作符的辅助功能，简化日志记录过程。

## 功能特点

- 提供自定义字面量操作符
- 支持简洁的日志记录语法
- 与LoggerSingletone配合使用
- 自动包含源代码位置信息

## 方法说明

### 自定义字面量
- `operator""_log_debug` - 调试日志字面量操作符
- `operator""_log_info` - 信息日志字面量操作符
- `operator""_log_warning` - 警告日志字面量操作符
- `operator""_log_error` - 错误日志字面量操作符

## 使用示例

```cpp
#include "logger/logger.hpp"

using namespace HsBa::Slicer::Log;

// 使用自定义字面量记录日志（更简洁的方式）
"Application started"_log_info;
"Configuration loaded"_log_debug;
"Invalid parameter detected"_log_warning;
"Failed to connect to database"_log_error;
```

## 注意事项

- 自定义字面量提供了一种更简洁的日志记录方式
- 使用时会自动包含源代码位置信息
- 仍依赖LoggerSingletone进行实际的日志记录
- 需要C++11或更高版本支持自定义字面量