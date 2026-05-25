# LoggerSingletone (单例日志类)

LoggerSingletone 类采用单例模式实现的核心日志记录器，提供线程安全的日志记录功能。

## 功能特点

- 采用单例模式确保全局唯一实例
- 提供多级别的日志记录（调试、信息、警告、错误）
- 支持源代码位置追踪
- 线程安全的实现
- 支持日志文件输出控制

## 方法说明

### 实例获取
- `GetInstance()` - 获取日志记录器单例实例
- `CreateInstance()` - 创建单例实例（内部使用）

### 日志记录功能
- `Log()` - 通用日志记录函数，支持指定日志级别
- `LogDebug()` - 记录调试级别日志
- `LogInfo()` - 记录信息级别日志
- `LogWarning()` - 记录警告级别日志
- `LogError()` - 记录错误级别日志

### 配置属性
- `UseLogFile()` - 查询是否启用日志文件输出

## 使用示例

```cpp
#include "logger/logger.hpp"

using namespace HsBa::Slicer::Log;

// 获取日志记录器实例
auto logger = LoggerSingletone::GetInstance();

// 使用常规方法记录日志
LoggerSingletone::LogDebug("这是调试信息");
LoggerSingletone::LogInfo("这是普通信息");
LoggerSingletone::LogWarning("这是警告信息");
LoggerSingletone::LogError("这是错误信息");
```

## 注意事项

- 日志记录器使用单例模式，确保全局只有一个实例
- 在多线程环境中使用时，内部采用共享互斥锁保证线程安全
- 使用源代码位置追踪功能需要C++20支持
- 合理设置日志级别以平衡调试信息和性能