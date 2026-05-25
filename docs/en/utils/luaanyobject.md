# LuaAnyObject

The LuaAnyObject component provides functionality for registering and using AnyObject types in Lua, supporting type conversion and object management between C++ and Lua.

## Features

- Support for registering custom types in Lua
- Provides `new_` and `cast_` series functions for object creation and type conversion
- Seamless integration with AnyObject system
- Specialized implementations for basic Lua types (int, double, string, bool, etc.)
- Automatic memory management and garbage collection support

## Core Classes

### LuaAnyObjectNewCastBase

Base class for all type registrars, defining the basic interface for type registration.

```cpp
class LuaAnyObjectNewCastBase
{
public:
    typedef int(*LuaFunc)(lua_State* L);
    struct LuaFuncPair
    {
        std::string name;
        LuaFunc func;
    };
    virtual LuaFuncPair GetNewFuncPair() const = 0;
    virtual LuaFuncPair GetCastFuncPair() const = 0;
    virtual ~LuaAnyObjectNewCastBase() = default;
};
```

### LuaAnyObjectNewCastAbstract

Template abstract class providing generic `GetNewFuncPair` and `GetCastFuncPair` implementations for derived classes.

```cpp
template <typename Derived>
class LuaAnyObjectNewCastAbstract : public LuaAnyObjectNewCastBase
{
public:
    LuaFuncPair GetNewFuncPair() const override
    {
        return { "new_" + std::string(type_name()), new_func() };
    }
    LuaFuncPair GetCastFuncPair() const override
    {
        return { "cast_" + std::string(type_name()), cast_func() };
    }
};
```

### LuaAnyObjectNewCastImpl

Generic template implementation for specific type registrars.

```cpp
template<typename T, Utils::TemplateString TypeName>
class LuaAnyObjectNewCastImpl : public LuaAnyObjectNewCastAbstract<LuaAnyObjectNewCastImpl<T, TypeName>>
{
public:
    static int new_func_impl(lua_State* L);
    static int cast_func_impl(lua_State* L);
};
```

## Predefined Type Aliases

LuaAnyObject provides predefined type aliases for common types:

```cpp
using LuaInt = LuaAnyObjectNewCastImpl<int, "int">;
using LuaLongLong = LuaAnyObjectNewCastImpl<long long, "longlong">;
using LuaLong = LuaAnyObjectNewCastImpl<long, "long">;
using LuaSize_t = LuaAnyObjectNewCastImpl<size_t, "size_t">;
using LuaDouble = LuaAnyObjectNewCastImpl<double, "double">;
using LuaFloat = LuaAnyObjectNewCastImpl<float, "float">;
using LuaBool = LuaAnyObjectNewCastImpl<bool, "bool">;
using LuaString = LuaAnyObjectNewCastImpl<std::string, "string">;
using LuaCString = LuaAnyObjectNewCastImpl<const char*, "cstring">;
```

## Usage

### 1. Registering Custom Types to Lua

**Important**: Using LuaAnyObject requires registration at two levels:

1. **C++ Level**: Register `TypeInfo` to support reflection and `Invoke` calls
2. **Lua Level**: Register Lua metatable to support direct object creation and method calls in Lua

```cpp
#include "utils/LuaAnyObject.hpp"
#include <lua.hpp>

// Assume a class
struct Standard
{
    int value;
    Standard(int v = 0) : value(v) {}
    int Add(int x) const { return value + x; }
};

// ========== Step 1: Register TypeInfo at C++ Level ==========
namespace HsBa::Slicer::Utils
{
    template<>
    TypeInfo* GetTypeInfo<Standard>()
    {
        static TypeInfo info;
        info.Name = "Standard";
        info.destroy = [](void* data) { delete static_cast<Standard*>(data); };
        info.copy = [](const void* data) -> void* { 
            return new Standard(*static_cast<const Standard*>(data)); 
        };
        info.move = [](void* data) -> void* { 
            return new Standard(std::move(*static_cast<Standard*>(data))); 
        };
        info.fields.clear();
        info.fields.emplace("value", std::make_pair(GetTypeInfo<int>(), 0));
        info.methods.clear();
        info.methods.emplace("Add", type_ensure<&Standard::Add>());
        return &info;
    }
}

// ========== Step 2: Register Type at Lua Level ==========
// Helper function to create Standard objects in Lua
int lua_standard_new(lua_State* L)
{
    int value = luaL_checkinteger(L, 1);
    Standard* obj = new Standard(value);
    // Wrap with LuaAnyObject
    HsBa::Slicer::NewLuaObject<HsBa::Slicer::Utils::AnyObject, "AnyObject">(L, *obj);
    delete obj; // NewLuaObject already copied, can delete temporary object
    return 1;
}

// Helper function for Standard::add method in Lua
int lua_standard_add(lua_State* L)
{
    auto* obj = (HsBa::Slicer::Utils::AnyObject*)lua_topointer(L, 1);
    if (!obj) {
        lua_pushstring(L, "Invalid Standard object");
        lua_error(L);
        return 0;
    }
    
    int arg = luaL_checkinteger(L, 2);
    try {
        HsBa::Slicer::Utils::AnyObject args[] = { HsBa::Slicer::Utils::AnyObject(arg) };
        HsBa::Slicer::Utils::AnyObject result = obj->Invoke("Add", args);
        lua_pushinteger(L, result.cast<int>());
        return 1;
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        lua_error(L);
        return 0;
    }
}

// Helper function for Standard garbage collection in Lua
int lua_standard_gc(lua_State* L)
{
    return HsBa::Slicer::LuaGC<HsBa::Slicer::Utils::AnyObject, "AnyObject">(L);
}

// Register Standard type to Lua
void RegisterStandardType(lua_State* L)
{
    // Register metatable
    luaL_newmetatable(L, "Standard");
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, lua_standard_gc);
    lua_setfield(L, -2, "__gc");
    lua_pushcfunction(L, lua_standard_add);
    lua_setfield(L, -2, "add");
    lua_pop(L, 1);

    // Create Standard global table
    lua_newtable(L);
    lua_pushcfunction(L, lua_standard_new);
    lua_setfield(L, -2, "new");
    lua_setglobal(L, "Standard");
}

// ========== Step 3: Register AnyObject System with RegisterAnyObject ==========
int main()
{
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    
    // First register custom type Lua bindings
    RegisterStandardType(L);
    
    // Register Standard type to AnyObject system
    std::vector<LuaAnyObjectNewCastBase*> types;
    static LuaAnyObjectNewCastImpl<Standard, "Standard"> standard_type;
    types.push_back(&standard_type);
    
    // Register basic types
    static LuaInt int_type;
    types.push_back(&int_type);
    static LuaDouble double_type;
    types.push_back(&double_type);
    static LuaString string_type;
    types.push_back(&string_type);
    
    // Perform registration
    RegisterAnyObject(L, types);
    
    // Now you can use Standard type in Lua
    luaL_dostring(L, R"(
        local obj = Standard.new(42)
        local result = obj:add(8)
        print(result) -- Output: 50
    )");
    
    lua_close(L);
    return 0;
}
```

### 2. Using Registered Types in Lua

```lua
-- Create Standard object
local standard_obj = Standard.new(42)

-- Call method
local add_result = standard_obj:add(8)
print("Add result:", add_result)  -- Output: 50

-- Convert to AnyObject
local any_obj = AnyObject.new_Standard(standard_obj)

-- Convert back to Standard from AnyObject
local back_to_standard = any_obj:cast_Standard()
```

### 3. Using AnyObject for Type Conversion

```cpp
// Create Lua state and register types
lua_State* L = luaL_newstate();
luaL_openlibs(L);

std::vector<LuaAnyObjectNewCastBase*> types;
static LuaAnyObjectNewCastImpl<Standard, "Standard"> standard_type;
types.push_back(&standard_type);
static LuaInt int_type;
types.push_back(&int_type);

RegisterAnyObject(L, types);

// Execute Lua script
const char* script = R"(
    local standard_obj = Standard.new(42)
    local any_obj = AnyObject.new_Standard(standard_obj)
    
    -- Convert to int type
    local int_obj = AnyObject.new_int(100)
    local value = int_obj:cast_int()
    
    -- Call method
    local invoke_result = any_obj:invoke("Add", int_obj)
    local result_value = invoke_result:cast_int()
)";

luaL_dostring(L, script);
```

### 4. Complete Example

This example demonstrates how to perform complete registration and usage for a complex type (Player) with multiple fields.

```cpp
#include "utils/LuaAnyObject.hpp"
#include "base/any_object.hpp"
#include <lua.hpp>
#include <iostream>

struct Player
{
    int health;
    float speed;
    
    Player(int h = 100, float s = 5.0f) : health(h), speed(s) {}
    
    int AddHealth(int amount) const {
        return health + amount;
    }
};

// ========== Step 1: Register C++ TypeInfo ==========
namespace HsBa::Slicer::Utils
{
    template<>
    TypeInfo* GetTypeInfo<Player>()
    {
        static TypeInfo info;
        info.Name = "Player";
        info.destroy = [](void* data) { delete static_cast<Player*>(data); };
        info.copy = [](const void* data) -> void* { 
            return new Player(*static_cast<const Player*>(data)); 
        };
        info.move = [](void* data) -> void* { 
            return new Player(std::move(*static_cast<Player*>(data))); 
        };
        info.fields.clear();
        info.fields.emplace("health", std::make_pair(GetTypeInfo<int>(), 0));
        info.fields.emplace("speed", std::make_pair(GetTypeInfo<float>(), 4));
        info.methods.clear();
        info.methods.emplace("AddHealth", type_ensure<&Player::AddHealth>());
        return &info;
    }
}

// ========== Step 2: Register Lua Bindings ==========
int lua_player_new(lua_State* L)
{
    int health = luaL_checkinteger(L, 1);
    float speed = luaL_checknumber(L, 2);
    Player* obj = new Player(health, speed);
    HsBa::Slicer::NewLuaObject<HsBa::Slicer::Utils::AnyObject, "AnyObject">(L, *obj);
    delete obj;
    return 1;
}

int lua_player_add_health(lua_State* L)
{
    auto* obj = (HsBa::Slicer::Utils::AnyObject*)lua_topointer(L, 1);
    if (!obj) {
        lua_pushstring(L, "Invalid Player object");
        lua_error(L);
        return 0;
    }
    
    int amount = luaL_checkinteger(L, 2);
    try {
        HsBa::Slicer::Utils::AnyObject args[] = { HsBa::Slicer::Utils::AnyObject(amount) };
        HsBa::Slicer::Utils::AnyObject result = obj->Invoke("AddHealth", args);
        lua_pushinteger(L, result.cast<int>());
        return 1;
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        lua_error(L);
        return 0;
    }
}

int lua_player_gc(lua_State* L)
{
    return HsBa::Slicer::LuaGC<HsBa::Slicer::Utils::AnyObject, "AnyObject">(L);
}

void RegisterPlayerType(lua_State* L)
{
    luaL_newmetatable(L, "Player");
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, lua_player_gc);
    lua_setfield(L, -2, "__gc");
    lua_pushcfunction(L, lua_player_add_health);
    lua_setfield(L, -2, "add_health");
    lua_pop(L, 1);

    lua_newtable(L);
    lua_pushcfunction(L, lua_player_new);
    lua_setfield(L, -2, "new");
    lua_setglobal(L, "Player");
}

// ========== Step 3: Main Function ==========
int main()
{
    // Initialize Lua state
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    
    // First register Player's Lua bindings
    RegisterPlayerType(L);
    
    // Register Player type to AnyObject system
    std::vector<LuaAnyObjectNewCastBase*> types;
    static LuaAnyObjectNewCastImpl<Player, "Player"> player_type;
    types.push_back(&player_type);
    static LuaInt int_type;
    types.push_back(&int_type);
    static LuaFloat float_type;
    types.push_back(&float_type);
    
    RegisterAnyObject(L, types);
    
    // Execute Lua script
    const char* script = R"(
        -- Create Player object
        local player = Player.new(100, 5.0)
        
        -- Convert to AnyObject
        local any_player = AnyObject.new_Player(player)
        
        -- Create parameter object
        local health_add = AnyObject.new_int(50)
        
        -- Call method
        local result = any_player:invoke("AddHealth", health_add)
        local new_health = result:cast_int()
        
        print("New health:", new_health)
        
        -- Iterate through fields
        any_player:foreach_field(function(name, value)
            print("Field:", name, "=", value:cast_int())
        end)
    )";
    
    luaL_dostring(L, script);
    
    lua_close(L);
    return 0;
}
```

### 5. Using Basic Types

```cpp
#include "utils/LuaAnyObject.hpp"
#include <lua.hpp>

int main()
{
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    
    // Register basic types
    std::vector<LuaAnyObjectNewCastBase*> types;
    static LuaInt int_type;
    types.push_back(&int_type);
    static LuaDouble double_type;
    types.push_back(&double_type);
    static LuaString string_type;
    types.push_back(&string_type);
    static LuaBool bool_type;
    types.push_back(&bool_type);
    
    RegisterAnyObject(L, types);
    
    // Use basic types in Lua script
    const char* script = R"(
        -- Create various types of AnyObjects
        local int_obj = AnyObject.new_int(42)
        local double_obj = AnyObject.new_double(3.14)
        local string_obj = AnyObject.new_string("Hello")
        local bool_obj = AnyObject.new_bool(true)
        
        -- Convert back to Lua types
        local int_val = int_obj:cast_int()
        local double_val = double_obj:cast_double()
        local string_val = string_obj:cast_string()
        local bool_val = bool_obj:cast_bool()
        
        print("Int:", int_val)          -- Output: 42
        print("Double:", double_val)    -- Output: 3.14
        print("String:", string_val)    -- Output: Hello
        print("Bool:", bool_val)        -- Output: true
    )";
    
    luaL_dostring(L, script);
    
    lua_close(L);
    return 0;
}
```

## Notes

- **Dual-Level Registration**: Using custom types requires registration at two levels:
  - C++ Level: Register `TypeInfo` to support reflection and `Invoke` calls
  - Lua Level: Register Lua metatable to support direct object creation and method calls in Lua
  - `RegisterAnyObject` only registers the AnyObject system itself, it does not automatically register custom type Lua bindings
- **TypeInfo Registration**: Before using `LuaAnyObjectNewCastImpl`, you must register `TypeInfo` for the corresponding type in the `HsBa::Slicer::Utils` namespace
- **Lifetime Management**: Ensure Lua object lifetimes match the Lua state lifetime to avoid dangling pointers
- **Type Safety**: The `cast_` function will throw an exception on type mismatch; using try-catch is recommended
- **Static Storage**: Type registrar objects should use `static` storage to ensure validity throughout the Lua state's lifetime
- **Memory Management**: Use the `cast_new` method to avoid double destruction issues during Lua garbage collection

## Related Components

- [AnyObject](../base/any_object.md) - Arbitrary object storage and reflection system
- [LuaNewObject](./luanewobject.md) - Lua object creation and management utilities
- [TemplateHelper](../base/template_helper.md) - Template string helper utilities
