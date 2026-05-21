# Tuple Each

The Tuple Each component provides functionality to iterate and manipulate elements in std::tuple, supporting the application of functions to each element of the tuple.

## Features

- Apply functions to each element of a tuple
- Support returning new tuples or performing side effects
- Type-safe tuple operations
- Compile-time index sequence generation

## Usage

### 1. Basic Usage

```cpp
#include "base/tuple_each.hpp"
#include <tuple>
#include <iostream>

// Create a tuple containing elements of different types
std::tuple<int, double, std::string> my_tuple{42, 3.14, "Hello"};

// Perform operations on each element of the tuple
HsBa::Slicer::Utils::TupleEach(my_tuple, [](auto&& element) {
    std::cout << element << " ";
});
// Output: 42 3.14 Hello
```

### 2. Parameterized Iteration

```cpp
#include "base/tuple_each.hpp"
#include <tuple>

std::tuple<int, double, float> numbers{1, 2.5, 3.0f};

// Perform operations on each element of the tuple, passing additional parameters
HsBa::Slicer::Utils::TupleEach(numbers, [](auto&& element, int multiplier) {
    std::cout << element * multiplier << " ";
}, 10);
// Output: 10 25 30
```

### 3. Generate New Tuple

When the callback function has a return value, TupleEach returns a new tuple containing all return values:

```cpp
#include "base/tuple_each.hpp"
#include <tuple>
#include <string>

std::tuple<int, double, char> original{10, 20.5, 'A'};

// Create a new tuple where each element is the string representation of the original element
auto string_tuple = HsBa::Slicer::Utils::TupleEach(original, [](auto&& element) {
    return std::to_string(element);
});

// string_tuple is now std::tuple<std::string, std::string, std::string>
```

### 4. Practical Application Example

```cpp
#include "base/tuple_each.hpp"
#include <tuple>
#include <vector>
#include <iostream>

// Define some data of different types
struct Point { int x, y; };
struct Color { int r, g, b; };
struct Size { int width, height; };

int main() {
    // Create a tuple containing objects of different types
    std::tuple<Point, Color, Size> objects{
        Point{10, 20},
        Color{255, 128, 0},
        Size{100, 50}
    };
    
    // Print information about each object
    std::cout << "Object properties:" << std::endl;
    HsBa::Slicer::Utils::TupleEach(objects, [](const auto& obj) {
        using T = std::decay_t<decltype(obj)>;
        if constexpr (std::is_same_v<T, Point>) {
            std::cout << "Point: (" << obj.x << ", " << obj.y << ")" << std::endl;
        } else if constexpr (std::is_same_v<T, Color>) {
            std::cout << "Color: (" << obj.r << ", " << obj.g << ", " << obj.b << ")" << std::endl;
        } else if constexpr (std::is_same_v<T, Size>) {
            std::cout << "Size: " << obj.width << "x" << obj.height << std::endl;
        }
    });
    
    // Create a tuple containing property counts for each object
    auto counts = HsBa::Slicer::Utils::TupleEach(objects, [](const auto& obj) {
        using T = std::decay_t<decltype(obj)>;
        if constexpr (std::is_same_v<T, Point>) {
            return 2; // x, y
        } else if constexpr (std::is_same_v<T, Color>) {
            return 3; // r, g, b
        } else if constexpr (std::is_same_v<T, Size>) {
            return 2; // width, height
        }
    });
    
    std::cout << "Property counts: ";
    HsBa::Slicer::Utils::TupleEach(counts, [](int count) {
        std::cout << count << " ";
    });
    std::cout << std::endl;
    
    return 0;
}
```

### 5. Integration with Other Components

```cpp
#include "base/tuple_each.hpp"
#include "base/delegate.hpp"
#include <tuple>

// Create a tuple containing multiple delegates
std::tuple<
    HsBa::Slicer::Utils::Delegate<void, int>,
    HsBa::Slicer::Utils::Delegate<void, double>,
    HsBa::Slicer::Utils::Delegate<void, std::string>
> delegates;

// Add callbacks to each delegate
HsBa::Slicer::Utils::TupleEach(delegates, [](auto& delegate) {
    using DelegateType = std::decay_t<decltype(delegate)>;
    using ArgType = typename DelegateType::template Callback;  // Get parameter type
    
    // Add appropriate callbacks based on different parameter types
    if constexpr (std::is_invocable_v<decltype(delegate), int>) {
        delegate += [](int value) { std::cout << "Int: " << value << std::endl; };
    } else if constexpr (std::is_invocable_v<decltype(delegate), double>) {
        delegate += [](double value) { std::cout << "Double: " << value << std::endl; };
    } else if constexpr (std::is_invocable_v<decltype(delegate), std::string>) {
        delegate += [](const std::string& value) { std::cout << "String: " << value << std::endl; };
    }
});
```

## Notes

- The TupleEach function processes each element of the tuple in order
- If the callback function returns void, TupleEach also returns void (performing side effects)
- If the callback function returns a non-void value, TupleEach returns a new tuple containing all return values
- Use `constexpr if` to perform different operations based on element types
- All operations are type-safe, with compile-time type compatibility checks