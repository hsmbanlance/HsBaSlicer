# Struct JSON (Structure JSON Serialization)

The Struct JSON component provides serialization and deserialization functionality between C++ structures and JSON, supporting aggregate types, static reflectable types, and various standard library types.

## Features

- Supports JSON serialization of aggregate types
- Supports JSON serialization of static reflectable types
- Supports various standard library types, such as ranges, optional types, enum types, etc.
- Uses the RapidJSON library for underlying JSON operations
- Supports writing/reading objects to/from files or strings
- Provides pretty-formatted JSON output

## Usage

### 1. Basic Aggregate Type Serialization

```cpp
#include "utils/struct_json.hpp"
#include <iostream>

struct Point {
    int x;
    int y;
};

// Serialize to JSON
Point p{10, 20};
std::string json_str = HsBa::Slicer::Utils::write_json(p);
std::cout << json_str << std::endl;  // Output: {"x":10,"y":20}

// Deserialize from JSON
std::string input = R"({"x":5,"y":15})";
Point p2 = HsBa::Slicer::Utils::read_json_from_string<Point>(input);
std::cout << "p2: " << p2.x << ", " << p2.y << std::endl;  // Output: p2: 5, 15
```

### 2. Static Reflection Type Serialization

```cpp
#include "utils/struct_json.hpp"
#include "base/static_reflect.hpp"

using namespace HsBa::Slicer::Utils::TemplateStringLiterals;

class Config
{
public:
    int max_connections;
    std::string server_name;
    bool enable_logging;

    using FieldList = std::tuple <
        HsBa::Slicer::Utils::StaticReflect::FieldInfo<Config, int, "max_connections"_ts, &Config::max_connections>,
        HsBa::Slicer::Utils::StaticReflect::FieldInfo<Config, std::string, "server_name"_ts, &Config::server_name>,
        HsBa::Slicer::Utils::StaticReflect::FieldInfo<Config, bool, "enable_logging"_ts, &Config::enable_logging>
    >;
    
    constexpr static auto ClassName = "Config"_ts;
};

// Serialize and deserialize reflectable types
Config cfg{100, "localhost", true};
std::string json_str = HsBa::Slicer::Utils::write_pretty_json(cfg);
std::cout << json_str << std::endl;

Config cfg2 = HsBa::Slicer::Utils::read_json_from_string<Config>(json_str);
```

### 3. Container Type Serialization

```cpp
#include "utils/struct_json.hpp"
#include <vector>
#include <optional>

struct Data {
    std::vector<int> numbers;
    std::optional<std::string> description;
};

Data d{{1, 2, 3}, "A sample data"};
std::string json_str = HsBa::Slicer::Utils::write_json(d);

Data d2 = HsBa::Slicer::Utils::read_json_from_string<Data>(json_str);
```

### 4. File Operations

```cpp
#include "utils/struct_json.hpp"

struct Settings {
    int width;
    int height;
    std::string theme;
};

Settings settings{1920, 1080, "dark"};

// Write to JSON file
HsBa::Slicer::Utils::write_json("settings.json", settings);

// Read from JSON file
Settings loaded_settings = HsBa::Slicer::Utils::read_json_from_file<Settings>("settings.json");
```

### 5. Custom RapidJSON Convertible Types

```cpp
#include "utils/struct_json.hpp"
#include <rapidjson/document.h>

// Class implementing RapidJsonValueConvertible concept
class CustomObject
{
public:
    int value;
    std::string name;

    void to_json(rapidjson::Value& json, rapidjson::Document::AllocatorType& allocator) const
    {
        json.SetObject();
        json.AddMember("value", value, allocator);
        json.AddMember("name", rapidjson::Value(name.c_str(), allocator), allocator);
    }

    static CustomObject from_json(const rapidjson::Value& json)
    {
        CustomObject obj;
        if (json.HasMember("value")) obj.value = json["value"].GetInt();
        if (json.HasMember("name")) obj.name = json["name"].GetString();
        return obj;
    }
};

// Using custom type
CustomObject obj{42, "test"};
std::string json_str = HsBa::Slicer::Utils::write_json(obj);
CustomObject obj2 = HsBa::Slicer::Utils::read_json_from_string<CustomObject>(json_str);
```

## Supported Types

- Arithmetic types (int, float, double, bool, etc.)
- String types (std::string)
- Enum types
- Aggregate types (POD structures)
- Static reflectable types
- Standard containers (std::vector, std::list, std::array, etc.)
- Optional types (std::optional)
- Custom RapidJSON convertible types

## Notes

- Aggregate types must be aggregate classes (classes that can be initialized with brace initialization)
- Static reflectable types need to implement static reflection mechanism
- Enum types need to support EnumName and EnumFromName functions
- All types must be copy constructible
- For complex nested types, ensure all subtypes support JSON serialization