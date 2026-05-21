# Delegate (委托)

Delegate 组件实现了一个类型安全的委托/事件系统，支持多播委托和线程安全的回调管理。

## 功能特点

- 类型安全的委托系统
- 支持多播委托（多个回调函数）
- 线程安全的回调管理
- 支持任意可调用对象
- 支持 void 和非 void 返回类型

## 使用方法

### 1. 基本用法

```cpp
#include "base/delegate.hpp"

// 创建一个返回void，接受两个int参数的委托
HsBa::Slicer::Utils::Delegate<void, int, int> my_delegate;

// 添加回调函数
my_delegate += [](int a, int b) {
    std::cout << "Sum: " << a + b << std::endl;
};

my_delegate += [](int a, int b) {
    std::cout << "Product: " << a * b << std::endl;
};

// 调用所有回调函数
my_delegate(3, 4);  // 输出: Sum: 7 和 Product: 12
```

### 2. 返回值处理

对于非 void 返回类型的委托，处理方式取决于返回类型：

```cpp
// 对于可加类型的返回值（如 int, double 等）
HsBa::Slicer::Utils::Delegate<int, int> add_delegate;

add_delegate += [](int x) { return x; };
add_delegate += [](int x) { return x * 2; };
add_delegate += [](int x) { return x * 3; };

int result = add_delegate(5);  // 结果是 5 + 10 + 15 = 30

// 对于非可加类型的返回值，最后一个回调的返回值会被返回
HsBa::Slicer::Utils::Delegate<std::string, std::string> string_delegate;

string_delegate += [](const std::string& s) { return "First: " + s; };
string_delegate += [](const std::string& s) { return "Second: " + s; };

std::string result2 = string_delegate("Hello");  // 结果是 "Second: Hello"
```

### 3. 事件系统

Delegate 组件还提供了 Event 和 EventSource，用于实现事件驱动的编程模式：

```cpp
#include "base/delegate.hpp"

// 继承 EventSource 创建支持事件的类
class Button : public HsBa::Slicer::Utils::EventSource<Button, void, int> {
public:
    void Click(int click_count) {
        // 触发事件
        RaiseEvent(click_count);
    }
};

// 使用事件
Button button;
button += [](int clicks) {
    std::cout << "Button clicked " << clicks << " times" << std::endl;
};

button.Click(3);  // 输出: Button clicked 3 times
```

### 4. 完整示例

```cpp
#include "base/delegate.hpp"
#include <iostream>
#include <string>

int main() {
    // 创建一个返回 int，接受两个 double 参数的委托
    HsBa::Slicer::Utils::Delegate<int, double, double> calc_delegate;
    
    // 添加不同的回调函数
    calc_delegate.Add([](double a, double b) -> int {
        return static_cast<int>(a + b);
    });
    
    calc_delegate.Add([](double a, double b) -> int {
        return static_cast<int>(a * b);
    });
    
    // 调用委托（对于可加返回类型，结果会被累加）
    int result = calc_delegate(3.5, 2.0);  // 5 + 7 = 12
    std::cout << "Result: " << result << std::endl;
    
    // 使用 void 委托
    HsBa::Slicer::Utils::Delegate<void, std::string> log_delegate;
    
    log_delegate += [](const std::string& msg) {
        std::cout << "[INFO] " << msg << std::endl;
    };
    
    log_delegate += [](const std::string& msg) {
        std::cout << "[DEBUG] " << msg << std::endl;
    };
    
    log_delegate("Application started");
    
    // 检查委托状态
    std::cout << "Delegate size: " << log_delegate.size() << std::endl;
    std::cout << "Is empty: " << log_delegate.empty() << std::endl;
    
    return 0;
}
```

## 注意事项

- 委托系统是线程安全的，但回调函数本身需要保证线程安全
- 对于可加类型的返回值，所有回调的结果会被累加
- 对于非可加类型的返回值，只有最后一个回调的结果会被返回
- 移除回调功能已被标记为弃用，因为 std::function 没有 operator== 操作符
- 委托不持有回调函数中捕获对象的生命周期，需要确保回调函数有效

### 进一步说明

#### 关于委托反注册的特殊用法

理论上，如果为每个注册的委托使用不同类型的非匿名仿函数（Functor），则可以正常进行同类型委托的反注册操作。

```cpp
class Button : public HsBa::Slicer::Utils::EventSource<Button, void, int> {
public:
    void Click(int click_count) {
        // 触发事件
        RaiseEvent(click_count);
    }
};

struct AClickCallback{
    void operator()(int click_count) {
        std::cout << "A: Click with " << click_count << "\n";
    }
};

struct BClickCallback{
    void operator()(int click_count) {
        std::cout << "B: Click with " << click_count << "\n";
    }
};

// 使用事件
Button button;
button += AClickCallback{};
button += BClickCallback{};

button.Click(3);  // 输出: A: Click with 3\n B: Click with 3

button -= BClickCallback{};  // 反册 BClickCallback 类型的回调

button.Click(2);  // 输出: A: Click with 2
```

#### 推荐做法

一般情况下不推荐使用上述特殊方法，即使可以通过 NameType 等方案增强可读性。原因是这种做法与函数指针（或函数名）以及 Lambda 表达式的使用方式不一致。委托/事件的 C++ 实现在统一处理这三种可调用对象时存在困难。

由于本库主要面向切片引擎等非 UI 场景，通常使用不带反注册功能的委托方案即可满足需求。

#### 使用场景说明

Delegate 组件一般只用于简单事件回调，如触发时向终端、UI、日志系统等选择性触发输出，此时反注册有多、漏等情况影响往往不严重，但依然不建议。如果参考进一步说明使用了，请自行负责和清除对应的编译警告。