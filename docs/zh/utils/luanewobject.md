# LuaNewObject (Lua 对象管理)

LuaNewObject 组件提供了 C++ 与 Lua 之间对象创建和内存管理的工具函数，支持自动内存管理和智能指针。

## 功能特点

- 提供在 Lua 中创建 C++ 对象的便捷方法
- 支持自动析构函数调用
- 提供智能指针封装的 Lua 状态管理
- 支持模板字符串作为元表名称

## 使用方法

### 1. 在 Lua 中创建对象

```cpp
#include "utils/LuaNewObject.hpp"
#include <lua.hpp>

// 示例类
class MyClass {
public:
    MyClass(int value) : data(value) {}
    int data;
};

// 在 Lua 中创建对象
lua_State* L = luaL_newstate();
luaL_openlibs(L);

// 使用类型和元表名称创建对象
MyClass* obj = HsBa::Slicer::NewLuaObject<MyClass>(L, "MyClass_mt", 42);

// 现在 obj 在 Lua 栈顶，可以进一步设置元表等
std::cout << obj->data << std::endl;  // 输出: 42

lua_close(L);
```

### 2. 使用模板字符串元表

```cpp
#include "utils/LuaNewObject.hpp"
#include "base/template_helper.hpp"

using namespace HsBa::Slicer::Utils::TemplateStringLiterals;

class Widget {
public:
    Widget(const std::string& name) : name_(name) {}
    std::string name_;
};

lua_State* L = luaL_newstate();
luaL_openlibs(L);

// 使用模板字符串作为元表名称
Widget* widget = HsBa::Slicer::NewLuaObject<Widget, "Widget"_ts>(L, "MyWidget");

std::cout << widget->name_ << std::endl;  // 输出: MyWidget

lua_close(L);
```

### 3. Lua 对象垃圾回收

```cpp
#include "utils/LuaNewObject.hpp"

// 为 Lua 注册垃圾回收函数
int myClassGC = [](lua_State* L) -> int {
    // 使用 LuaGC 函数进行清理
    return HsBa::Slicer::LuaGC<MyClass>(L, "MyClass_mt");
};

// 或者使用模板字符串版本
int widgetGC = [](lua_State* L) -> int {
    return HsBa::Slicer::LuaGC<Widget, "Widget"_ts>(L);
};
```

### 4. 使用智能指针管理 Lua 状态

```cpp
#include "utils/LuaNewObject.hpp"

// 使用智能指针自动管理 Lua 状态
HsBa::Slicer::UniqueLua lua_state = HsBa::Slicer::MakeUniqueLuaState();
lua_State* L = lua_state.get();

// 使用 Lua 状态...
lua_pushnumber(L, 42);
lua_setglobal(L, "answer");

// 不需要手动调用 lua_close，智能指针会在离开作用域时自动清理
```

### 5. 完整示例

```cpp
#include "utils/LuaNewObject.hpp"
#include <iostream>

class Calculator {
public:
    Calculator(double initialValue) : value(initialValue) {}
    
    double value;
    
    double add(double x) {
        value += x;
        return value;
    }
};

int main() {
    // 使用智能指针管理 Lua 状态
    auto lua_state = HsBa::Slicer::MakeUniqueLuaState();
    lua_State* L = lua_state.get();
    luaL_openlibs(L);
    
    // 在 Lua 中创建计算器对象
    Calculator* calc = HsBa::Slicer::NewLuaObject<Calculator>(L, "Calculator_mt", 10.0);
    
    std::cout << "Initial value: " << calc->value << std::endl;  // 输出: Initial value: 10
    
    // 执行计算
    double result = calc->add(5.0);
    std::cout << "After add(5): " << result << std::endl;  // 输出: After add(5): 15
    
    // Lua 状态将在 UniqueLua 离开作用域时自动关闭
    return 0;
}
```

## 注意事项

- NewLuaObject 函数会调用对象的构造函数并将其存储在 Lua 用户数据中
- LuaGC 函数负责在 Lua 垃圾回收时调用 C++ 对象的析构函数
- 使用 UniqueLua 可以确保 Lua 状态的自动清理，防止内存泄漏
- 元表名称必须在 Lua 中预先注册才能正常使用
- 确保对象的生命周期与 Lua 状态的生命周期相匹配