# Any Visit (任意类型访问)

Any Visit 组件提供了对 std::any 和 boost::any 类型安全访问的功能，允许在运行时对任意类型执行类型匹配和操作。

## 功能特点

- 类型安全的 std::any 和 boost::any 访问
- 支持多种类型列表的匹配
- 编译时类型检查
- 支持带参数的回调函数
- 返回值类型一致性检查

## 使用方法

### 1. 基本用法

```cpp
#include "base/any_visit.hpp"
#include <any>
#include <iostream>

// 创建一个 std::any 对象
std::any value = 42;

// 使用 Visit 访问 any 中的值
auto result = HsBa::Slicer::Utils::Visit<int, double, std::string>(
    [](auto&& val) -> std::string {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, int>) {
            return "Integer: " + std::to_string(val);
        } else if constexpr (std::is_same_v<T, double>) {
            return "Double: " + std::to_string(val);
        } else if constexpr (std::is_same_v<T, std::string>) {
            return "String: " + val;
        }
    },
    value
);

std::cout << result << std::endl;  // 输出: Integer: 42
```

### 2. 使用 boost::any

```cpp
#include "base/any_visit.hpp"
#include <boost/any.hpp>
#include <iostream>

// 创建一个 boost::any 对象
boost::any value = std::string("Hello World");

// 使用 Visit 访问 any 中的值
auto result = HsBa::Slicer::Utils::Visit<int, double, std::string>(
    [](auto&& val) -> std::string {
        return std::string("Value: ") + val;
    },
    value
);

std::cout << result << std::endl;  // 输出: Value: Hello World
```

### 3. 带参数的访问

```cpp
#include "base/any_visit.hpp"
#include <any>

std::any value = 100;

// 使用带额外参数的访问
auto result = HsBa::Slicer::Utils::Visit<int, double>(
    [](auto&& val, int multiplier) -> int {
        return static_cast<int>(val * multiplier);
    },
    value,
    5  // 额外参数
);

std::cout << result << std::endl;  // 输出: 500
```

### 4. void 返回类型

当回调函数返回 void 时，Visit 也返回 void：

```cpp
#include "base/any_visit.hpp"
#include <any>
#include <iostream>

std::any value = 3.14;

// 使用返回 void 的回调
HsBa::Slicer::Utils::Visit<int, double, std::string>(
    [](auto&& val) {
        std::cout << "Processing value: " << val << std::endl;
    },
    value
);
// 输出: Processing value: 3.14
```

### 5. 类型不匹配处理

当 any 中的类型不在指定的类型列表中时，Visit 会返回默认值（对于 void 回调则不执行任何操作）：

```cpp
#include "base/any_visit.hpp"
#include <any>
#include <iostream>

std::any value = true;  // bool 类型不在 int, double, std::string 中

auto result = HsBa::Slicer::Utils::Visit<int, double, std::string>(
    [](auto&& val) -> int {
        return static_cast<int>(val);
    },
    value
);

// result 将是 int 的默认值 (0)，因为类型不匹配
std::cout << "Result: " << result << std::endl;  // 输出: Result: 0
```

### 6. 实际应用示例

```cpp
#include "base/any_visit.hpp"
#include <any>
#include <iostream>
#include <vector>

int main() {
    // 创建一个包含不同类型值的容器
    std::vector<std::any> values;
    values.push_back(42);
    values.push_back(3.14);
    values.push_back(std::string("Hello"));
    values.push_back(std::vector<int>{1, 2, 3});

    // 定义处理函数
    auto process_value = [](auto&& val) -> std::string {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, int>) {
            return "Integer: " + std::to_string(val);
        } else if constexpr (std::is_same_v<T, double>) {
            return "Double: " + std::to_string(val);
        } else if constexpr (std::is_same_v<T, std::string>) {
            return "String: " + val;
        } else {
            return "Other type";
        }
    };

    // 处理容器中的每个值
    for (auto& any_val : values) {
        auto result = HsBa::Slicer::Utils::Visit<int, double, std::string>(
            process_value,
            any_val
        );
        
        std::cout << result << std::endl;
    }
    
    // 输出:
    // Integer: 42
    // Double: 3.140000
    // String: Hello
    // Other type
    
    return 0;
}
```

### 7. 与模板结合使用

```cpp
#include "base/any_visit.hpp"
#include <any>
#include <iostream>

// 创建一个模板函数来处理 any
template<typename... Types>
void ProcessAny(const std::any& value) {
    auto handler = [](auto&& val) {
        std::cout << "Type: " << typeid(decltype(val)).name() 
                  << ", Value: " << val << std::endl;
    };
    
    HsBa::Slicer::Utils::Visit<Types...>(handler, value);
}

int main() {
    std::any int_val = 123;
    std::any str_val = std::string("Test");
    
    ProcessAny<int, std::string>(int_val);  // 处理 int
    ProcessAny<int, std::string>(str_val);  // 处理 string
    
    return 0;
}
```

### 8. Visit 与 Overloaded 结合使用

Visit 组件可以与 template_helper 中的 Overloaded 工具结合使用，以提供更灵活的类型处理方式：

```cpp
#include "base/any_visit.hpp"
#include "base/template_helper.hpp"  // 包含 Overloaded
#include <any>
#include <iostream>
#include <variant>

int main() {
    // 创建一个 std::any 对象，存储 variant
    std::any value = std::any(std::variant<int, double, std::string>(42));
    
    // 使用 Overloaded 定义特定类型的处理函数
    auto visitor = HsBa::Slicer::Utils::Overloaded{
        [](int i) { 
            std::cout << "Integer: " << i << std::endl; 
            return "processed_int";
        },
        [](double d) { 
            std::cout << "Double: " << d << std::endl; 
            return "processed_double";
        },
        [](const std::string& s) { 
            std::cout << "String: " << s << std::endl; 
            return "processed_string";
        }
    };
    
    // 使用 Visit 访问 any 中的值
    // 注意：这里我们先提取 variant，然后使用 std::visit
    if (auto* var_ptr = std::any_cast<std::variant<int, double, std::string>>(&value)) {
        auto result = std::visit(visitor, *var_ptr);
        std::cout << "Visit result: " << result << std::endl;
    }
    
    // 或者，直接处理 any 中的具体类型
    std::any int_val = 100;
    std::any double_val = 3.14;
    std::any string_val = std::string("Hello");
    
    // 定义一个处理不同类型 any 值的函数
    auto process_any = [&visitor](const std::any& val) {
        // 尝试每种可能的类型
        if (val.type() == typeid(int)) {
            auto result = HsBa::Slicer::Utils::Visit<int>(visitor, val);
            std::cout << "Process result: " << result << std::endl;
        } else if (val.type() == typeid(double)) {
            auto result = HsBa::Slicer::Utils::Visit<double>(visitor, val);
            std::cout << "Process result: " << result << std::endl;
        } else if (val.type() == typeid(std::string)) {
            auto result = HsBa::Slicer::Utils::Visit<std::string>(visitor, val);
            std::cout << "Process result: " << result << std::endl;
        }
    };
    
    process_any(int_val);      // 处理 int
    process_any(double_val);   // 处理 double
    process_any(string_val);   // 处理 string
    
    return 0;
}
```

## 注意事项

- 所有在 Visit 模板参数中指定的类型必须与回调函数兼容
- 返回值类型必须在所有可能的回调中保持一致
- 如果 any 中的类型不在指定的类型列表中，将返回默认值（void 回调则不执行）
- 使用 boost::any 和 std::any 时，API 是相同的
- 回调函数可以是 lambda、函数指针或任何可调用对象