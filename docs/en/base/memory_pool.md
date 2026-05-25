# Memory Pool

MemoryPool and StaticMemoryPool provide two types of memory pool allocator implementations for optimizing performance in scenarios with frequent memory allocation and deallocation.

## Features

### MemoryPool (Shared Memory Pool)
- Memory pool with shared state based on shared_ptr
- Supports multiple MemoryPool instances sharing the same memory region
- Thread-safe memory allocation and deallocation
- Supports allocation for any type
- Compliant with STL Allocator interface specification

### StaticMemoryPool (Static Memory Pool)
- Globally shared static memory pool
- All instances share the same static memory region
- Thread-safe memory allocation and deallocation
- Zero instance overhead, all instances are considered equal
- Compliant with STL Allocator interface specification

## Class Structure

```cpp
// Memory pool state (for MemoryPool)
template <size_t PoolSize>
struct MemoryPoolState
{
    alignas(std::max_align_t) char pool[PoolSize];
    bool used[PoolSize] = { false };
    size_t used_count = 0;
};

// Shared memory pool
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

// Static memory pool
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

## Usage

### 1. Basic MemoryPool Usage

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
    // Create 64KB memory pool
    HsBa::Slicer::MemoryPool<MyObject, 65536> pool;
    
    // Allocate single object using memory pool
    MyObject* obj = pool.allocate(1);
    pool.construct(obj, 42);
    
    std::cout << "Value: " << obj->getValue() << std::endl;
    
    // Destroy and release object
    pool.destroy(obj);
    pool.deallocate(obj, 1);
    
    return 0;
}
```

### 2. Using with STL Containers

```cpp
#include "base/memory_pool.hpp"
#include <vector>
#include <iostream>

int main() 
{
    // Create 1MB memory pool
    using MyPool = HsBa::Slicer::MemoryPool<int, 1048576>;
    MyPool pool;
    
    // Vector using memory pool
    std::vector<int, MyPool> vec(pool);
    
    // Normal vector operations
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

### 3. MemoryPool State Sharing

```cpp
#include "base/memory_pool.hpp"
#include <iostream>

int main() 
{
    // Create memory pool
    HsBa::Slicer::MemoryPool<double, 32768> pool1;
    
    // Copy pool, shares the same memory
    HsBa::Slicer::MemoryPool<double, 32768> pool2(pool1);
    
    // Allocate from pool1
    double* ptr1 = pool1.allocate(10);
    pool1.construct(ptr1, 3.14);
    
    std::cout << "Value from pool1: " << *ptr1 << std::endl;
    
    // Allocate from pool2 (uses same memory region)
    double* ptr2 = pool2.allocate(10);
    pool2.construct(ptr2, 2.71);
    
    std::cout << "Value from pool2: " << *ptr2 << std::endl;
    
    // Cleanup
    pool1.destroy(ptr1);
    pool1.deallocate(ptr1, 10);
    pool2.destroy(ptr2);
    pool2.deallocate(ptr2, 10);
    
    return 0;
}
```

### 4. StaticMemoryPool Usage

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
    // Static memory pool, all instances share the same memory
    HsBa::Slicer::StaticMemoryPool<Node, 65536> pool1;
    HsBa::Slicer::StaticMemoryPool<Node, 65536> pool2;
    
    // Two pools are considered equal
    std::cout << "Pools equal: " << (pool1 == pool2) << std::endl;
    
    // List using static memory pool
    std::list<Node, HsBa::Slicer::StaticMemoryPool<Node, 65536>> list;
    
    // Add nodes
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

### 5. Performance Optimization Example

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
    
    // Allocate using memory pool
    for (int i = 0; i < 10000; ++i) 
    {
        SmallObject* obj = pool.allocate(1);
        pool.construct(obj, SmallObject{i, i+1, i+2, static_cast<double>(i)});
        ptrs.push_back(obj);
    }
    
    // Cleanup
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
    
    // Using regular new/delete
    for (int i = 0; i < 10000; ++i) 
    {
        SmallObject* obj = new SmallObject{i, i+1, i+2, static_cast<double>(i)};
        ptrs.push_back(obj);
    }
    
    // Cleanup
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

## Important Notes

- **Alignment Requirements**: Memory pool uses `alignas(std::max_align_t)` to ensure maximum alignment, suitable for any type
- **Fragmentation Management**: Uses simple first-fit strategy, which may cause memory fragmentation
- **Thread Safety**: Uses `std::mutex` to protect allocation and deallocation operations
- **Capacity Limits**: Throws `std::bad_alloc` when pool capacity is reached
- **MemoryPool vs StaticMemoryPool**:
  - `MemoryPool`: Instances can share state, managed via shared_ptr
  - `StaticMemoryPool`: Global singleton, zero instance overhead, all instances share the same static memory
- **STL Compatibility**: Fully compliant with STL Allocator interface, can be used directly with standard containers
- **Memory Leak Detection**: Can check `used_count` to determine if there are unreleased memory blocks
- **Applicable Scenarios**: Suitable for scenarios with frequent allocation/deallocation of same-size objects, not suitable for large objects or objects with significantly different lifecycles

## Typical Use Cases

- Order object allocation in high-frequency trading systems
- Entity/component allocation in game development
- Deterministic memory allocation in real-time systems
- Object storage in caching systems
- Scenarios requiring reduced memory fragmentation
