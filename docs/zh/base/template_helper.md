# Template Helper (模板辅助)

Template Helper 组件提供了一系列模板相关的辅助功能，包括模板字符串、类型约束、格式化支持等。

## 功能特点

- TemplateString: 编译期字符串处理
- 类型约束和概念检查
- 模板字符串字面量
- 格式化支持

## 使用方法

### 1. TemplateString (模板字符串)

TemplateString 允许在编译期处理字符串，支持字符串拼接、比较等操作。

```cpp
#include "base/template_helper.hpp"

// 使用模板字符串
using namespace HsBa::Slicer::Utils::TemplateStringLiterals;

// 定义编译期字符串
constexpr auto name = "Hello"_ts;
constexpr auto world = "World"_ts;

// 字符串拼接
constexpr auto hello_world = name + world;

// 使用模板字符串作为类型参数
template<typename T>
struct MyStruct {
    static constexpr auto value_name = T::Name();
};

// 通过模板字符串创建类型
using MyType = HsBa::Slicer::Utils::NamedType<int, "counter"_ts>;
```

### 2. NamedType (命名类型)

NamedType 提供类型安全的类型别名，即使基础类型相同但名称不同也被视为不同类型。

```cpp
#include "base/template_helper.hpp"

// 创建不同名称但相同基础类型的变量
using Width = HsBa::Slicer::Utils::NamedType<int, "width"_ts>;
using Height = HsBa::Slicer::Utils::NamedType<int, "height"_ts>;

Width w{100};
Height h{200};

// 这样可以避免参数传递时的混淆
void SetSize(Width width, Height height) {
    // ...
}
```

### 3. 类型约束和概念

组件包含各种类型约束和概念，用于模板编程中的类型检查。

```cpp
#include "base/template_helper.hpp"

// 使用概念约束模板参数
template<typename T>
    requires HsBa::Slicer::Utils::CharType<T>
void ProcessCharType(T ch) {
    // 只接受字符类型
}

// 使用 Addable 概念
template<typename T>
    requires HsBa::Slicer::Utils::Addable<T>
T AddValues(const T& a, const T& b) {
    return a + b;
}
```

### 4. 完整示例

```cpp
#include "base/template_helper.hpp"
#include <iostream>
#include <string>

using namespace HsBa::Slicer::Utils::TemplateStringLiterals;

int main() {
    // 模板字符串示例
    constexpr auto app_name = "MyApp"_ts;
    constexpr auto version = "1.0"_ts;
    constexpr auto full_name = app_name + "_" + version;
    
    std::cout << "App: " << full_name.ToString() << std::endl;
    
    // 命名类型示例
    using Distance = HsBa::Slicer::Utils::NamedType<double, "distance"_ts>;
    using Time = HsBa::Slicer::Utils::NamedType<double, "time"_ts>;
    
    Distance dist{10.5};
    Time time{2.0};
    
    std::cout << "Distance: " << dist.Get() << std::endl;
    std::cout << "Time: " << time.Get() << std::endl;
    
    return 0;
}
```

## 其他功能

### 5. template_call

`template_call` 允许你使用模板字符串作为第一个参数来调用函数。

```cpp
#include "base/template_helper.hpp"

using namespace HsBa::Slicer::Utils::TemplateStringLiterals;

void printName(std::string_view name, int value) {
    std::cout << "名称: " << name << ", 值: " << value << std::endl;
}

int main() {
    // 使用模板字符串调用函数
    HsBa::Slicer::Utils::template_call<"my_value"_ts>(printName, 42);
    return 0;
}
```

### 6. Invoke 和 AsyncInvoke

`Invoke` 和 `AsyncInvoke` 提供同步和异步函数执行的工具。

```cpp
#include "base/template_helper.hpp"
#include <iostream>
#include <thread>
#include <chrono>

int add(int a, int b) {
    return a + b;
}

int main() {
    // 同步调用
    int result1 = HsBa::Slicer::Utils::Invoke(add, 5, 3);
    std::cout << "同步结果: " << result1 << std::endl;
    
    // 异步调用
    auto future = HsBa::Slicer::Utils::AsyncInvoke(add, 10, 20);
    int result2 = future.get();
    std::cout << "异步结果: " << result2 << std::endl;
    
    return 0;
}
```

### 7. Overloaded

`Overloaded` 是用于创建重载lambda函数的工具，特别适用于 `std::visit`。

```cpp
#include "base/template_helper.hpp"
#include <variant>
#include <iostream>

using VariantType = std::variant<int, double, std::string>;

int main() {
    VariantType var = 42;
    
    std::visit(
        HsBa::Slicer::Utils::Overloaded{
            [](int i) { std::cout << "整数: " << i << std::endl; },
            [](double d) { std::cout << "双精度: " << d << std::endl; },
            [](const std::string& s) { std::cout << "字符串: " << s << std::endl; }
        },
        var
    );
    
    return 0;
}
```

### 8. 枚举工具

组件提供了处理枚举的工具，包括在枚举值和字符串名称之间转换。

```cpp
#include "base/template_helper.hpp"
#include <iostream>

enum class Color { Red, Green, Blue, MaxColor };

int main() {
    // 编译期枚举名称
    constexpr auto red_name = HsBa::Slicer::Utils::EnumName<Color::Red>();
    std::cout << "红色名称: " << red_name << std::endl;
    
    // 运行时枚举名称
    Color color = Color::Green;
    std::cout << "绿色名称: " << HsBa::Slicer::Utils::EnumName(color) << std::endl;
    
    // 从名称获取枚举
    Color blue = HsBa::Slicer::Utils::EnumFromName<Color>("Blue");
    std::cout << "蓝色值: " << static_cast<int>(blue) << std::endl;
    
    return 0;
}
```

### 9. 命名指针类型

`NamedRawPtr` 和 `NamedPtr` 提供命名指针类型以提高类型安全性。

```cpp
#include "base/template_helper.hpp"
#include <memory>
#include <iostream>

using namespace HsBa::Slicer::Utils::TemplateStringLiterals;

int main() {
    int value = 42;
    
    // 命名原始指针
    HsBa::Slicer::Utils::NamedRawPtr<"my_ptr"_ts, int> named_ptr{&value};
    std::cout << "命名指针名称: " << named_ptr.Name() << std::endl;
    std::cout << "值: " << *named_ptr << std::endl;
    
    // 命名智能指针
    std::unique_ptr<int> smart_ptr = std::make_unique<int>(100);
    HsBa::Slicer::Utils::NamedPtr<"smart_ptr"_ts, int, std::unique_ptr> named_smart_ptr{std::move(smart_ptr)};
    std::cout << "命名智能指针名称: " << named_smart_ptr.Name() << std::endl;
    std::cout << "值: " << *named_smart_ptr << std::endl;
    
    return 0;
}
```

### 10. TemplateString 高级方法

`TemplateString` 包含用于字符串操作的附加方法。

```cpp
#include "base/template_helper.hpp"
#include <iostream>
#include <vector>

using namespace HsBa::Slicer::Utils::TemplateStringLiterals;

int main() {
    constexpr auto text = "Hello,World,Test"_ts;
    
    // 转换为大写
    constexpr auto upper = text.ToUpper();
    std::cout << "大写: " << upper.ToString() << std::endl;
    
    // 转换为小写
    constexpr auto lower = text.ToLower();
    std::cout << "小写: " << lower.ToString() << std::endl;
    
    // 分割字符串
    auto parts = text.Split<std::vector<HsBa::Slicer::Utils::TemplateString<char, 10>>>(","_ts.ToStringView());
    for (const auto& part : parts) {
        std::cout << "部分: " << part.ToString() << std::endl;
    }
    
    // 按索引访问
    std::cout << "首字符: " << text[0] << std::endl;
    std::cout << "长度: " << text.size() << std::endl;
    
    return 0;
}
```

### 11. 其他工具

- `AllTheSame`: 一个类型特征，确保参数包中的所有类型都相同
- `HasVoidV`: 一个变量模板，检查参数包中是否有任何类型为void
- `YCombinator`: 用于创建递归lambda的工具
- `FloatTemplateArgs` 和 `DoubleTemplateArgs`: 用于处理某些Android NDK版本兼容性的浮点模板参数类型别名

```cpp
#include "base/template_helper.hpp"
#include <iostream>

int main() {
    // YCombinator 用于递归lambda
    auto factorial = HsBa::Slicer::Utils::YCombinator{
        [](auto&& self, int n) -> int {
            return (n <= 1) ? 1 : n * self(self, n - 1);
        }
    };
    
    std::cout << "5的阶乘: " << factorial(5) << std::endl;
    
    return 0;
}
```

## 实现注意事项

- 对于某些特殊版本的Android NDK，提供了 `FloatTemplateArgs` 和 `DoubleTemplateArgs` 来处理浮点模板参数，而其他平台直接使用 `float` 和 `double`。因此，请避免直接访问 `value` 成员。
- YCombinator 封装的lambda需要手动指定返回值类型。如果支持显式this参数，则直接在lambda中显式this参数，此时不提供Y组合子。

## 注意事项

- TemplateString 在编译期确定大小，需要指定最大长度
- 命名类型主要用于类型安全，基础操作通过 Get() 方法访问
- 概念约束用于编译期类型检查，提高代码安全性
- 命名指针通过基于名称区分指针来提供类型安全性
- YCombinator 在C++23之前的版本中启用lambda中的递归，无需显式自引用
