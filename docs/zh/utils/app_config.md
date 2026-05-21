# AppConfig (应用程序配置)

AppConfig 组件提供了一个线程安全的单例模式实现，用于管理应用程序的配置信息。

## 功能特点

- 线程安全的单例模式实现
- 使用互斥锁确保线程安全
- 提供获取 SevenZip 路径的方法

## 使用方法

### 1. 获取实例

```cpp
#include "utils/app_config.hpp"

// 获取 AppConfig 单例实例
auto& config = HsBa::Slicer::AppConfigSingletone::GetInstance();
```

### 2. 使用配置功能

```cpp
// 获取 SevenZip 路径
std::string sevenZPath = config.GetSevenZPath();
```

### 3. 清理实例

```cpp
// 当应用程序退出时清理实例
HsBa::Slicer::AppConfigSingletone::DeleteInstance();
```

### 4. 完整示例

```cpp
#include "utils/app_config.hpp"
#include <iostream>

int main() {
    // 获取配置实例
    auto& config = HsBa::Slicer::AppConfigSingletone::GetInstance();
    
    // 使用配置功能
    std::string sevenZPath = config.GetSevenZPath();
    std::cout << "SevenZ Path: " << sevenZPath << std::endl;
    
    // 应用程序结束时清理
    HsBa::Slicer::AppConfigSingletone::DeleteInstance();
    
    return 0;
}
```

## 注意事项

- AppConfigSingletone 是一个单例类，始终通过 GetInstance() 获取实例
- 使用完后应调用 DeleteInstance() 清理资源
- 所有方法都是线程安全的，内部使用 shared_mutex 进行同步
- 当前仅提供获取 SevenZip 路径的功能，可根据需要扩展