# Singleton (单例模式)

Singleton 组件提供了一个线程安全的单例模式实现，使用现代 C++ 特性确保实例的唯一性和线程安全。

## 功能特点

- 线程安全的单例模式实现
- 使用 std::call_once 确保实例只创建一次
- 使用 std::shared_ptr 管理对象生命周期
- 支持带参数的构造函数

## 使用方法

### 1. 定义单例类

```cpp
#include "base/singleton.hpp"

class MyService
{
public:
    // 需要继承自 Singleton<T> 并使用 Protected 构造函数
    struct Protected {};
    
    MyService(Protected, int value) : data_(value) {}
    
    void DoSomething() {
        // 业务逻辑
    }
    
    int GetData() const { return data_; }

private:
    int data_;
    friend class HsBa::Slicer::Utils::Singleton<MyService>; // 友元访问权限
};
```

### 2. 获取单例实例

```cpp
// 获取单例实例
auto instance = HsBa::Slicer::Utils::Singleton<MyService>::GetInstance(42);
auto same_instance = HsBa::Slicer::Utils::Singleton<MyService>::GetInstance(42);

// 两个实例是相同的
assert(instance == same_instance);
```

### 3. 完整示例

```cpp
#include "base/singleton.hpp"
#include <iostream>

class Logger
{
public:
    struct Protected {};
    
    Logger(Protected, const std::string& name) : name_(name) {}
    
    void Log(const std::string& message) {
        std::cout << "[" << name_ << "] " << message << std::endl;
    }

private:
    std::string name_;
    friend class HsBa::Slicer::Utils::Singleton<Logger>;
};

// 使用示例
int main() {
    auto logger1 = HsBa::Slicer::Utils::Singleton<Logger>::GetInstance("App");
    auto logger2 = HsBa::Slicer::Utils::Singleton<Logger>::GetInstance("App");
    
    logger1->Log("Hello World");
    logger2->Log("Same instance");
    
    // 两个实例是同一个对象
    assert(logger1 == logger2);
    
    return 0;
}
```

## 注意事项

- 单例类需要定义一个 `Protected` 结构体
- 单例类需要将 `Singleton<T>` 声明为友元类
- 构造函数需要接受 `Protected` 作为第一个参数
- 实例通过 `GetInstance` 方法获取，支持传递构造函数参数