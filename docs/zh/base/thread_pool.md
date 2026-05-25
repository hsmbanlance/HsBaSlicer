# Thread Pool (线程池)

ThreadPool 提供了一个高效的线程池实现，支持任务提交、异步执行和协程集成。配合 ThreadPoolExecutor 可以在协程中使用线程池作为执行器。

## 功能特点

- 可配置线程数量的线程池
- 支持任意函数和参数的任务提交
- 返回 std::future 用于获取任务结果
- 等待所有任务完成（WaitAll）
- 查询待处理和活跃任务数量
- 协程支持（ThreadPoolExecutor）
- 异常安全的任务处理
- 线程安全的设计

## 类结构

```cpp
class ThreadPool
{
public:
    explicit ThreadPool(size_t num_threads = std::thread::hardware_concurrency());
    ~ThreadPool();
    
    // 提交任务
    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args) 
        -> std::future<std::invoke_result_t<F, Args...>>;
    
    // 查询状态
    size_t PendingTasks() const;
    size_t ActiveTasks() const;
    void WaitAll();
    
    // 禁止拷贝和移动
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
};

// 协程执行器（需要 C++20 协程支持）
class ThreadPoolExecutor : public IExecutor
{
public:
    ThreadPoolExecutor() noexcept;
    explicit ThreadPoolExecutor(ThreadPool& pool) noexcept;
    
    void execute(std::function<void()>&& func) override;
    static void SetDefaultPool(ThreadPool* pool) noexcept;
};
```

## 使用方法

### 1. 基本使用

```cpp
#include "base/thread_pool.hpp"
#include <iostream>

int main() 
{
    // 创建线程池，使用默认线程数（硬件并发数）
    HsBa::Slicer::ThreadPool pool(4);
    
    // 提交简单任务
    auto future1 = pool.submit([]() {
        std::cout << "Task 1 running" << std::endl;
        return 42;
    });
    
    // 获取结果
    int result = future1.get();
    std::cout << "Result: " << result << std::endl;
    
    return 0;
}
```

### 2. 带参数的任务

```cpp
#include "base/thread_pool.hpp"
#include <string>
#include <iostream>

int add(int a, int b) 
{
    return a + b;
}

std::string greet(const std::string& name) 
{
    return "Hello, " + name + "!";
}

int main() 
{
    HsBa::Slicer::ThreadPool pool(2);
    
    // 提交带参数的函数
    auto future1 = pool.submit(add, 10, 20);
    std::cout << "10 + 20 = " << future1.get() << std::endl;
    
    // 提交 lambda 捕获外部变量
    int multiplier = 3;
    auto future2 = pool.submit([multiplier](int x) {
        return x * multiplier;
    }, 15);
    
    std::cout << "15 * 3 = " << future2.get() << std::endl;
    
    // 提交成员函数
    auto future3 = pool.submit(greet, "World");
    std::cout << future3.get() << std::endl;
    
    return 0;
}
```

### 3. 批量任务和等待

```cpp
#include "base/thread_pool.hpp"
#include <vector>
#include <iostream>
#include <numeric>

int main() 
{
    HsBa::Slicer::ThreadPool pool(4);
    
    std::vector<std::future<int>> futures;
    
    // 提交多个任务
    for (int i = 0; i < 10; ++i) 
    {
        futures.push_back(pool.submit([i]() {
            // 模拟耗时操作
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            return i * i;
        }));
    }
    
    std::cout << "Pending tasks: " << pool.PendingTasks() << std::endl;
    std::cout << "Active tasks: " << pool.ActiveTasks() << std::endl;
    
    // 等待所有任务完成
    pool.WaitAll();
    
    std::cout << "All tasks completed" << std::endl;
    
    // 收集结果
    std::vector<int> results;
    for (auto& f : futures) 
    {
        results.push_back(f.get());
    }
    
    int sum = std::accumulate(results.begin(), results.end(), 0);
    std::cout << "Sum of squares: " << sum << std::endl;
    
    return 0;
}
```

### 4. 异常处理

```cpp
#include "base/thread_pool.hpp"
#include <iostream>
#include <stdexcept>

int main() 
{
    HsBa::Slicer::ThreadPool pool(2);
    
    // 提交可能抛出异常的任务
    auto future1 = pool.submit([]() {
        throw std::runtime_error("Task failed!");
        return 0;
    });
    
    try 
    {
        int result = future1.get();  // 异常会在这里抛出
    } 
    catch (const std::exception& e) 
    {
        std::cout << "Caught exception: " << e.what() << std::endl;
    }
    
    // 线程池仍然可以正常工作
    auto future2 = pool.submit([]() {
        return 42;
    });
    
    std::cout << "Next task result: " << future2.get() << std::endl;
    
    return 0;
}
```

### 5. ThreadPoolExecutor 与协程

```cpp
#include "base/thread_pool.hpp"
#include "base/coroutine.hpp"
#include <iostream>

#if __cpp_lib_coroutine && __cpp_impl_coroutine

using namespace HsBa::Slicer;
using namespace HsBa::Slicer::Utils;

// 协程函数
Task<int> computeAsync(int value) 
{
    std::cout << "Computing " << value << "..." << std::endl;
    co_return value * 2;
}

int main() 
{
    // 创建线程池
    ThreadPool pool(4);
    
    // 设置默认池
    ThreadPoolExecutor::SetDefaultPool(&pool);
    
    // 创建执行器
    ThreadPoolExecutor executor(pool);
    
    // 在协程中使用
    auto task = computeAsync(21);
    
    // 等待结果
    task.await();
    std::cout << "Result: " << task.result() << std::endl;
    
    pool.WaitAll();
    
    return 0;
}

#endif
```

### 6. 并行计算示例

```cpp
#include "base/thread_pool.hpp"
#include <vector>
#include <numeric>
#include <iostream>

int main() 
{
    HsBa::Slicer::ThreadPool pool(8);
    
    const int size = 1000000;
    std::vector<int> data(size);
    std::iota(data.begin(), data.end(), 1);
    
    // 并行计算总和
    const int chunks = 8;
    const int chunk_size = size / chunks;
    
    std::vector<std::future<long long>> futures;
    
    for (int i = 0; i < chunks; ++i) 
    {
        auto start = data.begin() + i * chunk_size;
        auto end = (i == chunks - 1) ? data.end() : start + chunk_size;
        
        futures.push_back(pool.submit([start, end]() -> long long {
            return std::accumulate(start, end, 0LL);
        }));
    }
    
    // 汇总结果
    long long total = 0;
    for (auto& f : futures) 
    {
        total += f.get();
    }
    
    std::cout << "Sum of 1 to " << size << ": " << total << std::endl;
    
    return 0;
}
```

## 注意事项

- **线程数量**：建议设置为 `std::thread::hardware_concurrency()` 或根据任务特性调整
- **任务队列**：任务按提交顺序执行，但不保证完成顺序
- **生命周期**：ThreadPool 析构时会自动等待所有任务完成并 join 所有线程
- **异常传播**：任务中的异常会通过 `std::future::get()` 传播到调用方
- **线程安全**：submit、PendingTasks、ActiveTasks、WaitAll 都是线程安全的
- **协程支持**：ThreadPoolExecutor 需要 C++20 协程支持（`__cpp_lib_coroutine` 和 `__cpp_impl_coroutine`）
- **默认池**：ThreadPoolExecutor 如果没有指定池，会自动创建默认池实例
- **性能优化**：
  - 避免提交过多的短任务，任务队列本身有开销
  - 对于 CPU 密集型任务，线程数等于核心数最佳
  - 对于 I/O 密集型任务，可以适当增加线程数
- **资源管理**：线程池是独占资源，禁止拷贝和移动

## 典型应用场景

- 并行数据处理和计算
- 异步 I/O 操作
- 批量任务处理
- Web 服务器请求处理
- 图像处理流水线
- 机器学习推理任务
- 协程异步调度的执行器
