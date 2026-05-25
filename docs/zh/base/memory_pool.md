# Memory Pool (内存池)

MemoryPool 和 StaticMemoryPool 提供了两种不同类型的内存池分配器实现，用于优化频繁内存分配和释放场景的性能。

## 功能特点

### MemoryPool（共享内存池）
- 基于 shared_ptr 共享状态的内存池
- 支持多个 MemoryPool 实例共享同一块内存区域
- 线程安全的内存分配和释放
- 支持任意类型的内存分配
- 符合 STL Allocator 接口规范

### StaticMemoryPool（静态内存池）
- 全局共享的静态内存池
- 所有实例共享同一块静态内存区域
- 线程安全的内存分配和释放
- 零实例开销，所有实例被视为相等
- 符合 STL Allocator 接口规范

## 类结构

```cpp
// 内存池状态（用于 MemoryPool）
template <size_t PoolSize>
struct MemoryPoolState
{
    alignas(std::max_align_t) char pool[PoolSize];
    bool used[PoolSize] = { false };
    size_t used_count = 0;
};

// 共享内存池
template <typename T, size_t PoolSize>
class MemoryPool
{
public:
    using value_type = T;
    using pointer = T*;
    using size_type = size_t;
    
    template <typename U>
    struct rebind { using other = MemoryPool<U, PoolSize>; };
    
    pointer allocate(size_type n, const void* hint = nullptr);
    void deallocate(pointer p, size_type n);
    size_type max_size() const noexcept;
    
    template <typename U, typename... Args>
    void construct(U* p, Args&&... args);
    
    template <typename U>
    void destroy(U* p);
};

// 静态内存池
template <typename T, size_t PoolSize>
class StaticMemoryPool
{
public:
    using value_type = T;
    using pointer = T*;
    using size_type = size_t;
    
    template <typename U>
    struct rebind { using other = StaticMemoryPool<U, PoolSize>; };
    
    pointer allocate(size_type n, const void* hint = nullptr);
    void deallocate(pointer p, size_type n);
    size_type max_size() const noexcept;
    
    template <typename U, typename... Args>
    void construct(U* p, Args&&... args);
    
    template <typename U>
    void destroy(U* p);
    
    size_t UsedCount() const;
};
```

## 使用方法

### 1. MemoryPool 基本使用

```cpp
#include "base/memory_pool.hpp"
#include <vector>

class MyObject 
{
public:
    MyObject(int value) : value_(value) {}
    int getValue() const { return value_; }

private:
    int value_;
};

int main() 
{
    // 创建 64KB 的内存池
    HsBa::Slicer::MemoryPool<MyObject, 65536> pool;
    
    // 使用内存池分配单个对象
    MyObject* obj = pool.allocate(1);
    pool.construct(obj, 42);
    
    std::cout << "Value: " << obj->getValue() << std::endl;
    
    // 销毁并释放对象
    pool.destroy(obj);
    pool.deallocate(obj, 1);
    
    return 0;
}
```

### 2. 与 STL 容器配合使用

```cpp
#include "base/memory_pool.hpp"
#include <vector>
#include <iostream>

int main() 
{
    // 创建 1MB 的内存池
    using MyPool = HsBa::Slicer::MemoryPool<int, 1048576>;
    MyPool pool;
    
    // 使用内存池的 vector
    std::vector<int, MyPool> vec(pool);
    
    // 正常操作 vector
    for (int i = 0; i < 1000; ++i) 
    {
        vec.push_back(i);
    }
    
    std::cout << "Vector size: " << vec.size() << std::endl;
    std::cout << "Sum: ";
    int sum = 0;
    for (int val : vec) 
    {
        sum += val;
    }
    std::cout << sum << std::endl;
    
    return 0;
}
```

### 3. MemoryPool 共享状态

```cpp
#include "base/memory_pool.hpp"
#include <iostream>

int main() 
{
    // 创建内存池
    HsBa::Slicer::MemoryPool<double, 32768> pool1;
    
    // 复制池，共享同一块内存
    HsBa::Slicer::MemoryPool<double, 32768> pool2(pool1);
    
    // 从 pool1 分配
    double* ptr1 = pool1.allocate(10);
    pool1.construct(ptr1, 3.14);
    
    std::cout << "Value from pool1: " << *ptr1 << std::endl;
    
    // 从 pool2 分配（使用同一内存区域）
    double* ptr2 = pool2.allocate(10);
    pool2.construct(ptr2, 2.71);
    
    std::cout << "Value from pool2: " << *ptr2 << std::endl;
    
    // 清理
    pool1.destroy(ptr1);
    pool1.deallocate(ptr1, 10);
    pool2.destroy(ptr2);
    pool2.deallocate(ptr2, 10);
    
    return 0;
}
```

### 4. StaticMemoryPool 使用

```cpp
#include "base/memory_pool.hpp"
#include <list>
#include <iostream>

class Node 
{
public:
    Node(int data) : data_(data), next_(nullptr) {}
    
    int data_;
    Node* next_;
};

int main() 
{
    // 静态内存池，所有实例共享同一内存
    HsBa::Slicer::StaticMemoryPool<Node, 65536> pool1;
    HsBa::Slicer::StaticMemoryPool<Node, 65536> pool2;
    
    // 两个池子被视为相等
    std::cout << "Pools equal: " << (pool1 == pool2) << std::endl;
    
    // 使用静态内存池的 list
    std::list<Node, HsBa::Slicer::StaticMemoryPool<Node, 65536>> list;
    
    // 添加节点
    for (int i = 0; i < 100; ++i) 
    {
        list.emplace_back(i * 10);
    }
    
    std::cout << "List size: " << list.size() << std::endl;
    std::cout << "Memory used: " << list.get_allocator().UsedCount() 
              << " bytes" << std::endl;
    
    return 0;
}
```

### 5. 性能优化示例

```cpp
#include "base/memory_pool.hpp"
#include <vector>
#include <chrono>
#include <iostream>

struct SmallObject 
{
    int x, y, z;
    double value;
};

void benchmark_with_pool() 
{
    HsBa::Slicer::MemoryPool<SmallObject, 1048576> pool;
    std::vector<SmallObject*, HsBa::Slicer::MemoryPool<SmallObject*, 65536>> ptrs(pool);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // 使用内存池分配
    for (int i = 0; i < 10000; ++i) 
    {
        SmallObject* obj = pool.allocate(1);
        pool.construct(obj, SmallObject{i, i+1, i+2, static_cast<double>(i)});
        ptrs.push_back(obj);
    }
    
    // 清理
    for (auto* obj : ptrs) 
    {
        pool.destroy(obj);
        pool.deallocate(obj, 1);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "With pool: " << duration.count() << " μs" << std::endl;
}

void benchmark_without_pool() 
{
    std::vector<SmallObject*> ptrs;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // 使用普通 new/delete
    for (int i = 0; i < 10000; ++i) 
    {
        SmallObject* obj = new SmallObject{i, i+1, i+2, static_cast<double>(i)};
        ptrs.push_back(obj);
    }
    
    // 清理
    for (auto* obj : ptrs) 
    {
        delete obj;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "Without pool: " << duration.count() << " μs" << std::endl;
}

int main() 
{
    benchmark_with_pool();
    benchmark_without_pool();
    return 0;
}
```

## 注意事项

- **对齐要求**：内存池使用 `alignas(std::max_align_t)` 确保最大对齐，适用于任何类型
- **碎片管理**：采用简单的首次适配策略，可能产生内存碎片
- **线程安全**：使用 `std::mutex` 保护分配和释放操作
- **容量限制**：达到池容量时抛出 `std::bad_alloc`
- **MemoryPool vs StaticMemoryPool**：
  - `MemoryPool`：实例间可共享状态，通过 shared_ptr 管理
  - `StaticMemoryPool`：全局单例，零实例开销，所有实例共享同一静态内存
- **STL 兼容**：完全符合 STL Allocator 接口，可直接用于标准容器
- **内存泄漏检测**：可通过检查 `used_count` 判断是否有未释放的内存
- **适用场景**：适合频繁分配/释放相同大小对象的场景，不适合大对象或生命周期差异大的对象

## 典型应用场景

- 高频交易系统中的订单对象分配
- 游戏开发中的实体/组件分配
- 实时系统中的确定性内存分配
- 缓存系统中的对象存储
- 减少内存碎片化的场景
