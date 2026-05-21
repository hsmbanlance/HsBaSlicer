# Coroutine (协程)

Coroutine 组件提供了现代 C++ 协程和异步任务的支持，包括任务执行器、生成器和异步任务管理。

## 功能特点

- 支持 C++20 协程特性
- 提供多种任务执行器（NoopExecutor, AsyncExecutor）
- 支持自定义内存分配器的协程
- 生成器支持（Generator）
- 异步任务管理（Task）
- 线程安全的任务执行
- 链式回调支持（then, catching, finally）
- 异常处理和传播机制

## 使用方法

### 1. 基本任务 (Task)

```cpp
#include "base/coroutine.hpp"

// 创建一个返回 int 的异步任务
HsBa::Slicer::Utils::Task<int> async_task = []() -> HsBa::Slicer::Utils::Task<int> {
    co_return 42;
}();

// 等待任务完成并获取结果
int result = async_task.GetResult();  // 返回 42
```

### 2. 生成器 (Generator)

```cpp
#include "base/coroutine.hpp"

// 创建一个整数生成器
HsBa::Slicer::Utils::Generator<int> int_generator = []() -> HsBa::Slicer::Utils::Generator<int> {
    co_yield 1;
    co_yield 2;
    co_yield 3;
    co_return;  // 结束生成器
}();

// 遍历生成器的值
for (const auto& value : int_generator) {
    std::cout << value << std::endl;  // 输出 1, 2, 3
}
```

### 3. 任务执行器

组件提供了多种任务执行器来控制任务的执行方式：

```cpp
#include "base/coroutine.hpp"

// NoopExecutor: 直接在当前线程执行
auto noop_executor = std::make_shared<HsBa::Slicer::Utils::NoopExecutor>();

// AsyncExecutor: 使用 std::async 执行
auto async_executor = std::make_shared<HsBa::Slicer::Utils::AsyncExecutor>();

// 使用指定执行器的任务
HsBa::Slicer::Utils::Task<int, HsBa::Slicer::Utils::AsyncExecutor> task_with_executor = 
    []() -> HsBa::Slicer::Utils::Task<int, HsBa::Slicer::Utils::AsyncExecutor> {
        co_return 100;
    }();
```

### 4. 链式回调操作

Task 支持链式回调操作，提供更灵活的异步编程模式：

```cpp
#include "base/coroutine.hpp"

// 使用链式回调
auto task = []() -> HsBa::Slicer::Utils::Task<int> {
    co_return 42;
}();

// 成功回调
task.then([](int result) {
    std::cout << "Task completed with result: " << result << std::endl;
});

// 异常处理回调
task.catching([](std::exception& e) {
    std::cout << "Task failed with exception: " << e.what() << std::endl;
});

// 最终执行回调（无论成功或失败都会执行）
task.finally([]() {
    std::cout << "Task finished" << std::endl;
});
```

### 5. 自定义分配器协程

```cpp
#include "base/coroutine.hpp"

// 使用自定义分配器的任务
HsBa::Slicer::Utils::CustomAllocatorTask<int, HsBa::Slicer::Utils::AsyncExecutor, std::allocator<int>> custom_task = 
    []() -> HsBa::Slicer::Utils::CustomAllocatorTask<int, HsBa::Slicer::Utils::AsyncExecutor, std::allocator<int>> {
        co_return 100;
    }();

// 使用自定义分配器的生成器
HsBa::Slicer::Utils::CustomAllocatorGenerator<int, std::allocator<int>> custom_gen = 
    []() -> HsBa::Slicer::Utils::CustomAllocatorGenerator<int, std::allocator<int>> {
        co_yield 1;
        co_yield 2;
        co_return;
    }();

// 设置分配器（静态方法）
HsBa::Slicer::Utils::CustomAllocatorTask<int, HsBa::Slicer::Utils::AsyncExecutor, std::allocator<int>>::SetAllocator(
    std::allocator<int>{});
```

### 6. 协程链式调用

```cpp
#include "base/coroutine.hpp"

// 创建一个异步任务链
HsBa::Slicer::Utils::Task<int> chain_task = []() -> HsBa::Slicer::Utils::Task<int> {
    // 第一个异步操作
    int value = co_await []() -> HsBa::Slicer::Utils::Task<int> {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        co_return 10;
    }();
    
    // 第二个异步操作，使用第一个的结果
    value = co_await [value]() -> HsBa::Slicer::Utils::Task<int> {
        co_return value * 2;
    }();
    
    co_return value;  // 最终结果是 20
}();
```

### 7. GeneratorInvoke 工具函数

```cpp
#include "base/coroutine.hpp"

// 使用 GeneratorInvoke 转换容器
std::vector<int> numbers = {1, 2, 3, 4, 5};
auto square_generator = HsBa::Slicer::Utils::GeneratorInvoke<int, std::vector>(
    [](int i) { return i * i; }, 
    numbers
);

// 遍历生成的平方值
for (const auto& square : square_generator) {
    std::cout << square << " ";
}
// 输出: 1 4 9 16 25
```

### 8. 异常处理

```cpp
#include "base/coroutine.hpp"

// 生成器异常处理
HsBa::Slicer::Utils::Generator<int>::SetOnCancel([]() {
    std::cout << "Generator was canceled" << std::endl;
});

// 任务异常处理
auto task_with_exception = []() -> HsBa::Slicer::Utils::Task<int> {
    throw std::runtime_error("Something went wrong");
    co_return 42;
}();

try {
    int result = task_with_exception.GetResult();
} catch (const std::exception& e) {
    std::cout << "Caught exception: " << e.what() << std::endl;
}
```

### 9. 完整示例

```cpp
#include "base/coroutine.hpp"
#include <iostream>
#include <thread>

int main() {
    // 创建一个简单的异步任务
    auto simple_task = []() -> HsBa::Slicer::Utils::Task<int> {
        std::cout << "Task running in thread: " << std::this_thread::get_id() << std::endl;
        co_return 42;
    }();
    
    // 使用链式回调
    simple_task
        .then([](int result) {
            std::cout << "Task succeeded with result: " << result << std::endl;
        })
        .catching([](std::exception& e) {
            std::cout << "Task failed: " << e.what() << std::endl;
        })
        .finally([]() {
            std::cout << "Task completed" << std::endl;
        });
    
    // 创建一个生成器
    auto number_generator = []() -> HsBa::Slicer::Utils::Generator<int> {
        for (int i = 1; i <= 5; ++i) {
            co_yield i * i;  // 返回 i 的平方
        }
        co_return;
    }();
    
    // 使用生成器
    std::cout << "Generated squares:" << std::endl;
    for (const auto& num : number_generator) {
        std::cout << num << " ";
    }
    std::cout << std::endl;
    
    // 等待任务完成
    int result = simple_task.GetResult();
    std::cout << "Task result: " << result << std::endl;
    
    // 创建一个带执行器的任务
    auto task_with_executor = 
        []() -> HsBa::Slicer::Utils::Task<std::string, HsBa::Slicer::Utils::AsyncExecutor> {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            co_return "Hello from thread!";
        }();
    
    std::string msg = task_with_executor.GetResult();
    std::cout << msg << std::endl;
    
    return 0;
}
```

## 注意事项

- 需要编译器支持 C++20 协程特性
- 协程函数必须返回 Task 或 Generator 类型
- 使用 co_await 等待其他协程任务
- 使用 co_yield 从生成器产生值
- 使用 co_return 返回最终结果或结束生成器
- 任务执行器控制协程的执行上下文
- 协程对象需要在使用前完成初始化
- 所有协程任务（和协程生成器）应禁用异常或只抛出继承自 std::exception 的异常，否则不保证结果
- 链式回调方法（then, catching, finally）提供更灵活的异步编程模式
- 自定义分配器的协程需要通过静态方法 SetAllocator 进行配置