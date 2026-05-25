# Tuple Each (元组遍历)

Tuple Each 组件提供了对 std::tuple 中元素进行遍历和操作的功能，支持对元组中每个元素应用函数。

## 功能特点

- 对元组中的每个元素应用函数
- 支持返回新元组或执行副作用操作
- 类型安全的元组操作
- 编译期索引序列生成

## 使用方法

### 1. 基本用法

```cpp
#include "base/tuple_each.hpp"
#include <tuple>
#include <iostream>

// 创建一个包含不同类型元素的元组
std::tuple<int, double, std::string> my_tuple{42, 3.14, "Hello"};

// 对元组中的每个元素执行操作
HsBa::Slicer::Utils::TupleEach(my_tuple, [](auto&& element) {
    std::cout << element << " ";
});
// 输出: 42 3.14 Hello
```

### 2. 带参数的遍历

```cpp
#include "base/tuple_each.hpp"
#include <tuple>

std::tuple<int, double, float> numbers{1, 2.5, 3.0f};

// 对元组中的每个元素执行操作，并传递额外参数
HsBa::Slicer::Utils::TupleEach(numbers, [](auto&& element, int multiplier) {
    std::cout << element * multiplier << " ";
}, 10);
// 输出: 10 25 30
```

### 3. 生成新元组

当回调函数有返回值时，TupleEach 会返回一个包含所有返回值的新元组：

```cpp
#include "base/tuple_each.hpp"
#include <tuple>
#include <string>

std::tuple<int, double, char> original{10, 20.5, 'A'};

// 创建一个新元组，每个元素都是原元素的字符串表示
auto string_tuple = HsBa::Slicer::Utils::TupleEach(original, [](auto&& element) {
    return std::to_string(element);
});

// string_tuple 现在是 std::tuple<std::string, std::string, std::string>
```

### 4. 实际应用示例

```cpp
#include "base/tuple_each.hpp"
#include <tuple>
#include <vector>
#include <iostream>

// 定义一些不同类型的数据
struct Point { int x, y; };
struct Color { int r, g, b; };
struct Size { int width, height; };

int main() {
    // 创建一个包含不同类型对象的元组
    std::tuple<Point, Color, Size> objects{
        Point{10, 20},
        Color{255, 128, 0},
        Size{100, 50}
    };
    
    // 打印每个对象的信息
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
    
    // 创建一个包含各对象属性数量的元组
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

### 5. 与其他组件结合使用

```cpp
#include "base/tuple_each.hpp"
#include "base/delegate.hpp"
#include <tuple>

// 创建一个包含多个委托的元组
std::tuple<
    HsBa::Slicer::Utils::Delegate<void, int>,
    HsBa::Slicer::Utils::Delegate<void, double>,
    HsBa::Slicer::Utils::Delegate<void, std::string>
> delegates;

// 为每个委托添加回调
HsBa::Slicer::Utils::TupleEach(delegates, [](auto& delegate) {
    using DelegateType = std::decay_t<decltype(delegate)>;
    using ArgType = typename DelegateType::template Callback;  // 获取参数类型
    
    // 根据不同的参数类型添加相应的回调
    if constexpr (std::is_invocable_v<decltype(delegate), int>) {
        delegate += [](int value) { std::cout << "Int: " << value << std::endl; };
    } else if constexpr (std::is_invocable_v<decltype(delegate), double>) {
        delegate += [](double value) { std::cout << "Double: " << value << std::endl; };
    } else if constexpr (std::is_invocable_v<decltype(delegate), std::string>) {
        delegate += [](const std::string& value) { std::cout << "String: " << value << std::endl; };
    }
});
```

## 注意事项

- TupleEach 函数会按顺序处理元组中的每个元素
- 如果回调函数返回 void，则 TupleEach 也返回 void（执行副作用操作）
- 如果回调函数返回非 void 值，则 TupleEach 返回包含所有返回值的新元组
- 使用 `constexpr if` 可以根据元素类型执行不同的操作
- 所有操作都是类型安全的，编译时会检查类型兼容性