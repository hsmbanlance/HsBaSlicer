#define BOOST_TEST_MODULE any_object_test
#include <boost/test/included/unit_test.hpp>

#include "base/any_object.hpp"
#include "utils/LuaAnyObject.hpp"
#include <lua.hpp>

struct Standard
{
    int value;
    int Add(int x) const { return value + x; }
};

namespace HsBa::Slicer::Utils
{
    template<>
    TypeInfo* GetTypeInfo<Standard>()
    {
        static TypeInfo info;
        info.Name = "Standard";
        info.destroy = [](void* data) { delete static_cast<Standard*>(data); };
        info.copy = [](const void* data) -> void* { return new Standard(*static_cast<const Standard*>(data)); };
        info.move = [](void* data) -> void* { return new Standard(std::move(*static_cast<Standard*>(data))); };
        info.fields.clear();
        info.fields.emplace("value", std::make_pair(GetTypeInfo<int>(), 0));
        info.methods.clear();
        info.methods.emplace("Add", type_ensure<&Standard::Add>());
        return &info;
    }

#if defined(_MSC_VER) || defined(__GNUC__) || defined(__clang__)
    struct BaseVirtual
    {
        int value;
        BaseVirtual(int v = 0) : value(v) {}
        virtual int Add(int x) const { return value + x; }
        virtual ~BaseVirtual() = default;
    };

    struct DerivedVirtual : BaseVirtual
    {
        DerivedVirtual(int v = 0) : BaseVirtual(v) {}
        int Add(int x) const override { return value + x + 1; }
    };

    struct BitfieldClass
    {
        int value;
        unsigned flags : 3;
        BitfieldClass(int v = 0, unsigned f = 0) : value(v), flags(f) {}
        int Add(int x) const { return value + x; }
    };

    template<>
    TypeInfo* GetTypeInfo<BaseVirtual>()
    {
        static TypeInfo info;
        info.Name = "BaseVirtual";
        info.destroy = [](void* data) { delete static_cast<BaseVirtual*>(data); };
        info.copy = [](const void* data) -> void* { return new BaseVirtual(*static_cast<const BaseVirtual*>(data)); };
        info.move = [](void* data) -> void* { return new BaseVirtual(std::move(*static_cast<BaseVirtual*>(data))); };
        info.fields.clear();
        info.fields.emplace("value", std::make_pair(GetTypeInfo<int>(), offsetof(BaseVirtual, value)));
        info.methods.clear();
        info.methods.emplace("Add", type_ensure<&BaseVirtual::Add>());
        return &info;
    }

    template<>
    TypeInfo* GetTypeInfo<DerivedVirtual>()
    {
        static TypeInfo info;
        info.Name = "DerivedVirtual";
        info.destroy = [](void* data) { delete static_cast<DerivedVirtual*>(data); };
        info.copy = [](const void* data) -> void* { return new DerivedVirtual(*static_cast<const DerivedVirtual*>(data)); };
        info.move = [](void* data) -> void* { return new DerivedVirtual(std::move(*static_cast<DerivedVirtual*>(data))); };
        info.fields.clear();
        info.fields.emplace("value", std::make_pair(GetTypeInfo<int>(), offsetof(DerivedVirtual, value)));
        info.methods.clear();
        info.methods.emplace("Add", type_ensure<&DerivedVirtual::Add>());
        return &info;
    }

    template<>
    TypeInfo* GetTypeInfo<BitfieldClass>()
    {
        static TypeInfo info;
        info.Name = "BitfieldClass";
        info.destroy = [](void* data) { delete static_cast<BitfieldClass*>(data); };
        info.copy = [](const void* data) -> void* { return new BitfieldClass(*static_cast<const BitfieldClass*>(data)); };
        info.move = [](void* data) -> void* { return new BitfieldClass(std::move(*static_cast<BitfieldClass*>(data))); };
        info.fields.clear();
        info.fields.emplace("value", std::make_pair(GetTypeInfo<int>(), offsetof(BitfieldClass, value)));
        info.methods.clear();
        info.methods.emplace("Add", type_ensure<&BitfieldClass::Add>());
        return &info;
    }
#endif
}

BOOST_AUTO_TEST_SUITE(any_object)

BOOST_AUTO_TEST_CASE(default_anyobject_operations)
{
    using namespace HsBa::Slicer::Utils;
    AnyObject a = 5;
    AnyObject b = a;
    BOOST_CHECK_EQUAL(a.cast<int>(), 5);
    BOOST_CHECK_EQUAL(b.cast<int>(), 5);

    AnyObject c;
    c = std::move(a);
    BOOST_CHECK_EQUAL(c.cast<int>(), 5);
}

BOOST_AUTO_TEST_CASE(non_bitfield_non_virtual_standard_type)
{
    using namespace HsBa::Slicer::Utils;
    Standard src{7};
    AnyObject obj(src);
    BOOST_CHECK_EQUAL(obj.get_type_info()->Name, GetTypeInfo<Standard>()->Name);

    int fieldCount = 0;
    obj.ForeachField([&](std::string_view name, AnyObject value) {
        ++fieldCount;
        if (name == "value")
        {
            BOOST_CHECK_EQUAL(value.cast<int>(), 7);
        }
    });
    BOOST_CHECK_EQUAL(fieldCount, 1);

    AnyObject args[] = { AnyObject(3) };
    AnyObject result = obj.Invoke("Add", args);
    BOOST_CHECK_EQUAL(result.cast<int>(), 10);
}

#if defined(_MSC_VER) || defined(__GNUC__) || defined(__clang__)
BOOST_AUTO_TEST_CASE(custom_GetTypeInfo_with_bitfield)
{
    using namespace HsBa::Slicer::Utils;
    BitfieldClass src{10, 5};
    AnyObject obj(src);
    BOOST_CHECK(obj.get_type_info() == GetTypeInfo<BitfieldClass>());

    int fieldCount = 0;
    obj.ForeachField([&](std::string_view name, AnyObject value) {
        ++fieldCount;
        if (name == "value")
        {
            BOOST_CHECK_EQUAL(value.cast<int>(), 10);
        }
    });
    BOOST_CHECK_EQUAL(fieldCount, 1);

    AnyObject args[] = { AnyObject(2) };
    AnyObject result = obj.Invoke("Add", args);
    BOOST_CHECK_EQUAL(result.cast<int>(), 12);
}

BOOST_AUTO_TEST_CASE(custom_GetTypeInfo_with_virtual_override)
{
    using namespace HsBa::Slicer::Utils;
    DerivedVirtual src{20};
    AnyObject obj(src);

    AnyObject args[] = { AnyObject(3) };
    AnyObject result = obj.Invoke("Add", args);
    BOOST_CHECK_EQUAL(result.cast<int>(), 24); // 20 + 3 + 1 override

    AnyObject copied = obj;
    BOOST_CHECK_EQUAL(copied.cast<DerivedVirtual>().Add(2), 23);
}

BOOST_AUTO_TEST_CASE(compiler_specific_vtable_bitfield)
{
    using namespace HsBa::Slicer::Utils;
    DerivedVirtual src{33};
    AnyObject obj(src);
    BOOST_CHECK_EQUAL(obj.get_type_info()->Name, GetTypeInfo<DerivedVirtual>()->Name);
}
#endif

namespace 
{
    int lua_standard_new(lua_State* L)
    {
		int value = lua_tointeger(L, 1);
        HsBa::Slicer::NewLuaObject<Standard, "Standard">(L, value);
		return 1;
    }

    int lua_standard_gc(lua_State* L)
    {
        HsBa::Slicer::LuaGC<Standard, "Standard">(L);
        return 0;
	}

    int lua_standard_add(lua_State* L)
    {
        auto* obj = (Standard*)lua_topointer(L, 1);
        int x = lua_tointeger(L, 2);
        if (!obj)
        {
            lua_pushstring(L, "Invalid Standard object");
            return lua_error(L);
        }
        int result = obj->Add(x);
        lua_pushinteger(L, result);
        return 1;
	}

    void RegisterStandardType(lua_State* L)
    {
        // Register metatable for Standard
        luaL_newmetatable(L, "Standard");
		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index"); // metatable.__index = metatable
        lua_pushcfunction(L, lua_standard_gc);
        lua_setfield(L, -2, "__gc");
        lua_pushcfunction(L, lua_standard_add);
        lua_setfield(L, -2, "add");
        lua_pop(L, 1); // pop metatable

        // Create Standard table
        lua_newtable(L);
        lua_pushcfunction(L, lua_standard_new);
        lua_setfield(L, -2, "new");
        lua_setglobal(L, "Standard");
    }
}

BOOST_AUTO_TEST_CASE(lua_anyobject_test)
{
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);

    using namespace HsBa::Slicer;

	// Register Standard new and __gc functions first
    RegisterStandardType(L);

    // Register types
    std::vector<LuaAnyObjectNewCastBase*> types;
    static LuaAnyObjectNewCastImpl<Standard, "Standard"> standard_type;
    types.push_back(&standard_type);
    static LuaInt int_type;
    types.push_back(&int_type);
    static LuaDouble double_type;
    types.push_back(&double_type);
    static LuaString string_type;
    types.push_back(&string_type);
    static LuaBool bool_type;
    types.push_back(&bool_type);
	static LuaCString cstring_type;
	types.push_back(&cstring_type);
	static LuaSize_t size_t_type;
	types.push_back(&size_t_type);

    RegisterAnyObject(L, types);

    // Test creating Standard object and invoking methods via Lua script
    const char* test_script = R"(
        local standard_obj = Standard.new(42)
        _G.add_result = standard_obj:add(8)
        local any_obj = AnyObject.new_Standard(standard_obj)
        local add_obj = AnyObject.new_int(8)
        -- lua_Integer defined as long long
        -- any_obj:invoke("Add", 8) throw RuntimeError
        local invoke_result_any = any_obj:invoke("Add", add_obj)
        _G.invoke_result = invoke_result_any:cast_int()
        -- test foreach_field
        any_obj:foreach_field(function(name, value)
            if name == "value" then
                print(type(value)) -- should print "userdata"
                _G.foreach_value = value:cast_int()
            end
        end)
        local any_obj_string = AnyObject.new_string("Hello")
        _G.str = (AnyObject.invoke(any_obj_string, "c_str")):cast_cstring()
        _G.str = _G.str .. " World"
    )";

	if(luaL_dostring(L, test_script) != LUA_OK) {
        const char* error = lua_tostring(L, -1);
        BOOST_FAIL("Lua script error: " << error);
	}

    // Check add result
    lua_getglobal(L, "add_result");
    int add_result = lua_tointeger(L, -1);
    BOOST_CHECK_EQUAL(add_result, 50); // 42 + 8

    // Check invoke result
    lua_getglobal(L, "invoke_result");
    auto invoke_result = lua_tointeger(L, -1);
    BOOST_CHECK_EQUAL(invoke_result, 50); // 42 + 8

	// Check foreach_field result
	lua_getglobal(L, "foreach_value");
	auto foreach_value = lua_tointeger(L, -1);
	BOOST_CHECK_EQUAL(foreach_value, 42); // value field of Standard object

	// Check string result
    lua_getglobal(L, "str");
	auto str_result = lua_tostring(L, -1);
	BOOST_CHECK_EQUAL(str_result, "Hello World");

    lua_close(L);
}

BOOST_AUTO_TEST_SUITE_END()
