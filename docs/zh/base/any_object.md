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
- Mockit 风格的测试 Mock 支持（仅在检测到常用测试宏时启用，可运行时手动开关）

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

### 8. Mock 支持（测试专用）

`AnyObject::Invoke` 内置了 Mockit 风格的方法打桩能力，便于在单元测试中隔离被测代码与其依赖。为了避免污染生产构建，所有 Mock 相关代码由 `HSBA_ANY_OBJECT_ENABLE_MOCK` 宏整体门控。

**启用条件（满足任一即可）**：

- 显式定义 `HSBA_ANY_OBJECT_ENABLE_MOCK=1`
- 当前编译单元中出现下列任一常用测试框架宏：
  - Boost.Test：`BOOST_TEST_MODULE`、`BOOST_TEST_INCLUDED`、`BOOST_TEST_DYN_LINK`、`BOOST_TEST_ALTERNATIVE_INIT_API`
  - GoogleTest：`GTEST_INCLUDE_GTEST_GTEST_H_`、`GTEST_API_`、`GTEST_HAS_MOCK`
  - Catch2：`CATCH_VERSION_MAJOR`、`CATCH_CONFIG_MAIN`、`CATCH_CONFIG_RUNNER`
  - doctest：`DOCTEST_LIBRARY_INCLUDED`、`DOCTEST_CONFIG_IMPLEMENT`、`DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN`
- 项目层：当 `HSBA_SLICER_USE_TESTS=ON` 时，CMake 会为 `HsBaSlicerBase` 追加 `PUBLIC HSBA_ANY_OBJECT_ENABLE_MOCK=1`，链接它的测试目标无需重复设置。

**运行时开关**：即便编译进来，Mock 默认关闭，`AnyObject::Invoke` 走真实方法路径，无任何行为变化。可通过下列 API 手动开启或关闭：

| API | 说明 |
| --- | --- |
| `Mock::EnableMock()` / `MockRegistry::instance().enable()` | 手动开启 Mock |
| `Mock::DisableMock()` / `MockRegistry::instance().disable()` | 手动关闭 Mock（保留已注册的桩） |
| `Mock::SetMockEnabled(bool)` | 显式设置开关 |
| `Mock::IsMockEnabled()` | 查询当前状态 |
| `Mock::ClearMocks()` / `MockRegistry::clear()` | 清空所有桩与调用记录 |
| `Mock::MockRegistry::ScopedEnable` | RAII：作用域内开启，退出时恢复先前状态 |
| `Mock::MockRegistry::ScopedStubs` | RAII：进入时清空并开启，退出时清空并恢复；用于测试用例之间的隔离 |

**打桩 API**：桩以 `(TypeInfo*, method_name)` 为键。

- `stub<T>(method_name, fn)`：安装自定义桩函数，签名 `AnyObject(void* self, std::span<AnyObject> args)`
- `stub_return<T>(method_name, value)`：安装固定返回值桩
- `stub_throw<T>(method_name, eptr)`：安装总是重抛 `std::exception_ptr` 的桩
- `stub_throw<T, E, Args...>(method_name, args...)`：安装每次构造 `E(args...)` 并抛出的桩
- `add_rule<T>(method_name, predicate, action)`：新增一条**依赖实参**的规则（谓词匹配时才生效）
- `when_called<T>(method_name, action)`：等价于 `add_rule<T>(name, AnyArgs(), action)`
- `unstub<T>(method_name)` / `clear<T>()`：移除单个桩或某个类型下的全部桩与规则
- `clear_rules<T>(method_name)`：仅清除规则，保留普通桩
- `rule_count<T>(method_name)`：查询当前规则数量
- `call_count<T>(method_name)`：查询已记录的调用次数
- `calls<T>(method_name)`：获取调用记录（含 `method_name` 与参数快照）
- `reset_calls()`：清空调用记录但保留桩与规则

**优先级**：`AnyObject::Invoke` 首先按插入顺序依次评估当前 (类型, 方法) 下的所有规则，**首个谓词命中的规则**接管调用；若无规则命中，则回退到普通桩（`stub*` 系列）；仍无匹配时执行真实方法。只有**实际被 Mock 接管**的调用才会被记录到 `calls<T>()`。

所有桩与调用记录均由内部 `std::mutex` 保护，可在并行测试中安全使用。为避免与用户回调相互锁定，谓词与动作均在锁外执行。

**动作工厂 (`Mock::`)**：

| 工厂 | 作用 |
| --- | --- |
| `Return(value)` | 返回一个包装为 `AnyObject` 的固定值 |
| `Throw(std::exception_ptr)` | 重抛给定的异常指针 |
| `ThrowOf<E, Args...>(args...)` | 每次构造 `E(args...)` 并抛出 |
| `Do(callable)` | 包装任意 `AnyObject(void*, std::span<AnyObject>)` 可调用对象（可自由读参、访问 `self`、抛异常） |

**谓词工厂 (`Mock::`)**：

| 工厂 | 作用 |
| --- | --- |
| `AnyArgs()` | 总是匹配 |
| `ArgCountIs(n)` | 实参个数等于 `n` |
| `ArgAtIs<V>(i, expected)` | `args[i]` 能 `cast<V>()` 且与 `expected` 相等；类型不匹配时返回 false而非抛异常 |
| `ArgAtMatches<V>(i, unary_pred)` | `args[i].cast<V>()` 满足 `unary_pred` |
| `AllOf(ps...)` / `AnyOf(ps...)` / `Not(p)` | 谓词的与/或/非组合 |

**示例**：

```cpp
#include "base/any_object.hpp"

using namespace HsBa::Slicer::Utils;

BOOST_AUTO_TEST_CASE(player_add_health_mocked)
{
    Player p{100, 1.0f};
    AnyObject obj(p);
    AnyObject args[] = {AnyObject(50)};

    // 默认关闭：真实方法生效
    BOOST_CHECK_EQUAL(obj.Invoke("AddHealth", args).cast<int>(), 150);

    {
        // 进入作用域：清空 + 开启 Mock，并注册固定返回值桩
        Mock::MockRegistry::ScopedStubs scope;
        scope.registry().stub_return<Player>("AddHealth", 999);

        BOOST_CHECK_EQUAL(obj.Invoke("AddHealth", args).cast<int>(), 999);
        BOOST_CHECK_EQUAL(scope.registry().call_count<Player>("AddHealth"), 1u);

        auto records = scope.registry().calls<Player>("AddHealth");
        BOOST_REQUIRE_EQUAL(records.size(), 1u);
        BOOST_CHECK_EQUAL(records[0].args[0].cast<int>(), 50);
    }
    // 离开作用域：桩已清理、Mock 已关闭，行为恢复真实方法
    BOOST_CHECK(!Mock::IsMockEnabled());
    BOOST_CHECK_EQUAL(obj.Invoke("AddHealth", args).cast<int>(), 200);
}
```

**抛异常桩**：

```cpp
using namespace HsBa::Slicer::Utils;
Mock::MockRegistry::ScopedStubs scope;

// 方式 A：直接传入 std::exception_ptr
scope.registry().stub_throw<Player>("AddHealth",
    std::make_exception_ptr(std::runtime_error("db down")));

// 方式 B：每次构造并抛出指定异常类型
scope.registry().stub_throw<Player, std::logic_error>("AddHealth", "invalid state");

// 方式 C：自由函数快捷
Mock::StubThrow<Player, std::out_of_range>("AddHealth", "oor");

BOOST_CHECK_THROW(obj.Invoke("AddHealth", args), std::out_of_range);
// 抛出的调用仍会记录到 calls<Player>("AddHealth")
```

**依赖实参的规则桩**：

```cpp
using namespace HsBa::Slicer::Utils;
Mock::MockRegistry::ScopedStubs scope;
auto& reg = scope.registry();

// 规则 1：实参为 0 时抛异常
reg.add_rule<Player>("AddHealth",
    Mock::ArgAtIs<int>(0, 0),
    Mock::ThrowOf<std::runtime_error>("zero not allowed"));

// 规则 2：实参 > 100 时返回固定值
reg.add_rule<Player>("AddHealth",
    Mock::ArgAtMatches<int>(0, [](const int& v) { return v > 100; }),
    Mock::Return<int>(-1));

// 普通桩作为兑底：自定义依赖 self 与实参的行为
reg.stub<Player>("AddHealth", [](void* self, std::span<AnyObject> a) -> AnyObject {
    auto* p = static_cast<Player*>(self);
    p->health += a[0].cast<int>();
    return AnyObject{p->health};
});

AnyObject zero[] = {AnyObject(0)};
AnyObject big[]  = {AnyObject(500)};
AnyObject mid[]  = {AnyObject(20)};

BOOST_CHECK_THROW(obj.Invoke("AddHealth", zero), std::runtime_error); // 命中规则 1
BOOST_CHECK_EQUAL(obj.Invoke("AddHealth", big).cast<int>(), -1);       // 命中规则 2
BOOST_CHECK_EQUAL(obj.Invoke("AddHealth", mid).cast<int>(), 120);      // 回退到普通桩
```

**组合谓词**：`AllOf` / `AnyOf` / `Not` 可以拼接出任意复杂的实参匹配条件：

```cpp
reg.add_rule<Player>("AddHealth",
    Mock::AllOf(Mock::ArgCountIs(1u),
                Mock::Not(Mock::ArgAtIs<int>(0, 0))),
    Mock::Return<int>(42));
```

**注意事项**：

- `MockRegistry` 是全局单例，务必使用 `ScopedStubs`，或在测试夹具的 `setup/teardown` 中显式调用 `ClearMocks()`，避免用例之间串扰。
- 桩函数的第一个参数是原对象的 `void* self`，可安全地 `static_cast` 回原类型以读写其状态。
- `calls<T>(name)` 返回的 `CallRecord::args` 通过 `AnyObject` 的拷贝构造保存参数快照，参数类型必须支持拷贝。
- **抛异常仍会记录调用**：即使动作抛出，`CallRecord` 已在派发前写入，因此可以在断言异常后继续校验调用次数。
- **规则优先于普通桩**：同一 (类型, 方法) 下同时存在规则和 `stub*` 时，先按插入顺序逐条评估规则，均不命中时才回退到普通桩。
- `ArgAtIs` / `ArgAtMatches` 在实参个数不足或类型不匹配时**返回 false而不抛异常**，便于安全地写宽松谓词。
- 未启用 `HSBA_ANY_OBJECT_ENABLE_MOCK` 时，`Mock` 命名空间不存在，`Invoke` 也没有任何额外开销。

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
