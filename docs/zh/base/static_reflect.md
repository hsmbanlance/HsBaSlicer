# Static Reflect (静态反射)

Static Reflect 组件提供了编译时的类型反射功能，允许在不使用运行时类型信息 (RTTI) 的情况下检查和操作类型成员。

## 功能特点

- 编译时类型反射
- 字段和方法信息提取
- 类型安全的成员访问
- 模板字符串支持
- 概念约束检查

## 使用方法

### 1. 使类可反射

要使用静态反射功能，需要在类中定义 `FieldList`、`MethodList` 和 `ClassName`：

```cpp
#include "base/static_reflect.hpp"

using namespace HsBa::Slicer::Utils::TemplateStringLiterals;

class Player
{
private:
    int health;
    float speed;

public:
    Player() : health{100}, speed{0.1f} {}
    
    void TakeDamage(int damage) {
        health -= damage;
    }
    
    int Heal(int amount) {
        health += amount;
        return health;
    }

    // 定义字段列表
    using FieldList = std::tuple <
        HsBa::Slicer::Utils::StaticReflect::FieldInfo<Player, int, "health"_ts, &Player::health>,
        HsBa::Slicer::Utils::StaticReflect::FieldInfo<Player, float, "speed"_ts, &Player::speed>
    >;
    
    // 定义方法列表
    using MethodList = std::tuple <
        HsBa::Slicer::Utils::StaticReflect::MethodInfo<Player, void(int), "TakeDamage"_ts, &Player::TakeDamage>,
        HsBa::Slicer::Utils::StaticReflect::MethodInfo<Player, int(int), "Heal"_ts, &Player::Heal>
    >;
    
    // 定义类名
    constexpr static auto ClassName = "Player"_ts;
};
```

### 2. 使用 Reflector

通过 Reflector 类访问反射信息：

```cpp
#include "base/static_reflect.hpp"

// 使用反射器访问 Player 类的信息
using PlayerReflect = HsBa::Slicer::Utils::StaticReflect::Reflector<Player>;

// 获取类名
std::cout << "Class name: " << PlayerReflect::ClassName() << std::endl;

// 获取字段数量
std::cout << "Field count: " << PlayerReflect::FieldCount() << std::endl;

// 获取方法数量
std::cout << "Method count: " << PlayerReflect::MethodCount() << std::endl;

// 创建对象并访问字段
Player player;
auto health = PlayerReflect::GetField<0>(player);  // 获取 health 字段
auto speed = PlayerReflect::GetField<1>(player);   // 获取 speed 字段

std::cout << "Health: " << health << ", Speed: " << speed << std::endl;
```

### 3. 遍历字段和方法

```cpp
#include "base/static_reflect.hpp"
#include "base/tuple_each.hpp"
#include <iostream>

Player player;

// 遍历所有字段并打印信息
std::cout << "Fields:" << std::endl;
for (size_t i = 0; i < PlayerReflect::FieldCount(); ++i) {
    std::cout << "  Field " << i << ": " << PlayerReflect::FieldName<0>() << std::endl;
}

// 使用 TupleEach 遍历字段列表
HsBa::Slicer::Utils::TupleEach(Player::FieldList{}, [](auto field_info) {
    std::cout << "Field name: " << field_info.GetName() << std::endl;
});
```

### 4. 方法调用

```cpp
#include "base/static_reflect.hpp"

Player player;

// 通过反射调用方法
// 注意：目前需要使用具体的方法名进行调用
auto& health_field = PlayerReflect::GetField<0>(player);
health_field = 80;  // 直接修改 health 字段

// 调用 Heal 方法
int new_health = PlayerReflect::InvokeMemberFunction<"Heal"_ts>(player, 10);
std::cout << "New health after healing: " << new_health << std::endl;
```

### 5. 创建通用反射处理函数

```cpp
#include "base/static_reflect.hpp"
#include "base/tuple_each.hpp"
#include <iostream>

// 创建一个通用的序列化函数
template<typename T>
    requires HsBa::Slicer::Utils::StaticReflect::Reflectable<T>
std::string Serialize(const T& obj) {
    using Reflector = HsBa::Slicer::Utils::StaticReflect::Reflector<T>;
    std::string result = "{";
    
    // 遍历所有字段并序列化
    for (size_t i = 0; i < Reflector::FieldCount(); ++i) {
        if (i > 0) result += ", ";
        
        // 获取字段名和值（这里简化处理，实际需要根据类型转换）
        result += std::string(Reflector::FieldName<i>());
        result += ": ";
        
        // 由于模板限制，这里不能直接获取字段值，需要特殊处理
        result += "value";
    }
    
    result += "}";
    return result;
}

// 使用示例
Player player;
std::string serialized = Serialize(player);
std::cout << "Serialized: " << serialized << std::endl;
```

### 6. 完整示例

```cpp
#include "base/static_reflect.hpp"
#include <iostream>

using namespace HsBa::Slicer::Utils::TemplateStringLiterals;

class Config
{
private:
    int max_connections;
    std::string server_name;
    bool enable_logging;

public:
    Config() : max_connections(100), server_name("localhost"), enable_logging(true) {}
    
    void SetMaxConnections(int n) { max_connections = n; }
    void SetServerName(const std::string& name) { server_name = name; }
    void ToggleLogging() { enable_logging = !enable_logging; }

    using FieldList = std::tuple <
        HsBa::Slicer::Utils::StaticReflect::FieldInfo<Config, int, "max_connections"_ts, &Config::max_connections>,
        HsBa::Slicer::Utils::StaticReflect::FieldInfo<Config, std::string, "server_name"_ts, &Config::server_name>,
        HsBa::Slicer::Utils::StaticReflect::FieldInfo<Config, bool, "enable_logging"_ts, &Config::enable_logging>
    >;
    
    using MethodList = std::tuple <
        HsBa::Slicer::Utils::StaticReflect::MethodInfo<Config, void(int), "SetMaxConnections"_ts, &Config::SetMaxConnections>,
        HsBa::Slicer::Utils::StaticReflect::MethodInfo<Config, void(const std::string&), "SetServerName"_ts, &Config::SetServerName>,
        HsBa::Slicer::Utils::StaticReflect::MethodInfo<Config, void(), "ToggleLogging"_ts, &Config::ToggleLogging>
    >;
    
    constexpr static auto ClassName = "Config"_ts;
};

int main() {
    using ConfigReflect = HsBa::Slicer::Utils::StaticReflect::Reflector<Config>;
    
    Config config;
    
    std::cout << "Class: " << ConfigReflect::ClassName() << std::endl;
    std::cout << "Fields: " << ConfigReflect::FieldCount() << std::endl;
    std::cout << "Methods: " << ConfigReflect::MethodCount() << std::endl;
    
    // 访问字段
    auto& max_conn = ConfigReflect::GetField<0>(config);
    auto& server_name = ConfigReflect::GetField<1>(config);
    auto& logging = ConfigReflect::GetField<2>(config);
    
    std::cout << "Initial values:" << std::endl;
    std::cout << "Max connections: " << max_conn << std::endl;
    std::cout << "Server name: " << server_name << std::endl;
    std::cout << "Logging enabled: " << logging << std::endl;
    
    // 修改字段
    max_conn = 200;
    server_name = "production";
    
    std::cout << "\nAfter modification:" << std::endl;
    std::cout << "Max connections: " << max_conn << std::endl;
    std::cout << "Server name: " << server_name << std::endl;
    
    return 0;
}
```

## 注意事项

- 要使用静态反射，类必须满足 `Reflectable` 概念
- 类需要定义 `FieldList`、`MethodList` 和 `ClassName`
- 字段和方法信息通过模板参数在编译时确定
- `FieldInfo` 和 `MethodInfo` 需要指定类类型、成员类型、名称和指针
- 模板字符串使用 `_ts` 后缀字面量，也可以手动构造（包括隐式的）模板字符串
- 静态反射完全在编译时处理，没有运行时开销