# Static Reflect

The Static Reflect component provides compile-time type reflection functionality, allowing inspection and manipulation of type members without using runtime type information (RTTI).

## Features

- Compile-time type reflection
- Field and method information extraction
- Type-safe member access
- Template string support
- Concept constraint checking

## Usage

### 1. Making Classes Reflectable

To use static reflection functionality, you need to define `FieldList`, `MethodList`, and `ClassName` in your class:

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

    // Define field list
    using FieldList = std::tuple <
        HsBa::Slicer::Utils::StaticReflect::FieldInfo<Player, int, "health"_ts, &Player::health>,
        HsBa::Slicer::Utils::StaticReflect::FieldInfo<Player, float, "speed"_ts, &Player::speed>
    >;
    
    // Define method list
    using MethodList = std::tuple <
        HsBa::Slicer::Utils::StaticReflect::MethodInfo<Player, void(int), "TakeDamage"_ts, &Player::TakeDamage>,
        HsBa::Slicer::Utils::StaticReflect::MethodInfo<Player, int(int), "Heal"_ts, &Player::Heal>
    >;
    
    // Define class name
    constexpr static auto ClassName = "Player"_ts;
};
```

### 2. Using Reflector

Access reflection information through the Reflector class:

```cpp
#include "base/static_reflect.hpp"

// Use reflector to access Player class information
using PlayerReflect = HsBa::Slicer::Utils::StaticReflect::Reflector<Player>;

// Get class name
std::cout << "Class name: " << PlayerReflect::ClassName() << std::endl;

// Get field count
std::cout << "Field count: " << PlayerReflect::FieldCount() << std::endl;

// Get method count
std::cout << "Method count: " << PlayerReflect::MethodCount() << std::endl;

// Create object and access fields
Player player;
auto health = PlayerReflect::GetField<0>(player);  // Get health field
auto speed = PlayerReflect::GetField<1>(player);   // Get speed field

std::cout << "Health: " << health << ", Speed: " << speed << std::endl;
```

### 3. Iterating Fields and Methods

```cpp
#include "base/static_reflect.hpp"
#include "base/tuple_each.hpp"
#include <iostream>

Player player;

// Iterate through all fields and print information
std::cout << "Fields:" << std::endl;
for (size_t i = 0; i < PlayerReflect::FieldCount(); ++i) {
    std::cout << "  Field " << i << ": " << PlayerReflect::FieldName<0>() << std::endl;
}

// Use TupleEach to iterate through field list
HsBa::Slicer::Utils::TupleEach(Player::FieldList{}, [](auto field_info) {
    std::cout << "Field name: " << field_info.GetName() << std::endl;
});
```

### 4. Method Invocation

```cpp
#include "base/static_reflect.hpp"

Player player;

// Call methods through reflection
// Note: Currently requires using specific method names for invocation
auto& health_field = PlayerReflect::GetField<0>(player);
health_field = 80;  // Directly modify health field

// Call Heal method
int new_health = PlayerReflect::InvokeMemberFunction<"Heal"_ts>(player, 10);
std::cout << "New health after healing: " << new_health << std::endl;
```

### 5. Creating Generic Reflection Processing Functions

```cpp
#include "base/static_reflect.hpp"
#include "base/tuple_each.hpp"
#include <iostream>

// Create a generic serialization function
template<typename T>
    requires HsBa::Slicer::Utils::StaticReflect::Reflectable<T>
std::string Serialize(const T& obj) {
    using Reflector = HsBa::Slicer::Utils::StaticReflect::Reflector<T>;
    std::string result = "{";
    
    // Iterate through all fields and serialize
    for (size_t i = 0; i < Reflector::FieldCount(); ++i) {
        if (i > 0) result += ", ";
        
        // Get field name and value (simplified here, actual conversion needed by type)
        result += std::string(Reflector::FieldName<i>());
        result += ": ";
        
        // Due to template limitations, field values cannot be directly obtained here, special handling needed
        result += "value";
    }
    
    result += "}";
    return result;
}

// Usage example
Player player;
std::string serialized = Serialize(player);
std::cout << "Serialized: " << serialized << std::endl;
```

### 6. Complete Example

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
    
    // Access fields
    auto& max_conn = ConfigReflect::GetField<0>(config);
    auto& server_name = ConfigReflect::GetField<1>(config);
    auto& logging = ConfigReflect::GetField<2>(config);
    
    std::cout << "Initial values:" << std::endl;
    std::cout << "Max connections: " << max_conn << std::endl;
    std::cout << "Server name: " << server_name << std::endl;
    std::cout << "Logging enabled: " << logging << std::endl;
    
    // Modify fields
    max_conn = 200;
    server_name = "production";
    
    std::cout << "\nAfter modification:" << std::endl;
    std::cout << "Max connections: " << max_conn << std::endl;
    std::cout << "Server name: " << server_name << std::endl;
    
    return 0;
}
```

## Notes

- To use static reflection, classes must satisfy the `Reflectable` concept
- Classes need to define `FieldList`, `MethodList`, and `ClassName`
- Field and method information is determined at compile time through template parameters
- `FieldInfo` and `MethodInfo` need to specify class type, member type, name, and pointer
- Template strings use the `_ts` suffix literal, or can be constructed manually (including implicitly)
- Static reflection is completely processed at compile time, with no runtime overhead