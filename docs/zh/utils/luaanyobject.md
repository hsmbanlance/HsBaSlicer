# LuaAnyObject (Lua 任意对象管理)

LuaAnyObject 组件提供了在 Lua 中注册和使用 AnyObject 类型的功能，支持 C++ 与 Lua 之间的类型转换和对象管理。

## 功能特点

- 支持在 Lua 中注册自定义类型
- 提供 `new_` 和 `cast_` 系列函数用于对象创建和类型转换
- 支持与 AnyObject 系统的无缝集成
- 提供基本 Lua 类型的特化实现（int、double、string、bool 等）
- 自动内存管理和垃圾回收支持

## 核心类

### LuaAnyObjectNewCastBase

所有类型注册器的基类，定义了类型注册的基本接口。

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

模板抽象类，为派生类提供通用的 `GetNewFuncPair` 和 `GetCastFuncPair` 实现。

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

通用模板实现，用于特定类型的注册器。

```cpp
template<typename T, Utils::TemplateString TypeName>
class LuaAnyObjectNewCastImpl : public LuaAnyObjectNewCastAbstract<LuaAnyObjectNewCastImpl<T, TypeName>>
{
public:
    static int new_func_impl(lua_State* L);
    static int cast_func_impl(lua_State* L);
};
```

## 预定义类型别名

LuaAnyObject 为常用类型提供了预定义的类型别名：

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

## 使用方法

### 1. 注册自定义类型到 Lua

**重要提示**：使用 LuaAnyObject 需要在两个层面进行注册：

1. **C++ 层面**：注册 `TypeInfo` 以支持反射和 `Invoke` 调用
2. **Lua 层面**：注册 Lua 元表以支持在 Lua 中直接创建对象和调用方法

```cpp
#include "utils/LuaAnyObject.hpp"
#include <lua.hpp>

// 假设有一个类
struct Standard
{
    int value;
    Standard(int v = 0) : value(v) {}
    int Add(int x) const { return value + x; }
};

// ========== 第一步：在 C++ 层面注册 TypeInfo ==========
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

// ========== 第二步：在 Lua 层面注册类型 ==========
// 为 Lua 创建 Standard 对象的辅助函数
int lua_standard_new(lua_State* L)
{
    int value = luaL_checkinteger(L, 1);
    Standard* obj = new Standard(value);
    // 使用 LuaAnyObject 包装
    HsBa::Slicer::NewLuaObject<HsBa::Slicer::Utils::AnyObject, "AnyObject">(L, *obj);
    delete obj; // NewLuaObject 已经拷贝，可以删除临时对象
    return 1;
}

// 为 Lua 创建 Standard::add 方法的辅助函数
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

// 为 Lua 创建 Standard 垃圾回收的辅助函数
int lua_standard_gc(lua_State* L)
{
    return HsBa::Slicer::LuaGC<HsBa::Slicer::Utils::AnyObject, "AnyObject">(L);
}

// 注册 Standard 类型到 Lua
void RegisterStandardType(lua_State* L)
{
    // 注册元表
    luaL_newmetatable(L, "Standard");
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, lua_standard_gc);
    lua_setfield(L, -2, "__gc");
    lua_pushcfunction(L, lua_standard_add);
    lua_setfield(L, -2, "add");
    lua_pop(L, 1);

    // 创建 Standard 全局表
    lua_newtable(L);
    lua_pushcfunction(L, lua_standard_new);
    lua_setfield(L, -2, "new");
    lua_setglobal(L, "Standard");
}

// ========== 第三步：使用 RegisterAnyObject 注册 AnyObject 系统 ==========
int main()
{
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    
    // 先注册自定义类型的 Lua 绑定
    RegisterStandardType(L);
    
    // 注册 Standard 类型到 AnyObject 系统
    std::vector<LuaAnyObjectNewCastBase*> types;
    static LuaAnyObjectNewCastImpl<Standard, "Standard"> standard_type;
    types.push_back(&standard_type);
    
    // 注册基本类型
    static LuaInt int_type;
    types.push_back(&int_type);
    static LuaDouble double_type;
    types.push_back(&double_type);
    static LuaString string_type;
    types.push_back(&string_type);
    
    // 执行注册
    RegisterAnyObject(L, types);
    
    // 现在可以在 Lua 中使用 Standard 类型了
    luaL_dostring(L, R"(
        local obj = Standard.new(42)
        local result = obj:add(8)
        print(result) -- 输出：50
    )");
    
    lua_close(L);
    return 0;
}
```

### 2. 在 Lua 中使用注册的类型

```lua
-- 创建 Standard 对象
local standard_obj = Standard.new(42)

-- 调用方法
local add_result = standard_obj:add(8)
print("Add result:", add_result)  -- 输出：50

-- 转换为 AnyObject
local any_obj = AnyObject.new_Standard(standard_obj)

-- 从 AnyObject 转换回 Standard
local back_to_standard = any_obj:cast_Standard()
```

### 3. 使用 AnyObject 进行类型转换

```cpp
// 创建 Lua 状态并注册类型
lua_State* L = luaL_newstate();
luaL_openlibs(L);

std::vector<LuaAnyObjectNewCastBase*> types;
static LuaAnyObjectNewCastImpl<Standard, "Standard"> standard_type;
types.push_back(&standard_type);
static LuaInt int_type;
types.push_back(&int_type);

RegisterAnyObject(L, types);

// 执行 Lua 脚本
const char* script = R"(
    local standard_obj = Standard.new(42)
    local any_obj = AnyObject.new_Standard(standard_obj)
    
    -- 转换为 int 类型
    local int_obj = AnyObject.new_int(100)
    local value = int_obj:cast_int()
    
    -- 调用方法
    local invoke_result = any_obj:invoke("Add", int_obj)
    local result_value = invoke_result:cast_int()
)";

luaL_dostring(L, script);
```

### 4. 完整示例

本示例展示如何为一个包含多个字段的复杂类型（Player）进行完整的注册和使用。

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

// ========== 第一步：注册 C++ TypeInfo ==========
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

// ========== 第二步：注册 Lua 绑定 ==========
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

// ========== 第三步：主函数 ==========
int main()
{
    // 初始化 Lua 状态
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    
    // 先注册 Player 的 Lua 绑定
    RegisterPlayerType(L);
    
    // 注册 Player 类型到 AnyObject 系统
    std::vector<LuaAnyObjectNewCastBase*> types;
    static LuaAnyObjectNewCastImpl<Player, "Player"> player_type;
    types.push_back(&player_type);
    static LuaInt int_type;
    types.push_back(&int_type);
    static LuaFloat float_type;
    types.push_back(&float_type);
    
    RegisterAnyObject(L, types);
    
    // 执行 Lua 脚本
    const char* script = R"(
        -- 创建 Player 对象
        local player = Player.new(100, 5.0)
        
        -- 转换为 AnyObject
        local any_player = AnyObject.new_Player(player)
        
        -- 创建参数对象
        local health_add = AnyObject.new_int(50)
        
        -- 调用方法
        local result = any_player:invoke("AddHealth", health_add)
        local new_health = result:cast_int()
        
        print("New health:", new_health)
        
        -- 遍历字段
        any_player:foreach_field(function(name, value)
            print("Field:", name, "=", value:cast_int())
        end)
    )";
    
    luaL_dostring(L, script);
    
    lua_close(L);
    return 0;
}
```

### 5. 基本类型的使用

```cpp
#include "utils/LuaAnyObject.hpp"
#include <lua.hpp>

int main()
{
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    
    // 注册基本类型
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
    
    // Lua 脚本中使用基本类型
    const char* script = R"(
        -- 创建各种类型的 AnyObject
        local int_obj = AnyObject.new_int(42)
        local double_obj = AnyObject.new_double(3.14)
        local string_obj = AnyObject.new_string("Hello")
        local bool_obj = AnyObject.new_bool(true)
        
        -- 转换回 Lua 类型
        local int_val = int_obj:cast_int()
        local double_val = double_obj:cast_double()
        local string_val = string_obj:cast_string()
        local bool_val = bool_obj:cast_bool()
        
        print("Int:", int_val)          -- 输出：42
        print("Double:", double_val)    -- 输出：3.14
        print("String:", string_val)    -- 输出：Hello
        print("Bool:", bool_val)        -- 输出：true
    )";
    
    luaL_dostring(L, script);
    
    lua_close(L);
    return 0;
}
```

## 注意事项

- **双层注册**：使用自定义类型需要在两个层面进行注册：
  - C++ 层面：注册 `TypeInfo` 以支持反射和 `Invoke` 调用
  - Lua 层面：注册 Lua 元表以支持在 Lua 中直接创建对象和调用方法
  - `RegisterAnyObject` 只负责注册 AnyObject 系统本身，不会自动注册自定义类型的 Lua 绑定
- **TypeInfo 注册**：在使用 `LuaAnyObjectNewCastImpl` 之前，必须先在 `HsBa::Slicer::Utils` 命名空间中为对应类型注册 `TypeInfo`
- **生命周期管理**：确保 Lua 对象的生命周期与 Lua 状态匹配，避免悬空指针
- **类型安全**：`cast_` 函数会在类型不匹配时抛出异常，建议使用 try-catch 包裹
- **静态存储**：类型注册器对象应使用 `static` 存储，确保其在 Lua 状态生命周期内有效
- **内存管理**：使用 `cast_new` 方法避免在 Lua 垃圾回收时的双重析构问题

## 相关组件

- [AnyObject](../base/any_object.md) - 任意对象存储和反射系统
- [LuaNewObject](./luanewobject.md) - Lua 对象创建和管理工具
- [TemplateHelper](../base/template_helper.md) - 模板字符串辅助工具
