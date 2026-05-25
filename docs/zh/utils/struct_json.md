# Struct JSON (结构体 JSON 序列化)

Struct JSON 组件提供了 C++ 结构体与 JSON 之间的序列化和反序列化功能，支持聚合类型、静态反射类型以及各种标准库类型。

## 功能特点

- 支持聚合类型 (Aggregate Types) 的 JSON 序列化
- 支持静态反射类型 (Static Reflectable Types) 的 JSON 序列化
- 支持各种标准库类型，如范围 (Ranges)、可选类型 (Optional)、枚举类型等
- 使用 RapidJSON 库进行底层 JSON 操作
- 支持将对象写入/读取文件或字符串
- 提供美观格式化的 JSON 输出

## 使用方法

### 1. 基本聚合类型序列化

```cpp
#include "utils/struct_json.hpp"
#include <iostream>

struct Point {
    int x;
    int y;
};

// 序列化为 JSON
Point p{10, 20};
std::string json_str = HsBa::Slicer::Utils::write_json(p);
std::cout << json_str << std::endl;  // 输出: {"x":10,"y":20}

// 从 JSON 反序列化
std::string input = R"({"x":5,"y":15})";
Point p2 = HsBa::Slicer::Utils::read_json_from_string<Point>(input);
std::cout << "p2: " << p2.x << ", " << p2.y << std::endl;  // 输出: p2: 5, 15
```

### 2. 静态反射类型序列化

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

// 序列化和反序列化反射类型
Config cfg{100, "localhost", true};
std::string json_str = HsBa::Slicer::Utils::write_pretty_json(cfg);
std::cout << json_str << std::endl;

Config cfg2 = HsBa::Slicer::Utils::read_json_from_string<Config>(json_str);
```

### 3. 容器类型序列化

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

### 4. 文件操作

```cpp
#include "utils/struct_json.hpp"

struct Settings {
    int width;
    int height;
    std::string theme;
};

Settings settings{1920, 1080, "dark"};

// 写入 JSON 文件
HsBa::Slicer::Utils::write_json("settings.json", settings);

// 从 JSON 文件读取
Settings loaded_settings = HsBa::Slicer::Utils::read_json_from_file<Settings>("settings.json");
```

### 5. 自定义 RapidJSON 可转换类型

```cpp
#include "utils/struct_json.hpp"
#include <rapidjson/document.h>

// 实现 RapidJsonValueConvertible 概念的类
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

// 使用自定义类型
CustomObject obj{42, "test"};
std::string json_str = HsBa::Slicer::Utils::write_json(obj);
CustomObject obj2 = HsBa::Slicer::Utils::read_json_from_string<CustomObject>(json_str);
```

## 支持的类型

- 算术类型 (int, float, double, bool 等)
- 字符串类型 (std::string)
- 枚举类型
- 聚合类型 (POD 结构体)
- 静态反射类型
- 标准容器 (std::vector, std::list, std::array 等)
- 可选类型 (std::optional)
- 自定义 RapidJSON 可转换类型

## 注意事项

- 聚合类型必须是聚合类 (aggregate class)，即可以通过大括号初始化的类
- 静态反射类型需要实现静态反射机制
- 枚举类型需要支持 EnumName 和 EnumFromName 函数
- 所有类型必须是可复制构造的
- 对于复杂嵌套类型，确保所有子类型都支持 JSON 序列化