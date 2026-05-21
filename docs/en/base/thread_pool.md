# Thread Pool

ThreadPool provides an efficient thread pool implementation that supports task submission, asynchronous execution, and coroutine integration. Combined with ThreadPoolExecutor, it can be used as an executor in coroutines.

## Features

- Configurable number of threads
- Task submission supporting any function and parameters
- Returns std::future for obtaining task results
- Wait for all tasks to complete (WaitAll)
- Query pending and active task counts
- Coroutine support (ThreadPoolExecutor)
- Exception-safe task handling
- Thread-safe design

## Class Structure

```cpp
class ThreadPool
{
public:
    explicit ThreadPool(size_t num_threads = std::thread::hardware_concurrency());
    ~ThreadPool();
    
    // Submit tasks
    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args) 
        -> std::future<std::invoke_result_t<F, Args...>>;
    
    // Query status
    size_t PendingTasks() const;
    size_t ActiveTasks() const;
    void WaitAll();
    
    // Copy and move disabled
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
};

// Coroutine executor (requires C++20 coroutine support)
class ThreadPoolExecutor : public IExecutor
{
public:
    ThreadPoolExecutor() noexcept;
    explicit ThreadPoolExecutor(ThreadPool& pool) noexcept;
    
    void execute(std::function<void()>&& func) override;
    static void SetDefaultPool(ThreadPool* pool) noexcept;
};
```

## Usage

### 1. Basic Usage

```cpp
#include "base/thread_pool.hpp"
#include <iostream>

int main() 
{
    // Create thread pool with default thread count (hardware concurrency)
    HsBa::Slicer::ThreadPool pool(4);
    
    // Submit simple task
    auto future1 = pool.submit([]() {
        std::cout << "Task 1 running" << std::endl;
        return 42;
    });
    
    // Get result
    int result = future1.get();
    std::cout << "Result: " << result << std::endl;
    
    return 0;
}
```

### 2. Tasks with Parameters

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
    
    // Submit function with parameters
    auto future1 = pool.submit(add, 10, 20);
    std::cout << "10 + 20 = " << future1.get() << std::endl;
    
    // Submit lambda capturing external variables
    int multiplier = 3;
    auto future2 = pool.submit([multiplier](int x) {
        return x * multiplier;
    }, 15);
    
    std::cout << "15 * 3 = " << future2.get() << std::endl;
    
    // Submit member function
    auto future3 = pool.submit(greet, "World");
    std::cout << future3.get() << std::endl;
    
    return 0;
}
```

### 3. Batch Tasks and Waiting

```cpp
#include "base/thread_pool.hpp"
#include <vector>
#include <iostream>
#include <numeric>

int main() 
{
    HsBa::Slicer::ThreadPool pool(4);
    
    std::vector<std::future<int>> futures;
    
    // Submit multiple tasks
    for (int i = 0; i < 10; ++i) 
    {
        futures.push_back(pool.submit([i]() {
            // Simulate time-consuming operation
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            return i * i;
        }));
    }
    
    std::cout << "Pending tasks: " << pool.PendingTasks() << std::endl;
    std::cout << "Active tasks: " << pool.ActiveTasks() << std::endl;
    
    // Wait for all tasks to complete
    pool.WaitAll();
    
    std::cout << "All tasks completed" << std::endl;
    
    // Collect results
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

### 4. Exception Handling

```cpp
#include "base/thread_pool.hpp"
#include <iostream>
#include <stdexcept>

int main() 
{
    HsBa::Slicer::ThreadPool pool(2);
    
    // Submit task that may throw exception
    auto future1 = pool.submit([]() {
        throw std::runtime_error("Task failed!");
        return 0;
    });
    
    try 
    {
        int result = future1.get();  // Exception will be thrown here
    } 
    catch (const std::exception& e) 
    {
        std::cout << "Caught exception: " << e.what() << std::endl;
    }
    
    // Thread pool still works normally
    auto future2 = pool.submit([]() {
        return 42;
    });
    
    std::cout << "Next task result: " << future2.get() << std::endl;
    
    return 0;
}
```

### 5. ThreadPoolExecutor with Coroutines

```cpp
#include "base/thread_pool.hpp"
#include "base/coroutine.hpp"
#include <iostream>

#if __cpp_lib_coroutine && __cpp_impl_coroutine

using namespace HsBa::Slicer;
using namespace HsBa::Slicer::Utils;

// Coroutine function
Task<int> computeAsync(int value) 
{
    std::cout << "Computing " << value << "..." << std::endl;
    co_return value * 2;
}

int main() 
{
    // Create thread pool
    ThreadPool pool(4);
    
    // Set default pool
    ThreadPoolExecutor::SetDefaultPool(&pool);
    
    // Create executor
    ThreadPoolExecutor executor(pool);
    
    // Use in coroutine
    auto task = computeAsync(21);
    
    // Wait for result
    task.await();
    std::cout << "Result: " << task.result() << std::endl;
    
    pool.WaitAll();
    
    return 0;
}

#endif
```

### 6. Parallel Computation Example

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
    
    // Parallel sum calculation
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
    
    // Aggregate results
    long long total = 0;
    for (auto& f : futures) 
    {
        total += f.get();
    }
    
    std::cout << "Sum of 1 to " << size << ": " << total << std::endl;
    
    return 0;
}
```

## Important Notes

- **Thread Count**: Recommended to set to `std::thread::hardware_concurrency()` or adjust based on task characteristics
- **Task Queue**: Tasks are executed in submission order, but completion order is not guaranteed
- **Lifecycle**: ThreadPool automatically waits for all tasks to complete and joins all threads during destruction
- **Exception Propagation**: Exceptions in tasks are propagated to the caller through `std::future::get()`
- **Thread Safety**: submit, PendingTasks, ActiveTasks, and WaitAll are all thread-safe
- **Coroutine Support**: ThreadPoolExecutor requires C++20 coroutine support (`__cpp_lib_coroutine` and `__cpp_impl_coroutine`)
- **Default Pool**: If no pool is specified, ThreadPoolExecutor automatically creates a default pool instance
- **Performance Optimization**:
  - Avoid submitting too many short tasks; the task queue itself has overhead
  - For CPU-intensive tasks, thread count equal to core count is optimal
  - For I/O-intensive tasks,可以适当 increase thread count
- **Resource Management**: Thread pool is an exclusive resource; copying and moving are disabled

## Typical Use Cases

- Parallel data processing and computation
- Asynchronous I/O operations
- Batch task processing
- Web server request handling
- Image processing pipelines
- Machine learning inference tasks
- Executor for coroutine asynchronous scheduling
