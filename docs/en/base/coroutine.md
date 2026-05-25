# Coroutine

The Coroutine component provides support for modern C++ coroutines and asynchronous tasks, including task executors, generators, and asynchronous task management.

## Features

- Supports C++20 coroutine features
- Provides multiple task executors (NoopExecutor, AsyncExecutor)
- Supports coroutines with custom memory allocators
- Generator support (Generator)
- Asynchronous task management (Task)
- Thread-safe task execution
- Chained callback support (then, catching, finally)
- Exception handling and propagation mechanisms

## Usage

### 1. Basic Task

```cpp
#include "base/coroutine.hpp"

// Create an asynchronous task that returns int
HsBa::Slicer::Utils::Task<int> async_task = []() -> HsBa::Slicer::Utils::Task<int> {
    co_return 42;
}();

// Wait for task completion and get result
int result = async_task.GetResult();  // Returns 42
```

### 2. Generator

```cpp
#include "base/coroutine.hpp"

// Create an integer generator
HsBa::Slicer::Utils::Generator<int> int_generator = []() -> HsBa::Slicer::Utils::Generator<int> {
    co_yield 1;
    co_yield 2;
    co_yield 3;
    co_return;  // End generator
}();

// Iterate through generator values
for (const auto& value : int_generator) {
    std::cout << value << std::endl;  // Output: 1, 2, 3
}
```

### 3. Task Executors

The component provides multiple task executors to control how tasks are executed:

```cpp
#include "base/coroutine.hpp"

// NoopExecutor: Execute directly in current thread
auto noop_executor = std::make_shared<HsBa::Slicer::Utils::NoopExecutor>();

// AsyncExecutor: Execute using std::async
auto async_executor = std::make_shared<HsBa::Slicer::Utils::AsyncExecutor>();

// Task with specified executor
HsBa::Slicer::Utils::Task<int, HsBa::Slicer::Utils::AsyncExecutor> task_with_executor = 
    []() -> HsBa::Slicer::Utils::Task<int, HsBa::Slicer::Utils::AsyncExecutor> {
        co_return 100;
    }();
```

### 4. Chained Callback Operations

Task supports chained callback operations, providing more flexible asynchronous programming patterns:

```cpp
#include "base/coroutine.hpp"

// Using chained callbacks
auto task = []() -> HsBa::Slicer::Utils::Task<int> {
    co_return 42;
}();

// Success callback
task.then([](int result) {
    std::cout << "Task completed with result: " << result << std::endl;
});

// Exception handling callback
task.catching([](std::exception& e) {
    std::cout << "Task failed with exception: " << e.what() << std::endl;
});

// Finally callback (executed regardless of success or failure)
task.finally([]() {
    std::cout << "Task finished" << std::endl;
});
```

### 5. Custom Allocator Coroutines

```cpp
#include "base/coroutine.hpp"

// Task with custom allocator
HsBa::Slicer::Utils::CustomAllocatorTask<int, HsBa::Slicer::Utils::AsyncExecutor, std::allocator<int>> custom_task = 
    []() -> HsBa::Slicer::Utils::CustomAllocatorTask<int, HsBa::Slicer::Utils::AsyncExecutor, std::allocator<int>> {
        co_return 100;
    }();

// Generator with custom allocator
HsBa::Slicer::Utils::CustomAllocatorGenerator<int, std::allocator<int>> custom_gen = 
    []() -> HsBa::Slicer::Utils::CustomAllocatorGenerator<int, std::allocator<int>> {
        co_yield 1;
        co_yield 2;
        co_return;
    }();

// Set allocator (static method)
HsBa::Slicer::Utils::CustomAllocatorTask<int, HsBa::Slicer::Utils::AsyncExecutor, std::allocator<int>>::SetAllocator(
    std::allocator<int>{});
```

### 6. Coroutine Chaining

```cpp
#include "base/coroutine.hpp"

// Create an asynchronous task chain
HsBa::Slicer::Utils::Task<int> chain_task = []() -> HsBa::Slicer::Utils::Task<int> {
    // First asynchronous operation
    int value = co_await []() -> HsBa::Slicer::Utils::Task<int> {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        co_return 10;
    }();
    
    // Second asynchronous operation, using result from first
    value = co_await [value]() -> HsBa::Slicer::Utils::Task<int> {
        co_return value * 2;
    }();
    
    co_return value;  // Final result is 20
}();
```

### 7. GeneratorInvoke Utility Function

```cpp
#include "base/coroutine.hpp"

// Using GeneratorInvoke to transform containers
std::vector<int> numbers = {1, 2, 3, 4, 5};
auto square_generator = HsBa::Slicer::Utils::GeneratorInvoke<int, std::vector>(
    [](int i) { return i * i; }, 
    numbers
);

// Iterate through generated square values
for (const auto& square : square_generator) {
    std::cout << square << " ";
}
// Output: 1 4 9 16 25
```

### 8. Exception Handling

```cpp
#include "base/coroutine.hpp"

// Generator exception handling
HsBa::Slicer::Utils::Generator<int>::SetOnCancel([]() {
    std::cout << "Generator was canceled" << std::endl;
});

// Task exception handling
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

### 9. Complete Example

```cpp
#include "base/coroutine.hpp"
#include <iostream>
#include <thread>

int main() {
    // Create a simple asynchronous task
    auto simple_task = []() -> HsBa::Slicer::Utils::Task<int> {
        std::cout << "Task running in thread: " << std::this_thread::get_id() << std::endl;
        co_return 42;
    }();
    
    // Using chained callbacks
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
    
    // Create a generator
    auto number_generator = []() -> HsBa::Slicer::Utils::Generator<int> {
        for (int i = 1; i <= 5; ++i) {
            co_yield i * i;  // Return square of i
        }
        co_return;
    }();
    
    // Use generator
    std::cout << "Generated squares:" << std::endl;
    for (const auto& num : number_generator) {
        std::cout << num << " ";
    }
    std::cout << std::endl;
    
    // Wait for task completion
    int result = simple_task.GetResult();
    std::cout << "Task result: " << result << std::endl;
    
    // Create a task with executor
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

## Notes

- Requires compiler support for C++20 coroutine features
- Coroutine functions must return Task or Generator type
- Use co_await to wait for other coroutine tasks
- Use co_yield to produce values from generator
- Use co_return to return final result or end generator
- Task executors control the execution context of coroutines
- Coroutine objects need to be initialized before use
- All coroutine tasks (and coroutine generators) should disable exceptions or only throw exceptions inherited from std::exception, otherwise results are not guaranteed
- Chained callback methods (then, catching, finally) provide more flexible asynchronous programming patterns
- Coroutines with custom allocators need to be configured through the static SetAllocator method