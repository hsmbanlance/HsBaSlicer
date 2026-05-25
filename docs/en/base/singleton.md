# Singleton

The Singleton component provides a thread-safe singleton pattern implementation using modern C++ features to ensure instance uniqueness and thread safety.

## Features

- Thread-safe singleton pattern implementation
- Uses std::call_once to ensure instance is created only once
- Uses std::shared_ptr to manage object lifecycle
- Supports parameterized constructors

## Usage

### 1. Define Singleton Class

```cpp
#include "base/singleton.hpp"

class MyService
{
public:
    // Need to inherit from Singleton<T> and use Protected constructor
    struct Protected {};
    
    MyService(Protected, int value) : data_(value) {}
    
    void DoSomething() {
        // Business logic
    }
    
    int GetData() const { return data_; }

private:
    int data_;
    friend class HsBa::Slicer::Utils::Singleton<MyService>; // Friend access permission
};
```

### 2. Get Singleton Instance

```cpp
// Get singleton instance
auto instance = HsBa::Slicer::Utils::Singleton<MyService>::GetInstance(42);
auto same_instance = HsBa::Slicer::Utils::Singleton<MyService>::GetInstance(42);

// Both instances are the same
assert(instance == same_instance);
```

### 3. Complete Example

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

// Usage example
int main() {
    auto logger1 = HsBa::Slicer::Utils::Singleton<Logger>::GetInstance("App");
    auto logger2 = HsBa::Slicer::Utils::Singleton<Logger>::GetInstance("App");
    
    logger1->Log("Hello World");
    logger2->Log("Same instance");
    
    // Both instances are the same object
    assert(logger1 == logger2);
    
    return 0;
}
```

## Notes

- Singleton class needs to define a `Protected` struct
- Singleton class needs to declare `Singleton<T>` as a friend class
- Constructor needs to accept `Protected` as the first parameter
- Instance is obtained via the `GetInstance` method, supporting constructor argument passing