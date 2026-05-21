# Object Pool (对象池)

NamedObjectPool 提供了一个基于名称管理的对象池实现，支持对象的创建、获取、自动清理等功能，适用于需要复用对象或集中管理对象生命周期的场景。

## 功能特点

- 基于名称的对象管理，避免重复创建
- 自动清理非活跃对象（引用计数为1）
- 线程安全（使用 shared_mutex 实现读写分离）
- 支持自定义分配器
- 容量限制，防止内存过度使用
- 支持弱引用追踪对象状态

## 类结构

```cpp
template <typename T, std::size_t MaxSize>
class NamedObjectPool
{
public:
    using ObjectType = T;
    using ObjectPtr = std::shared_ptr<T>;
    using WeakPtr = std::weak_ptr<T>;
    static constexpr std::size_t max_size = MaxSize;

    // 核心方法
    template <typename... Args>
    ObjectPtr emplace(const std::string& name, Args&&... args);
    
    template<typename Allocator, typename... Args>
    ObjectPtr allocate(const std::string& name, Allocator&& alloc, Args&&... args);
    
    ObjectPtr get(const std::string& name);
    ObjectPtr operator[](const std::string& name);
    
    bool Contains(const std::string& name) const;
    std::size_t size() const;
    std::size_t InactiveCount() const;
    std::size_t ActiveCount() const;
    std::size_t Cleanup();
    std::vector<std::string> GetNames() const;
};
```

## 使用方法

### 1. 基本使用

```cpp
#include "base/object_pool.hpp"

class MyObject 
{
public:
    MyObject(int value, const std::string& name) 
        : value_(value), name_(name) {}
    
    int getValue() const { return value_; }
    const std::string& getName() const { return name_; }

private:
    int value_;
    std::string name_;
};

int main() 
{
    // 创建对象池，最多容纳100个对象
    HsBa::Slicer::NamedObjectPool<MyObject, 100> pool;
    
    // 创建并添加对象
    auto obj1 = pool.emplace("obj1", 10, "Object 1");
    auto obj2 = pool.emplace("obj2", 20, "Object 2");
    
    // 通过名称获取对象
    auto retrieved = pool.get("obj1");
    if (retrieved) 
    {
        std::cout << retrieved->getName() << ": " 
                  << retrieved->getValue() << std::endl;
    }
    
    // 使用操作符重载
    auto via_op = pool["obj2"];
    
    return 0;
}
```

### 2. 对象生命周期管理

```cpp
#include "base/object_pool.hpp"
#include <iostream>

class Resource 
{
public:
    Resource(const std::string& id) : id_(id) 
    {
        std::cout << "Resource " << id_ << " created" << std::endl;
    }
    
    ~Resource() 
    {
        std::cout << "Resource " << id_ << " destroyed" << std::endl;
    }

private:
    std::string id_;
};

int main() 
{
    HsBa::Slicer::NamedObjectPool<Resource, 50> pool;
    
    // 创建对象
    auto res1 = pool.emplace("res1", "R001");
    std::cout << "Pool size: " << pool.size() << std::endl;
    std::cout << "Active: " << pool.ActiveCount() << std::endl;
    std::cout << "Inactive: " << pool.InactiveCount() << std::endl;
    
    // 释放外部引用
    res1.reset();
    
    std::cout << "After reset:" << std::endl;
    std::cout << "Pool size: " << pool.size() << std::endl;
    std::cout << "Active: " << pool.ActiveCount() << std::endl;
    std::cout << "Inactive: " << pool.InactiveCount() << std::endl;
    
    // 手动清理非活跃对象
    pool.Cleanup();
    std::cout << "After cleanup: " << pool.size() << std::endl;
    
    return 0;
}
```

### 3. 使用自定义分配器

```cpp
#include "base/object_pool.hpp"

class ExpensiveObject 
{
public:
    ExpensiveObject(double param) : param_(param) {}

private:
    double param_;
};

int main() 
{
    HsBa::Slicer::NamedObjectPool<ExpensiveObject, 30> pool;
    
    // 使用自定义分配器（例如对齐分配器）
    std::allocator<ExpensiveObject> alloc;
    auto obj = pool.allocate("expensive", alloc, 3.14159);
    
    return 0;
}
```

### 4. 对象池状态查询

```cpp
#include "base/object_pool.hpp"
#include <vector>
#include <iostream>

class Data 
{
public:
    Data(int id) : id_(id) {}
    int getId() const { return id_; }

private:
    int id_;
};

int main() 
{
    HsBa::Slicer::NamedObjectPool<Data, 100> pool;
    
    // 创建多个对象
    for (int i = 0; i < 5; ++i) 
    {
        pool.emplace("data_" + std::to_string(i), i);
    }
    
    // 查询状态
    std::cout << "Total objects: " << pool.size() << std::endl;
    std::cout << "Active objects: " << pool.ActiveCount() << std::endl;
    std::cout << "Inactive objects: " << pool.InactiveCount() << std::endl;
    
    // 获取所有对象名称
    auto names = pool.GetNames();
    std::cout << "Object names:" << std::endl;
    for (const auto& name : names) 
    {
        std::cout << "  - " << name << std::endl;
    }
    
    // 检查对象是否存在
    if (pool.Contains("data_2")) 
    {
        std::cout << "data_2 exists" << std::endl;
    }
    
    return 0;
}
```

### 5. 自动清理机制

```cpp
#include "base/object_pool.hpp"
#include <memory>

class Task 
{
public:
    Task(int priority) : priority_(priority) {}
    int getPriority() const { return priority_; }

private:
    int priority_;
};

int main() 
{
    // 小容量对象池用于演示
    HsBa::Slicer::NamedObjectPool<Task, 3> pool;
    
    // 填满对象池
    auto t1 = pool.emplace("t1", 1);
    auto t2 = pool.emplace("t2", 2);
    auto t3 = pool.emplace("t3", 3);
    
    std::cout << "Pool full, size: " << pool.size() << std::endl;
    
    // 释放部分引用
    t1.reset();
    t2.reset();
    
    // 添加新对象时会触发自动清理
    // 因为池已满但有非活跃对象，会自动清理
    auto t4 = pool.emplace("t4", 4);  // 成功：清理了t1和t2
    
    std::cout << "After adding t4, size: " << pool.size() << std::endl;
    std::cout << "Active: " << pool.ActiveCount() << std::endl;
    
    return 0;
}
```

## 注意事项

- **线程安全**：使用 `std::shared_mutex` 实现读写分离锁，`get`、`Contains` 等只读操作使用共享锁，`emplace`、`allocate`、`Cleanup` 等写操作使用独占锁
- **对象识别**：通过名称唯一标识对象，不允许同名对象存在
- **活跃判断**：当 `shared_ptr` 的引用计数为 1 时（仅对象池内部持有），认为对象是非活跃的
- **容量管理**：达到 `MaxSize` 时，会先尝试清理非活跃对象；若清理后仍满，则抛出异常
- **延迟清理**：只有在尝试添加新对象且池满时才会触发自动清理，避免频繁清理影响性能
- **性能考虑**：对于频繁创建销毁的对象，对象池可以有效减少内存分配开销
- **内存占用**：对象池会保持活跃对象的引用，确保对象不会过早销毁

## 典型应用场景

- 缓存系统：缓存频繁访问的对象
- 资源池：管理数据库连接、文件句柄等昂贵资源
- 对象复用：减少频繁创建销毁相同类型对象的开销
- 集中管理：统一管理一组相关对象的生命周期
