# Any Visit

The Any Visit component provides type-safe access to std::any and boost::any, allowing type matching and operations on arbitrary types at runtime.

## Features

- Type-safe access to std::any and boost::any
- Support for matching multiple type lists
- Compile-time type checking
- Support for parameterized callback functions
- Return type consistency checking

## Usage

### 1. Basic Usage

```cpp
#include "base/any_visit.hpp"
#include <any>
#include <iostream>

// Create a std::any object
std::any value = 42;

// Use Visit to access the value in any
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

std::cout << result << std::endl;  // Output: Integer: 42
```

### 2. Using boost::any

```cpp
#include "base/any_visit.hpp"
#include <boost/any.hpp>
#include <iostream>

// Create a boost::any object
boost::any value = std::string("Hello World");

// Use Visit to access the value in any
auto result = HsBa::Slicer::Utils::Visit<int, double, std::string>(
    [](auto&& val) -> std::string {
        return std::string("Value: ") + val;
    },
    value
);

std::cout << result << std::endl;  // Output: Value: Hello World
```

### 3. Parameterized Access

```cpp
#include "base/any_visit.hpp"
#include <any>

std::any value = 100;

// Use access with additional parameters
auto result = HsBa::Slicer::Utils::Visit<int, double>(
    [](auto&& val, int multiplier) -> int {
        return static_cast<int>(val * multiplier);
    },
    value,
    5  // Additional parameter
);

std::cout << result << std::endl;  // Output: 500
```

### 4. Void Return Type

When the callback function returns void, Visit also returns void:

```cpp
#include "base/any_visit.hpp"
#include <any>
#include <iostream>

std::any value = 3.14;

// Use callback that returns void
HsBa::Slicer::Utils::Visit<int, double, std::string>(
    [](auto&& val) {
        std::cout << "Processing value: " << val << std::endl;
    },
    value
);
// Output: Processing value: 3.14
```

### 5. Type Mismatch Handling

When the type in any is not in the specified type list, Visit returns a default value (or performs no operation for void callbacks):

```cpp
#include "base/any_visit.hpp"
#include <any>
#include <iostream>

std::any value = true;  // bool type is not in int, double, std::string

auto result = HsBa::Slicer::Utils::Visit<int, double, std::string>(
    [](auto&& val) -> int {
        return static_cast<int>(val);
    },
    value
);

// result will be the default value of int (0) because of type mismatch
std::cout << "Result: " << result << std::endl;  // Output: Result: 0
```

### 6. Practical Application Example

```cpp
#include "base/any_visit.hpp"
#include <any>
#include <iostream>
#include <vector>

int main() {
    // Create a container containing values of different types
    std::vector<std::any> values;
    values.push_back(42);
    values.push_back(3.14);
    values.push_back(std::string("Hello"));
    values.push_back(std::vector<int>{1, 2, 3});

    // Define processing function
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

    // Process each value in the container
    for (auto& any_val : values) {
        auto result = HsBa::Slicer::Utils::Visit<int, double, std::string>(
            process_value,
            any_val
        );
        
        std::cout << result << std::endl;
    }
    
    // Output:
    // Integer: 42
    // Double: 3.140000
    // String: Hello
    // Other type
    
    return 0;
}
```

### 7. Integration with Templates

```cpp
#include "base/any_visit.hpp"
#include <any>
#include <iostream>

// Create a template function to process any
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
    
    ProcessAny<int, std::string>(int_val);  // Process int
    ProcessAny<int, std::string>(str_val);  // Process string
    
    return 0;
}
```

### 8. Combining Visit with Overloaded

The Visit component can be combined with the Overloaded utility from template_helper to provide more flexible type handling:

```cpp
#include "base/any_visit.hpp"
#include "base/template_helper.hpp"  // Include Overloaded
#include <any>
#include <iostream>
#include <variant>

int main() {
    // Create a std::any object storing a variant
    std::any value = std::any(std::variant<int, double, std::string>(42));
    
    // Use Overloaded to define handlers for specific types
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
    
    // Use Visit to access the value in any
    // Note: Here we extract the variant first, then use std::visit
    if (auto* var_ptr = std::any_cast<std::variant<int, double, std::string>>(&value)) {
        auto result = std::visit(visitor, *var_ptr);
        std::cout << "Visit result: " << result << std::endl;
    }
    
    // Or, directly process specific types stored in any
    std::any int_val = 100;
    std::any double_val = 3.14;
    std::any string_val = std::string("Hello");
    
    // Define a function to process any values of different types
    auto process_any = [&visitor](const std::any& val) {
        // Try each possible type
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
    
    process_any(int_val);      // Process int
    process_any(double_val);   // Process double
    process_any(string_val);   // Process string
    
    return 0;
}
```

### 8. Combining Visit with Overloaded

The Visit component can be combined with the Overloaded utility from template_helper to provide more flexible type handling:

```cpp
#include "base/any_visit.hpp"
#include "base/template_helper.hpp"  // Include Overloaded
#include <any>
#include <iostream>
#include <variant>

int main() {
    // Create a std::any object storing a variant
    std::any value = std::any(std::variant<int, double, std::string>(42));
    
    // Use Overloaded to define handlers for specific types
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
    
    // Use Visit to access the value in any
    // Note: Here we extract the variant first, then use std::visit
    if (auto* var_ptr = std::any_cast<std::variant<int, double, std::string>>(&value)) {
        auto result = std::visit(visitor, *var_ptr);
        std::cout << "Visit result: " << result << std::endl;
    }
    
    // Or, directly process specific types stored in any
    std::any int_val = 100;
    std::any double_val = 3.14;
    std::any string_val = std::string("Hello");
    
    // Define a function to process any values of different types
    auto process_any = [&visitor](const std::any& val) {
        // Try each possible type
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
    
    process_any(int_val);      // Process int
    process_any(double_val);   // Process double
    process_any(string_val);   // Process string
    
    return 0;
}
```

## Notes

- All types specified in Visit template parameters must be compatible with the callback function
- Return type must be consistent across all possible callbacks
- If the type in any is not in the specified type list, a default value will be returned (no operation for void callbacks)
- The API is the same when using boost::any and std::any
- Callback functions can be lambdas, function pointers, or any callable objects