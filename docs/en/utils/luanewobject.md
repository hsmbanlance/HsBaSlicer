# LuaNewObject

The LuaNewObject component provides utility functions for object creation and memory management between C++ and Lua, supporting automatic memory management and smart pointers.

## Features

- Provides convenient methods to create C++ objects in Lua
- Supports automatic destructor calls
- Provides smart pointer encapsulation for Lua state management
- Supports template strings as metatable names

## Usage

### 1. Creating Objects in Lua

```cpp
#include "utils/LuaNewObject.hpp"
#include <lua.hpp>

// Example class
class MyClass {
public:
    MyClass(int value) : data(value) {}
    int data;
};

// Create object in Lua
lua_State* L = luaL_newstate();
luaL_openlibs(L);

// Create object with type and metatable name
MyClass* obj = HsBa::Slicer::NewLuaObject<MyClass>(L, "MyClass_mt", 42);

// Now obj is at the top of Lua stack, can further set metatable, etc.
std::cout << obj->data << std::endl;  // Output: 42

lua_close(L);
```

### 2. Using Template String Metatables

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

// Use template string as metatable name
Widget* widget = HsBa::Slicer::NewLuaObject<Widget, "Widget"_ts>(L, "MyWidget");

std::cout << widget->name_ << std::endl;  // Output: MyWidget

lua_close(L);
```

### 3. Lua Object Garbage Collection

```cpp
#include "utils/LuaNewObject.hpp"

// Register garbage collection function for Lua
int myClassGC = [](lua_State* L) -> int {
    // Use LuaGC function for cleanup
    return HsBa::Slicer::LuaGC<MyClass>(L, "MyClass_mt");
};

// Or using template string version
int widgetGC = [](lua_State* L) -> int {
    return HsBa::Slicer::LuaGC<Widget, "Widget"_ts>(L);
};
```

### 4. Managing Lua State with Smart Pointers

```cpp
#include "utils/LuaNewObject.hpp"

// Use smart pointer to automatically manage Lua state
HsBa::Slicer::UniqueLua lua_state = HsBa::Slicer::MakeUniqueLuaState();
lua_State* L = lua_state.get();

// Use Lua state...
lua_pushnumber(L, 42);
lua_setglobal(L, "answer");

// No need to manually call lua_close, smart pointer will automatically clean up when leaving scope
```

### 5. Complete Example

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
    // Manage Lua state with smart pointer
    auto lua_state = HsBa::Slicer::MakeUniqueLuaState();
    lua_State* L = lua_state.get();
    luaL_openlibs(L);
    
    // Create calculator object in Lua
    Calculator* calc = HsBa::Slicer::NewLuaObject<Calculator>(L, "Calculator_mt", 10.0);
    
    std::cout << "Initial value: " << calc->value << std::endl;  // Output: Initial value: 10
    
    // Perform calculation
    double result = calc->add(5.0);
    std::cout << "After add(5): " << result << std::endl;  // Output: After add(5): 15
    
    // Lua state will be automatically closed when UniqueLua goes out of scope
    return 0;
}
```

## Notes

- The NewLuaObject function calls the object's constructor and stores it in Lua user data
- The LuaGC function is responsible for calling the C++ object's destructor during Lua garbage collection
- Using UniqueLua ensures automatic cleanup of the Lua state, preventing memory leaks
- Metatable names must be registered in Lua beforehand to work properly
- Ensure object lifetime matches the Lua state lifetime