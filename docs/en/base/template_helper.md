# Template Helper

The Template Helper component provides a series of template-related utility functions, including template strings, type constraints, formatting support, etc.

## Features

- TemplateString: Compile-time string processing
- Type constraints and concept checking
- Template string literals
- Formatting support

## Usage

### 1. TemplateString

TemplateString allows compile-time string processing, supporting string concatenation, comparison, and other operations.

```cpp
#include "base/template_helper.hpp"

// Using template strings
using namespace HsBa::Slicer::Utils::TemplateStringLiterals;

// Define compile-time strings
constexpr auto name = "Hello"_ts;
constexpr auto world = "World"_ts;

// String concatenation
constexpr auto hello_world = name + world;

// Using template strings as type parameters
template<typename T>
struct MyStruct {
    static constexpr auto value_name = T::Name();
};

// Creating types through template strings
using MyType = HsBa::Slicer::Utils::NamedType<int, "counter"_ts>;
```

### 2. NamedType

NamedType provides type-safe type aliases, where even if the base types are the same, different names are treated as different types.

```cpp
#include "base/template_helper.hpp"

// Create variables with different names but same base type
using Width = HsBa::Slicer::Utils::NamedType<int, "width"_ts>;
using Height = HsBa::Slicer::Utils::NamedType<int, "height"_ts>;

Width w{100};
Height h{200};

// This avoids confusion during parameter passing
void SetSize(Width width, Height height) {
    // ...
}
```

### 3. Type Constraints and Concepts

The component includes various type constraints and concepts for type checking in template programming.

```cpp
#include "base/template_helper.hpp"

// Using concepts to constrain template parameters
template<typename T>
    requires HsBa::Slicer::Utils::CharType<T>
void ProcessCharType(T ch) {
    // Accepts only character types
}

// Using Addable concept
template<typename T>
    requires HsBa::Slicer::Utils::Addable<T>
T AddValues(const T& a, const T& b) {
    return a + b;
}
```

### 4. Complete Example

```cpp
#include "base/template_helper.hpp"
#include <iostream>
#include <string>

using namespace HsBa::Slicer::Utils::TemplateStringLiterals;

int main() {
    // Template string example
    constexpr auto app_name = "MyApp"_ts;
    constexpr auto version = "1.0"_ts;
    constexpr auto full_name = app_name + "_" + version;
    
    std::cout << "App: " << full_name.ToString() << std::endl;
    
    // Named type example
    using Distance = HsBa::Slicer::Utils::NamedType<double, "distance"_ts>;
    using Time = HsBa::Slicer::Utils::NamedType<double, "time"_ts>;
    
    Distance dist{10.5};
    Time time{2.0};
    
    std::cout << "Distance: " << dist.Get() << std::endl;
    std::cout << "Time: " << time.Get() << std::endl;
    
    return 0;
}
```

## Additional Features

### 5. template_call

`template_call` allows you to call a function with a template string as the first parameter.

```cpp
#include "base/template_helper.hpp"

using namespace HsBa::Slicer::Utils::TemplateStringLiterals;

void printName(std::string_view name, int value) {
    std::cout << "Name: " << name << ", Value: " << value << std::endl;
}

int main() {
    // Call function with template string
    HsBa::Slicer::Utils::template_call<"my_value"_ts>(printName, 42);
    return 0;
}
```

### 6. Invoke and AsyncInvoke

`Invoke` and `AsyncInvoke` provide utilities for synchronous and asynchronous function execution.

```cpp
#include "base/template_helper.hpp"
#include <iostream>
#include <thread>
#include <chrono>

int add(int a, int b) {
    return a + b;
}

int main() {
    // Synchronous invoke
    int result1 = HsBa::Slicer::Utils::Invoke(add, 5, 3);
    std::cout << "Sync result: " << result1 << std::endl;
    
    // Asynchronous invoke
    auto future = HsBa::Slicer::Utils::AsyncInvoke(add, 10, 20);
    int result2 = future.get();
    std::cout << "Async result: " << result2 << std::endl;
    
    return 0;
}
```

### 7. Overloaded

`Overloaded` is a utility for creating overloaded lambda functions, particularly useful with `std::visit`.

```cpp
#include "base/template_helper.hpp"
#include <variant>
#include <iostream>

using VariantType = std::variant<int, double, std::string>;

int main() {
    VariantType var = 42;
    
    std::visit(
        HsBa::Slicer::Utils::Overloaded{
            [](int i) { std::cout << "Integer: " << i << std::endl; },
            [](double d) { std::cout << "Double: " << d << std::endl; },
            [](const std::string& s) { std::cout << "String: " << s << std::endl; }
        },
        var
    );
    
    return 0;
}
```

### 8. Enum Utilities

The component provides utilities for working with enums, including converting between enum values and string names.

```cpp
#include "base/template_helper.hpp"
#include <iostream>

enum class Color { Red, Green, Blue, MaxColor };

int main() {
    // Compile-time enum name
    constexpr auto red_name = HsBa::Slicer::Utils::EnumName<Color::Red>();
    std::cout << "Red name: " << red_name << std::endl;
    
    // Runtime enum name
    Color color = Color::Green;
    std::cout << "Green name: " << HsBa::Slicer::Utils::EnumName(color) << std::endl;
    
    // Enum from name
    Color blue = HsBa::Slicer::Utils::EnumFromName<Color>("Blue");
    std::cout << "Blue value: " << static_cast<int>(blue) << std::endl;
    
    return 0;
}
```

### 9. Named Pointer Types

`NamedRawPtr` and `NamedPtr` provide named pointer types for improved type safety.

```cpp
#include "base/template_helper.hpp"
#include <memory>
#include <iostream>

using namespace HsBa::Slicer::Utils::TemplateStringLiterals;

int main() {
    int value = 42;
    
    // Named raw pointer
    HsBa::Slicer::Utils::NamedRawPtr<"my_ptr"_ts, int> named_ptr{&value};
    std::cout << "Named ptr name: " << named_ptr.Name() << std::endl;
    std::cout << "Value: " << *named_ptr << std::endl;
    
    // Named smart pointer
    std::unique_ptr<int> smart_ptr = std::make_unique<int>(100);
    HsBa::Slicer::Utils::NamedPtr<"smart_ptr"_ts, int, std::unique_ptr> named_smart_ptr{std::move(smart_ptr)};
    std::cout << "Named smart ptr name: " << named_smart_ptr.Name() << std::endl;
    std::cout << "Value: " << *named_smart_ptr << std::endl;
    
    return 0;
}
```

### 10. TemplateString Advanced Methods

`TemplateString` includes additional methods for string manipulation.

```cpp
#include "base/template_helper.hpp"
#include <iostream>
#include <vector>

using namespace HsBa::Slicer::Utils::TemplateStringLiterals;

int main() {
    constexpr auto text = "Hello,World,Test"_ts;
    
    // Convert to uppercase
    constexpr auto upper = text.ToUpper();
    std::cout << "Uppercase: " << upper.ToString() << std::endl;
    
    // Convert to lowercase
    constexpr auto lower = text.ToLower();
    std::cout << "Lowercase: " << lower.ToString() << std::endl;
    
    // Split string
    auto parts = text.Split<std::vector<HsBa::Slicer::Utils::TemplateString<char, 10>>>(","_ts.ToStringView());
    for (const auto& part : parts) {
        std::cout << "Part: " << part.ToString() << std::endl;
    }
    
    // Access by index
    std::cout << "First char: " << text[0] << std::endl;
    std::cout << "Length: " << text.size() << std::endl;
    
    return 0;
}
```

### 11. Other Utilities

- `AllTheSame`: A type trait to ensure all types in a parameter pack are the same
- `HasVoidV`: A variable template to check if any type in a parameter pack is void
- `YCombinator`: A utility for creating recursive lambdas
- `FloatTemplateArgs` and `DoubleTemplateArgs`: Type aliases for floating-point template parameters that handle compatibility with certain Android NDK versions

```cpp
#include "base/template_helper.hpp"
#include <iostream>

int main() {
    // YCombinator for recursive lambda
    auto factorial = HsBa::Slicer::Utils::YCombinator{
        [](auto&& self, int n) -> int {
            return (n <= 1) ? 1 : n * self(self, n - 1);
        }
    };
    
    std::cout << "Factorial of 5: " << factorial(5) << std::endl;
    
    return 0;
}
```

## Implementation Notes

- For certain special versions of Android NDK, `FloatTemplateArgs` and `DoubleTemplateArgs` are provided to handle floating-point template parameters, while other platforms use `float` and `double` directly. Therefore, avoid accessing the `value` member directly.
- YCombinator-wrapped lambdas need to manually specify the return type. If the compiler supports explicit `this` parameter (C++23 and later), use explicit `this` parameter in the lambda directly, in which case the Y combinator is not provided.

## Notes

- TemplateString size is determined at compile time and requires specifying maximum length
- Named types are mainly used for type safety, basic operations are accessed through Get() method
- Concept constraints are used for compile-time type checking to improve code safety
- Named pointers provide type safety by distinguishing pointers based on their names
- The YCombinator enables recursion in lambdas without explicit self-reference in C++ versions before C++23