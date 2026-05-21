# Any Object (任意对象)

AnyObject 组件提供了运行时类型反射和动态调用的功能，允许在运行时检查对象的字段、调用方法，并支持自定义类型信息的注册。

## 功能特点

- 运行时类型反射
- 动态字段访问（可选）
- 动态方法调用
- 自定义类型信息注册
- 支持虚函数和位域
- 类型安全的任意类型存储
- 支持移动和拷贝语义
- 支持非成员函数注册

## 使用方法

### 1. 基本用法

```cpp
#include "base/any_object.hpp"
#include <iostream>

// 创建一个存储 int 值的 AnyObject
HsBa::Slicer::Utils::AnyObject obj = 42;

// 获取存储的值
int value = obj.cast<int>();
std::cout << "Value: " << value << std::endl;  // 输出：Value: 42
```

### 2. 自定义类型信息注册

对于自定义类型，需要特化 `GetTypeInfo` 函数来提供类型信息：

```cpp
#include "base/any_object.hpp"

struct Player
{
    int health;
    float speed;
    
    int AddHealth(int amount) {
        health += amount;
        return health;
    }
};

namespace HsBa::Slicer::Utils
{
    template<>
    TypeInfo* GetTypeInfo<Player>()
    {
        static TypeInfo info;
        info.Name = "Player";
        info.destroy = [](void* data) { delete static_cast<Player*>(data); };
        info.copy = [](const void* data) -> void* { 
            return new Player(*static_cast<const Player*>(data)); 
        };
        info.move = [](void* data) -> void* { 
            return new Player(std::move(*static_cast<Player*>(data))); 
        };
        
        // 注册字段（可选）
        info.fields.clear();
        info.fields.emplace("health", std::make_pair(GetTypeInfo<int>(), offsetof(Player, health)));
        info.fields.emplace("speed", std::make_pair(GetTypeInfo<float>(), offsetof(Player, speed)));
        
        // 注册方法
        info.methods.clear();
        info.methods.emplace("AddHealth", type_ensure<&Player::AddHealth>());
        
        return &info;
    }
}
```

**说明**：
- **字段注册是可选的**：可以选择不注册任何字段（保持 `fields` 为空），或者只注册部分需要反射访问的字段
- **最小化注册**：如果只需要方法调用功能，可以不注册字段，只注册方法
- **按需注册**：根据实际运行时需求，选择性注册需要动态访问的字段

### 3. 字段访问

使用 `ForeachField` 遍历对象的所有字段：

```cpp
#include "base/any_object.hpp"
#include <iostream>

Player player{100, 5.5f};
HsBa::Slicer::Utils::AnyObject obj(player);

// 遍历所有字段
obj.ForeachField([&](std::string_view name, AnyObject value) {
    std::cout << "Field: " << name;
    if (name == "health") {
        std::cout << " = " << value.cast<int>() << std::endl;
    } else if (name == "speed") {
        std::cout << " = " << value.cast<float>() << std::endl;
    }
});
```

### 3.1 简单类型的偏移量计算（不使用 offsetof）

对于不包含虚函数和位域的简单类型，可以使用 `alignof` 和 `sizeof` 直接计算字段偏移量，避免使用非标准扩展：

```cpp
#include "base/any_object.hpp"
#include <iostream>
#include <cstdint>

// 简单的 POD 结构体，无虚函数和位域
struct Point2D
{
    int32_t x;      // 第一个字段，偏移量为 0
    int32_t y;      // 第二个字段，偏移量为 sizeof(int32_t)
    double z;       // 第三个字段，需要考虑对齐
};

namespace HsBa::Slicer::Utils
{
    template<>
    TypeInfo* GetTypeInfo<Point2D>()
    {
        static TypeInfo info;
        info.Name = "Point2D";
        info.destroy = [](void* data) { delete static_cast<Point2D*>(data); };
        info.copy = [](const void* data) -> void* { 
            return new Point2D(*static_cast<const Point2D*>(data)); 
        };
        info.move = [](void* data) -> void* { 
            return new Point2D(std::move(*static_cast<Point2D*>(data))); 
        };
        
        // 使用 alignof 和 sizeof 计算偏移量，不使用 offsetof
        info.fields.clear();
        
        // x 字段：第一个字段，偏移量为 0
        info.fields.emplace("x", std::make_pair(GetTypeInfo<int32_t>(), 0));
        
        // y 字段：在 x 之后，偏移量为 sizeof(int32_t)
        info.fields.emplace("y", std::make_pair(GetTypeInfo<int32_t>(), sizeof(int32_t)));
        
        // z 字段：需要计算前面字段的总大小并考虑对齐
        constexpr size_t first_two_fields_size = sizeof(int32_t) + sizeof(int32_t);
        constexpr size_t alignment_of_double = alignof(double);
        // 计算对齐后的偏移量
        constexpr size_t offset_z = (first_two_fields_size + alignment_of_double - 1) / alignment_of_double * alignment_of_double;
        
        info.fields.emplace("z", std::make_pair(GetTypeInfo<double>(), offset_z));
        
        info.methods.clear();
        
        return &info;
    }
}

// 使用示例
Point2D point{10, 20, 30.5};
HsBa::Slicer::Utils::AnyObject obj(point);

// 访问字段
obj.ForeachField([&](std::string_view name, AnyObject value) {
    if (name == "x") {
        std::cout << "x = " << value.cast<int32_t>() << std::endl;  // 输出：x = 10
    } else if (name == "y") {
        std::cout << "y = " << value.cast<int32_t>() << std::endl;  // 输出：y = 20
    } else if (name == "z") {
        std::cout << "z = " << value.cast<double>() << std::endl;  // 输出：z = 30.5
    }
});
```

**注意**：此方法仅适用于简单的 POD 类型（无虚函数、无位域、无继承）。对于复杂类型，仍需使用 `offsetof` 宏。

### 4. 方法调用

使用 `Invoke` 方法动态调用对象的成员函数：

```cpp
#include "base/any_object.hpp"
#include <iostream>

Player player{100, 5.5f};
HsBa::Slicer::Utils::AnyObject obj(player);

// 准备参数
AnyObject args[] = { AnyObject(20) };

// 调用 AddHealth 方法
AnyObject result = obj.Invoke("AddHealth", args);
std::cout << "New health: " << result.cast<int>() << std::endl;  // 输出：New health: 120
```

### 4.1 注册非成员函数

除了成员函数，还可以注册自由函数、静态成员函数等非成员函数。这需要手动编写 lambda 包装器：

```cpp
#include "base/any_object.hpp"
#include <iostream>

struct Math
{
    int value;
    Math(int v = 0) : value(v) {}
    
    // 成员函数
    int Add(int x) const { return value + x; }
};

// 自由函数
int Multiply(Math& self, int factor) {
    return self.value * factor;
}

// 静态成员函数
struct Calculator
{
    static int Power(int base, int exp) {
        int result = 1;
        for (int i = 0; i < exp; ++i) result *= base;
        return result;
    }
};

namespace HsBa::Slicer::Utils
{
    template<>
    TypeInfo* GetTypeInfo<Math>()
    {
        static TypeInfo info;
        info.Name = "Math";
        info.destroy = [](void* data) { delete static_cast<Math*>(data); };
        info.copy = [](const void* data) -> void* { 
            return new Math(*static_cast<const Math*>(data)); 
        };
        info.move = [](void* data) -> void* { 
            return new Math(std::move(*static_cast<Math*>(data))); 
        };
        
        info.fields.clear();
        // 不注册字段也是可以的
        
        info.methods.clear();
        // 注册成员函数（使用 type_ensure）
        info.methods.emplace("Add", type_ensure<&Math::Add>());
        
        // 注册自由函数（手动 lambda 包装）
        info.methods.emplace("Multiply", +[](void* obj, std::span<AnyObject> args) -> AnyObject {
            auto self = static_cast<Math*>(obj);
            if (args.size() != 1 || !args[0].get_type_info() || args[0].get_type_info()->Name != "int")
            {
                throw RuntimeError("Math::Multiply expects one int argument");
            }
            int factor = args[0].cast<int>();
            return AnyObject{ Multiply(*self, factor) };
        });
        
        return &info;
    }
    
    template<>
    TypeInfo* GetTypeInfo<Calculator>()
    {
        static TypeInfo info;
        info.Name = "Calculator";
        info.destroy = [](void* data) { delete static_cast<Calculator*>(data); };
        info.copy = [](const void* data) -> void* { 
            return new Calculator(*static_cast<const Calculator*>(data)); 
        };
        info.move = [](void* data) -> void* { 
            return new Calculator(std::move(*static_cast<Calculator*>(data))); 
        };
        
        info.fields.clear();
        info.methods.clear();
        
        // 注册静态成员函数（不需要对象实例，但需要传入任意对象作为占位）
        info.methods.emplace("Power", +[](void* obj, std::span<AnyObject> args) -> AnyObject {
            if (args.size() != 2 || 
                !args[0].get_type_info() || args[0].get_type_info()->Name != "int" ||
                !args[1].get_type_info() || args[1].get_type_info()->Name != "int")
            {
                throw RuntimeError("Calculator::Power expects two int arguments");
            }
            int base = args[0].cast<int>();
            int exp = args[1].cast<int>();
            return AnyObject{ Calculator::Power(base, exp) };
        });
        
        return &info;
    }
}

// 使用示例
Math math{10};
HsBa::Slicer::Utils::AnyObject obj(math);

// 调用成员函数
AnyObject args1[] = { AnyObject(5) };
AnyObject result1 = obj.Invoke("Add", args1);
std::cout << "Add: " << result1.cast<int>() << std::endl;  // 输出：Add: 15

// 调用自由函数
AnyObject args2[] = { AnyObject(3) };
AnyObject result2 = obj.Invoke("Multiply", args2);
std::cout << "Multiply: " << result2.cast<int>() << std::endl;  // 输出：Multiply: 30

// 调用静态成员函数
Calculator calc;
HsBa::Slicer::Utils::AnyObject obj_calc(calc);
AnyObject args3[] = { AnyObject(2), AnyObject(8) };
AnyObject result3 = obj_calc.Invoke("Power", args3);
std::cout << "Power: " << result3.cast<int>() << std::endl;  // 输出：Power: 256
```

**注意**：
- 成员函数可以使用 `type_ensure` 辅助模板自动包装
- 自由函数和静态成员函数需要手动编写 lambda 包装器
- lambda 包装器的签名必须为 `AnyObject(void*, std::span<AnyObject>)`
- 第一个参数是对象指针（静态函数可忽略），第二个参数是参数列表

### 5. 支持虚函数

AnyObject 支持带有虚函数的类：

```cpp
#include "base/any_object.hpp"
#include <iostream>

struct Base
{
    int value;
    Base(int v = 0) : value(v) {}
    virtual int GetValue() const { return value; }
    virtual ~BaseVirtual() = default;
};

struct Derived : Base
{
    Derived(int v = 0) : Base(v) {}
    int GetValue() const override { return value + 1; }
};

namespace HsBa::Slicer::Utils
{
    template<>
    TypeInfo* GetTypeInfo<Base>()
    {
        static TypeInfo info;
        info.Name = "Base";
        info.destroy = [](void* data) { delete static_cast<Base*>(data); };
        info.copy = [](const void* data) -> void* { 
            return new Base(*static_cast<const Base*>(data)); 
        };
        info.move = [](void* data) -> void* { 
            return new Base(std::move(*static_cast<Base*>(data))); 
        };
        info.fields.clear();
        info.fields.emplace("value", std::make_pair(GetTypeInfo<int>(), offsetof(Base, value)));
        info.methods.clear();
        info.methods.emplace("GetValue", type_ensure<&Base::GetValue>());
        return &info;
    }
    
    template<>
    TypeInfo* GetTypeInfo<Derived>()
    {
        static TypeInfo info;
        info.Name = "Derived";
        info.destroy = [](void* data) { delete static_cast<Derived*>(data); };
        info.copy = [](const void* data) -> void* { 
            return new Derived(*static_cast<const Derived*>(data)); 
        };
        info.move = [](void* data) -> void* { 
            return new Derived(std::move(*static_cast<Derived*>(data))); 
        };
        info.fields.clear();
        info.fields.emplace("value", std::make_pair(GetTypeInfo<int>(), offsetof(Derived, value)));
        info.methods.clear();
        info.methods.emplace("GetValue", type_ensure<&Derived::GetValue>());
        return &info;
    }
}

// 使用示例
Derived derived{10};
HsBa::Slicer::Utils::AnyObject obj(derived);

AnyObject args[] = {};
AnyObject result = obj.Invoke("GetValue", args);
std::cout << "Value: " << result.cast<int>() << std::endl;  // 输出：Value: 11 (调用的是派生类的虚函数)
```

### 6. 支持位域

AnyObject 支持包含位域的类：

```cpp
#include "base/any_object.hpp"
#include <iostream>

struct BitfieldClass
{
    int value;
    unsigned flags : 3;  // 3 位位域
    
    BitfieldClass(int v = 0, unsigned f = 0) : value(v), flags(f) {}
    int Add(int x) const { return value + x; }
};

namespace HsBa::Slicer::Utils
{
    template<>
    TypeInfo* GetTypeInfo<BitfieldClass>()
    {
        static TypeInfo info;
        info.Name = "BitfieldClass";
        info.destroy = [](void* data) { delete static_cast<BitfieldClass*>(data); };
        info.copy = [](const void* data) -> void* { 
            return new BitfieldClass(*static_cast<const BitfieldClass*>(data)); 
        };
        info.move = [](void* data) -> void* { 
            return new BitfieldClass(std::move(*static_cast<BitfieldClass*>(data))); 
        };
        info.fields.clear();
        info.fields.emplace("value", std::make_pair(GetTypeInfo<int>(), offsetof(BitfieldClass, value)));
        info.methods.clear();
        info.methods.emplace("Add", type_ensure<&BitfieldClass::Add>());
        return &info;
    }
}

// 使用示例
BitfieldClass obj{10, 5};
HsBa::Slicer::Utils::AnyObject any_obj(obj);

// 访问普通字段（位域字段不会被访问）
any_obj.ForeachField([&](std::string_view name, AnyObject value) {
    if (name == "value") {
        std::cout << "Value: " << value.cast<int>() << std::endl;  // 输出：Value: 10
    }
});

// 调用方法
AnyObject args[] = { AnyObject(2) };
AnyObject result = any_obj.Invoke("Add", args);
std::cout << "Result: " << result.cast<int>() << std::endl;  // 输出：Result: 12
```

### 7. 完整示例

```cpp
#include "base/any_object.hpp"
#include <iostream>
#include <string>

// 定义一个简单的类
struct Person
{
    std::string name;
    int age;
    
    std::string Greet(const std::string& greeting) const {
        return greeting + ", I'm " + name + " and I'm " + std::to_string(age) + " years old.";
    }
};

// 注册类型信息
namespace HsBa::Slicer::Utils
{
    template<>
    TypeInfo* GetTypeInfo<Person>()
    {
        static TypeInfo info;
        info.Name = "Person";
        info.destroy = [](void* data) { delete static_cast<Person*>(data); };
        info.copy = [](const void* data) -> void* { 
            return new Person(*static_cast<const Person*>(data)); 
        };
        info.move = [](void* data) -> void* { 
            return new Person(std::move(*static_cast<Person*>(data))); 
        };
        
        info.fields.clear();
        info.fields.emplace("name", std::make_pair(GetTypeInfo<std::string>(), offsetof(Person, name)));
        info.fields.emplace("age", std::make_pair(GetTypeInfo<int>(), offsetof(Person, age)));
        
        info.methods.clear();
        info.methods.emplace("Greet", type_ensure<&Person::Greet>());
        
        return &info;
    }
}

int main()
{
    // 创建对象
    Person person{"Alice", 30};
    HsBa::Slicer::Utils::AnyObject obj(person);
    
    // 遍历字段
    std::cout << "Person fields:" << std::endl;
    obj.ForeachField([&](std::string_view name, AnyObject value) {
        if (name == "name") {
            std::cout << "  Name: " << value.cast<std::string>() << std::endl;
        } else if (name == "age") {
            std::cout << "  Age: " << value.cast<int>() << std::endl;
        }
    });
    
    // 调用方法
    AnyObject args[] = { AnyObject(std::string("Hello")) };
    AnyObject result = obj.Invoke("Greet", args);
    std::cout << "\nGreeting: " << result.cast<std::string>() << std::endl;
    
    // 测试拷贝和移动
    HsBa::Slicer::Utils::AnyObject copied = obj;
    std::cout << "\nCopied person name: " 
              << copied.cast<Person>().name << std::endl;
    
    return 0;
}
```

## 注意事项

- 需要为每个自定义类型特化 `GetTypeInfo` 函数
- 类型信息必须是静态的，以确保生命周期
- 字段的访问顺序不保证与定义顺序相同
- 位域字段不会被包含在字段列表中
- 虚函数表指针不会被当作字段处理（这是 C++ 实现细节，不同编译器可能有不同的实现方式，如 Itanium C++ ABI、Microsoft C++ ABI 或其他标准许可的实现）
- 对于包含虚函数或位域的类，通常需要使用非 C++ 标准的 `offsetof` 宏获取字段偏移量；对于其他简单情况，建议直接使用字段大小和对齐要求计算偏移量，以避免使用非标准扩展
- 所有方法调用都需要正确的参数类型匹配
- 不支持私有或受保护的成员访问（需要使用友元或公开成员）
- 类型信息中的名称必须与调用时使用的名称完全匹配
- 确保在使用 AnyObject 之前已经注册了相应的类型信息
- 对于包含引用的类型，需要特殊处理以确保安全性

## any_object.hpp 特化说明

`any_object.hpp` 头文件中已经为以下标准类型提供了特化的 `GetTypeInfo`：

### 已特化的标准类型

1. **`std::string`**
   - 注册方法：`size()`, `c_str()`, `at(size_t)`
   - 无需手动注册即可直接使用

2. **`std::string_view`**
   - 注册方法：`size()`, `data()`, `at(size_t)`
   - 无需手动注册即可直接使用

### 内置类型

以下内置类型使用默认的 `GetTypeInfo` 模板，自动支持基本操作：
- 所有算术类型（`int`, `float`, `double`, `size_t` 等）
- 所有 POD 类型
- 所有可拷贝/可移动的用户自定义类型

### 特化建议

- **标准容器**：如需支持 `std::vector`, `std::map` 等容器，需要手动特化
- **智能指针**：如需支持 `std::unique_ptr`, `std::shared_ptr`，需要手动特化并注意所有权管理
- **枚举类型**：建议特化以提供更好的类型安全
- **复杂业务类型**：根据运行时需求选择性注册字段和方法

### 最小化特化示例

如果只需要基本的存储和类型转换功能，可以不注册任何字段和方法：

```cpp
namespace HsBa::Slicer::Utils
{
    template<>
    TypeInfo* GetTypeInfo<MyType>()
    {
        static TypeInfo info;
        info.Name = "MyType";
        info.destroy = [](void* data) { delete static_cast<MyType*>(data); };
        info.copy = [](const void* data) -> void* { 
            return new MyType(*static_cast<const MyType*>(data)); 
        };
        info.move = [](void* data) -> void* { 
            return new MyType(std::move(*static_cast<MyType*>(data))); 
        };
        
        // 不注册任何字段和方法
        info.fields.clear();
        info.methods.clear();
        
        return &info;
    }
}
```

这种最小化特化适用于：
- 仅需类型擦除和运行时类型识别
- 通过其他方式（如虚函数）进行动态调度
- 性能敏感场景，减少反射开销
