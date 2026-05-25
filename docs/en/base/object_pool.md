# Object Pool

NamedObjectPool provides a name-based object pool implementation that supports object creation, retrieval, automatic cleanup, and more. It's ideal for scenarios requiring object reuse or centralized object lifecycle management.

## Features

- Name-based object management to prevent duplicate creation
- Automatic cleanup of inactive objects (reference count equals 1)
- Thread-safe (using shared_mutex for read-write separation)
- Support for custom allocators
- Capacity limits to prevent excessive memory usage
- Weak reference tracking for object state

## Class Structure

```cpp
template <typename T, std::size_t MaxSize>
class NamedObjectPool
{
public:
    using ObjectType = T;
    using ObjectPtr = std::shared_ptr<T>;
    using WeakPtr = std::weak_ptr<T>;
    static constexpr std::size_t max_size = MaxSize;

    // Core methods
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

## Usage

### 1. Basic Usage

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
    // Create object pool with max 100 objects
    HsBa::Slicer::NamedObjectPool<MyObject, 100> pool;
    
    // Create and add objects
    auto obj1 = pool.emplace("obj1", 10, "Object 1");
    auto obj2 = pool.emplace("obj2", 20, "Object 2");
    
    // Retrieve object by name
    auto retrieved = pool.get("obj1");
    if (retrieved) 
    {
        std::cout << retrieved->getName() << ": " 
                  << retrieved->getValue() << std::endl;
    }
    
    // Using operator overload
    auto via_op = pool["obj2"];
    
    return 0;
}
```

### 2. Object Lifecycle Management

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
    
    // Create objects
    auto res1 = pool.emplace("res1", "R001");
    std::cout << "Pool size: " << pool.size() << std::endl;
    std::cout << "Active: " << pool.ActiveCount() << std::endl;
    std::cout << "Inactive: " << pool.InactiveCount() << std::endl;
    
    // Release external reference
    res1.reset();
    
    std::cout << "After reset:" << std::endl;
    std::cout << "Pool size: " << pool.size() << std::endl;
    std::cout << "Active: " << pool.ActiveCount() << std::endl;
    std::cout << "Inactive: " << pool.InactiveCount() << std::endl;
    
    // Manually cleanup inactive objects
    pool.Cleanup();
    std::cout << "After cleanup: " << pool.size() << std::endl;
    
    return 0;
}
```

### 3. Using Custom Allocators

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
    
    // Use custom allocator (e.g., aligned allocator)
    std::allocator<ExpensiveObject> alloc;
    auto obj = pool.allocate("expensive", alloc, 3.14159);
    
    return 0;
}
```

### 4. Object Pool State Query

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
    
    // Create multiple objects
    for (int i = 0; i < 5; ++i) 
    {
        pool.emplace("data_" + std::to_string(i), i);
    }
    
    // Query state
    std::cout << "Total objects: " << pool.size() << std::endl;
    std::cout << "Active objects: " << pool.ActiveCount() << std::endl;
    std::cout << "Inactive objects: " << pool.InactiveCount() << std::endl;
    
    // Get all object names
    auto names = pool.GetNames();
    std::cout << "Object names:" << std::endl;
    for (const auto& name : names) 
    {
        std::cout << "  - " << name << std::endl;
    }
    
    // Check if object exists
    if (pool.Contains("data_2")) 
    {
        std::cout << "data_2 exists" << std::endl;
    }
    
    return 0;
}
```

### 5. Automatic Cleanup Mechanism

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
    // Small capacity pool for demonstration
    HsBa::Slicer::NamedObjectPool<Task, 3> pool;
    
    // Fill the pool
    auto t1 = pool.emplace("t1", 1);
    auto t2 = pool.emplace("t2", 2);
    auto t3 = pool.emplace("t3", 3);
    
    std::cout << "Pool full, size: " << pool.size() << std::endl;
    
    // Release some references
    t1.reset();
    t2.reset();
    
    // Adding a new object triggers automatic cleanup
    // Because pool is full but has inactive objects, cleanup occurs automatically
    auto t4 = pool.emplace("t4", 4);  // Success: cleaned up t1 and t2
    
    std::cout << "After adding t4, size: " << pool.size() << std::endl;
    std::cout << "Active: " << pool.ActiveCount() << std::endl;
    
    return 0;
}
```

## Important Notes

- **Thread Safety**: Uses `std::shared_mutex` for read-write separation. Read-only operations like `get` and `Contains` use shared locks, while write operations like `emplace`, `allocate`, and `Cleanup` use exclusive locks
- **Object Identification**: Objects are uniquely identified by name; duplicate names are not allowed
- **Activity Detection**: An object is considered inactive when its `shared_ptr` reference count is 1 (only held by the pool itself)
- **Capacity Management**: When reaching `MaxSize`, the pool first attempts to clean up inactive objects; if still full after cleanup, an exception is thrown
- **Lazy Cleanup**: Automatic cleanup is only triggered when attempting to add a new object and the pool is full, avoiding frequent cleanup that impacts performance
- **Performance Considerations**: Object pools effectively reduce memory allocation overhead for frequently created and destroyed objects
- **Memory Retention**: The pool maintains references to active objects, ensuring they are not prematurely destroyed

## Typical Use Cases

- Caching systems: Cache frequently accessed objects
- Resource pools: Manage expensive resources like database connections, file handles
- Object reuse: Reduce overhead of frequently creating and destroying objects of the same type
- Centralized management: Unified management of related object lifecycles
