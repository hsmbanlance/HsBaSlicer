/**
 * @file LuaAdapter.cpp
 * @brief Lua bindings for the file-operator layer: zipper/unzipper, SQL adapters, and the
 *        ParamStore process-parameter pipeline.
 */
#include "LuaAdapter.hpp"
#include "param_reflect.hpp"
#include "param_schema.hpp"
#include "param_store.hpp"
#include "sql_adapter.hpp"
#include <format>
#include <string>
#include <unordered_map>

namespace HsBa::Slicer
{
void PushAnyToLua(lua_State* L, const std::any& value)
{
    if (!value.has_value())
    {
        lua_pushnil(L);
        return;
    }
    if (value.type() == typeid(int))
    {
        lua_pushinteger(L, std::any_cast<int>(value));
    }
    else if (value.type() == typeid(int64_t))
    {
        lua_pushinteger(L, static_cast<lua_Integer>(std::any_cast<int64_t>(value)));
    }
    else if (value.type() == typeid(double))
    {
        lua_pushnumber(L, std::any_cast<double>(value));
    }
    else if (value.type() == typeid(std::string))
    {
        lua_pushstring(L, std::any_cast<std::string>(value).c_str());
    }
    else if (value.type() == typeid(std::vector<unsigned char>))
    {
        const auto& vec = std::any_cast<std::vector<unsigned char>>(value);
        lua_pushlstring(L, reinterpret_cast<const char*>(vec.data()), vec.size());
    }
    else
    {
        lua_pushnil(L);  // unsupported type
    }
}
namespace
{
// ============= Zipper Wrapper =============
constexpr Utils::TemplateString ZipperTypeName = "Zipper";
int lua_zipper_new(lua_State* L)
{
    NewLuaObject<Zipper, ZipperTypeName>(L);
    return 1;
}

int lua_zipper_add_file(lua_State* L)
{
    auto* zipper = (Zipper*)lua_topointer(L, 1);
    std::string_view const name = luaL_checkstring(L, 2);
    std::string_view const path = luaL_checkstring(L, 3);
    if (!zipper)
    {
        return raise_lua_error(L, "Invalid {} object", ZipperTypeName);
    }
    try
    {
        zipper->AddFile(name, path);
        lua_pushboolean(L, 1);
        return 1;
    }
    catch (const RuntimeError& e)
    {
        lua_pushstring(L, e.what());
    }
    return lua_error(L);
}

int lua_zipper_add_byte_file(lua_State* L)
{
    auto* zipper = (Zipper*)lua_topointer(L, 1);
    std::string_view const name = luaL_checkstring(L, 2);
    std::string_view const data = luaL_checkstring(L, 3);
    if (!zipper)
    {
        return raise_lua_error(L, "Invalid {} object", ZipperTypeName);
    }
    try
    {
        zipper->AddByteFile(name, std::string(data));
        lua_pushboolean(L, 1);
        return 1;
    }
    catch (const RuntimeError& e)
    {
        lua_pushstring(L, e.what());
    }
    return lua_error(L);
}

int lua_zipper_save(lua_State* L)
{
    auto* zipper = (Zipper*)lua_topointer(L, 1);
    std::string_view const path = luaL_checkstring(L, 2);
    if (!zipper)
    {
        return raise_lua_error(L, "Invalid {} object", ZipperTypeName);
    }
    try
    {
        zipper->Save(path);
        lua_pushboolean(L, 1);
        return 1;
    }
    catch (const RuntimeError& e)
    {
        lua_pushstring(L, e.what());
    }
    return lua_error(L);
}

int lua_zipper_gc(lua_State* L)
{
    LuaGC<Zipper, ZipperTypeName>(L);
    return 0;
}

const luaL_Reg zipperLib[] = {{.name = "new", .func = lua_zipper_new},
                              {.name = "AddFile", .func = lua_zipper_add_file},
                              {.name = "AddByteFile", .func = lua_zipper_add_byte_file},
                              {.name = "Save", .func = lua_zipper_save},
                              {.name = "__gc", .func = lua_zipper_gc},
                              {.name = nullptr, .func = nullptr}};


#ifdef HSBA_USE_BIT7Z
// ============= Bit7zZipper Wrapper =============
constexpr Utils::TemplateString Bit7zZipperTypeName = "Bit7zZipper";
int lua_bit7z_zipper_new(lua_State* L)
{
    // first arg: format
    luaL_checktype(L, 1, LUA_TSTRING);
    std::string_view const format_str = luaL_checkstring(L, 1);
    ZipperFormat zipper_format = ZipperFormat::Undefine;
    if (format_str == "Zip")
    {
        {
            zipper_format = ZipperFormat::Zip;
        }
    }
    else if (format_str == "SevenZip")
    {
        {
            zipper_format = ZipperFormat::SevenZip;
        }
    }
    else if (format_str == "XZ")
    {
        {
            zipper_format = ZipperFormat::XZ;
        }
    }
    else if (format_str == "BZIP2")
    {
        {
            zipper_format = ZipperFormat::BZIP2;
        }
    }
    else if (format_str == "GZIP")
    {
        {
            zipper_format = ZipperFormat::GZIP;
        }
    }
    else if (format_str == "TAR")
    {
        {
            zipper_format = ZipperFormat::TAR;
        }
    }
    else if (format_str == "TarGz")
    {
        {
            zipper_format = ZipperFormat::TarGz;
        }
    }
    else if (format_str == "TarXz")
    {
        {
            zipper_format = ZipperFormat::TarXz;
        }
    }
    else
    {
        lua_pushstring(L, "Unsupported Bit7z Zipper format");
        return lua_error(L);
    }
    // second arg: dll_path
    std::string dll_path = HSBA_7Z_DLL;
    if (lua_gettop(L) >= 2 && lua_isstring(L, 2))
    {
        dll_path = luaL_checkstring(L, 2);
    }
    // third arg: password (optional)
    std::string password;
    if (lua_gettop(L) >= 3 && lua_isstring(L, 3))
    {
        password = luaL_checkstring(L, 3);
    }
    NewLuaObject<Bit7zZipper, Bit7zZipperTypeName>(L, dll_path, zipper_format, password);
    return 1;
}

int lua_bit7z_zipper_add_file(lua_State* L)
{
    auto* zipper = (Bit7zZipper*)lua_topointer(L, 1);
    std::string_view const name = luaL_checkstring(L, 2);
    std::string_view const path = luaL_checkstring(L, 3);
    if (!zipper)
    {
        return raise_lua_error(L, "Invalid {} object", Bit7zZipperTypeName);
    }
    try
    {
        zipper->AddFile(name, path);
        lua_pushboolean(L, 1);
        return 1;
    }
    catch (const RuntimeError& e)
    {
        lua_pushstring(L, e.what());
    }
    return lua_error(L);
}

int lua_bit7z_zipper_add_byte_file(lua_State* L)
{
    auto* zipper = (Bit7zZipper*)lua_topointer(L, 1);
    std::string_view const name = luaL_checkstring(L, 2);
    size_t len = 0;
    const char* data = luaL_checklstring(L, 3, &len);
    if (!zipper)
    {
        return raise_lua_error(L, "Invalid {} object", Bit7zZipperTypeName);
    }
    try
    {
        zipper->AddByteFile(name, std::vector<unsigned char>(data, data + len));
        lua_pushboolean(L, 1);
        return 1;
    }
    catch (const RuntimeError& e)
    {
        lua_pushstring(L, e.what());
    }
    return lua_error(L);
}

int lua_bit7z_zipper_save(lua_State* L)
{
    auto* zipper = (Bit7zZipper*)lua_topointer(L, 1);
    std::string_view const path = luaL_checkstring(L, 2);
    if (!zipper)
    {
        return raise_lua_error(L, "Invalid {} object", Bit7zZipperTypeName);
    }
    try
    {
        zipper->Save(path);
        lua_pushboolean(L, 1);
        return 1;
    }
    catch (const RuntimeError& e)
    {
        lua_pushstring(L, e.what());
    }
    return lua_error(L);
}

int lua_bit7z_zipper_gc(lua_State* L)
{
    LuaGC<Bit7zZipper, Bit7zZipperTypeName>(L);
    return 0;
}

const luaL_Reg bit7z_zipperLib[] = {{.name = "new", .func = lua_bit7z_zipper_new},
                                    {.name = "AddFile", .func = lua_bit7z_zipper_add_file},
                                    {.name = "AddByteFile", .func = lua_bit7z_zipper_add_byte_file},
                                    {.name = "Save", .func = lua_bit7z_zipper_save},
                                    {.name = "__gc", .func = lua_bit7z_zipper_gc},
                                    {.name = nullptr, .func = nullptr}};

#endif  // HSBA_USE_BIT7Z

// ============= SQLiteAdapter Wrapper =============
constexpr Utils::TemplateString SQLiteAdapterTypeName = "SQLiteAdapter";
int lua_sqlite_new(lua_State* L)
{
    NewLuaObject<SQL::SQLiteAdapter, SQLiteAdapterTypeName>(L);
    return 1;
}

int lua_sqlite_connect(lua_State* L)
{
    auto* db = (SQL::SQLiteAdapter*)lua_topointer(L, 1);
    std::string_view const path = luaL_checkstring(L, 2);
    if (!db)
    {
        return raise_lua_error(L, "Invalid {} object", SQLiteAdapterTypeName);
    }
    try
    {
        db->Connect(path);
        lua_pushboolean(L, 1);
        return 1;
    }
    catch (const RuntimeError& e)
    {
        lua_pushstring(L, e.what());
    }
    return lua_error(L);
}

int lua_sqlite_execute(lua_State* L)
{
    auto* db = (SQL::SQLiteAdapter*)lua_topointer(L, 1);
    std::string_view const query = luaL_checkstring(L, 2);
    if (!db)
    {
        return raise_lua_error(L, "Invalid {} object", SQLiteAdapterTypeName);
    }
    try
    {
        db->Execute(std::string(query));
        lua_pushboolean(L, 1);
        return 1;
    }
    catch (const RuntimeError& e)
    {
        lua_pushstring(L, e.what());
    }
    return lua_error(L);
}

int lua_sqlite_query(lua_State* L)
{
    auto* db = (SQL::SQLiteAdapter*)lua_topointer(L, 1);
    std::string_view const query = luaL_checkstring(L, 2);
    if (!db)
    {
        return raise_lua_error(L, "Invalid {} object", SQLiteAdapterTypeName);
    }
    try
    {
        auto rows = db->Query(std::string(query));
        lua_createtable(L, static_cast<int>(rows.size()), 0);
        for (size_t i = 0; i < rows.size(); ++i)
        {
            lua_createtable(L, 0, static_cast<int>(rows[i].size()));
            for (const auto& [key, val] : rows[i])
            {
                PushAnyToLua(L, val);
                lua_setfield(L, -2, key.c_str());
            }
            lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
        }
        return 1;
    }
    catch (const RuntimeError& e)
    {
        lua_pushstring(L, e.what());
    }
    return lua_error(L);
}

int lua_sqlite_insert(lua_State* L)
{
    auto* db = (SQL::SQLiteAdapter*)lua_topointer(L, 1);
    std::string_view const table = luaL_checkstring(L, 2);
    luaL_checktype(L, 3, LUA_TTABLE);
    if (!db)
    {
        return raise_lua_error(L, "Invalid {} object", SQLiteAdapterTypeName);
    }
    try
    {
        std::unordered_map<std::string, std::any> data;
        lua_pushnil(L);
        while (lua_next(L, 3))
        {
            std::string const key = lua_tostring(L, -2);
            std::any val;
            if (lua_isnumber(L, -1))
            {
                val = static_cast<int64_t>(lua_tointeger(L, -1));
            }
            else if (lua_isstring(L, -1))
            {
                val = std::string(lua_tostring(L, -1));
            }
            else if (lua_isboolean(L, -1))
            {
                val = static_cast<int64_t>(lua_toboolean(L, -1) ? 1 : 0);
            }
            data[key] = val;
            lua_pop(L, 1);
        }
        db->Insert(std::string(table), data);
        lua_pushboolean(L, 1);
        return 1;
    }
    catch (const RuntimeError& e)
    {
        lua_pushstring(L, e.what());
    }
    return lua_error(L);
}

int lua_sqlite_update(lua_State* L)
{
    auto* db = (SQL::SQLiteAdapter*)lua_topointer(L, 1);
    std::string_view const table = luaL_checkstring(L, 2);
    luaL_checktype(L, 3, LUA_TTABLE);
    luaL_checktype(L, 4, LUA_TTABLE);
    if (!db)
    {
        return raise_lua_error(L, "Invalid {} object", SQLiteAdapterTypeName);
    }
    try
    {
        auto parse_table = [](lua_State* L, int idx) -> std::unordered_map<std::string, std::any>
        {
            std::unordered_map<std::string, std::any> result;
            lua_pushnil(L);
            while (lua_next(L, idx))
            {
                std::string const key = lua_tostring(L, -2);
                std::any val;
                if (lua_isnumber(L, -1))
                {
                    val = static_cast<int64_t>(lua_tointeger(L, -1));
                }
                else if (lua_isstring(L, -1))
                {
                    val = std::string(lua_tostring(L, -1));
                }
                result[key] = val;
                lua_pop(L, 1);
            }
            return result;
        };
        auto set_data = parse_table(L, 3);
        auto where_data = parse_table(L, 4);
        db->Update(std::string(table), set_data, where_data);
        lua_pushboolean(L, 1);
        return 1;
    }
    catch (const RuntimeError& e)
    {
        lua_pushstring(L, e.what());
    }
    return lua_error(L);
}

int lua_sqlite_delete(lua_State* L)
{
    auto* db = (SQL::SQLiteAdapter*)lua_topointer(L, 1);
    std::string_view const table = luaL_checkstring(L, 2);
    luaL_checktype(L, 3, LUA_TTABLE);
    if (!db)
    {
        return raise_lua_error(L, "Invalid {} object", SQLiteAdapterTypeName);
    }
    try
    {
        std::unordered_map<std::string, std::any> data;
        lua_pushnil(L);
        while (lua_next(L, 3))
        {
            std::string const key = lua_tostring(L, -2);
            std::any val;
            if (lua_isnumber(L, -1))
            {
                val = static_cast<int64_t>(lua_tointeger(L, -1));
            }
            else if (lua_isstring(L, -1))
            {
                val = std::string(lua_tostring(L, -1));
            }
            data[key] = val;
            lua_pop(L, 1);
        }
        db->Delete(std::string(table), data);
        lua_pushboolean(L, 1);
        return 1;
    }
    catch (const RuntimeError& e)
    {
        lua_pushstring(L, e.what());
    }
    return lua_error(L);
}

int lua_sqlite_create_table(lua_State* L)
{
    auto* db = (SQL::SQLiteAdapter*)lua_topointer(L, 1);
    std::string_view const table = luaL_checkstring(L, 2);
    luaL_checktype(L, 3, LUA_TTABLE);
    if (!db)
    {
        return raise_lua_error(L, "Invalid {} object", SQLiteAdapterTypeName);
    }
    try
    {
        std::unordered_map<std::string, std::string> columns;
        lua_pushnil(L);
        while (lua_next(L, 3))
        {
            std::string const key = lua_tostring(L, -2);
            std::string const col_type = lua_tostring(L, -1);
            columns[key] = col_type;
            lua_pop(L, 1);
        }
        db->CreateTable(std::string(table), columns);
        lua_pushboolean(L, 1);
        return 1;
    }
    catch (const RuntimeError& e)
    {
        lua_pushstring(L, e.what());
    }
    return lua_error(L);
}

int lua_sqlite_gc(lua_State* L)
{
    LuaGC<SQL::SQLiteAdapter, SQLiteAdapterTypeName>(L);
    return 0;
}

const luaL_Reg sqlite_adapter_methods[] = {{.name = "Connect", .func = lua_sqlite_connect},
                                           {.name = "Execute", .func = lua_sqlite_execute},
                                           {.name = "Query", .func = lua_sqlite_query},
                                           {.name = "Insert", .func = lua_sqlite_insert},
                                           {.name = "Update", .func = lua_sqlite_update},
                                           {.name = "Delete", .func = lua_sqlite_delete},
                                           {.name = "CreateTable", .func = lua_sqlite_create_table},
                                           {.name = "__gc", .func = lua_sqlite_gc},
                                           {.name = nullptr, .func = nullptr}};

#ifdef HSBA_USE_MYSQL
// ============= MySQLAdapter Wrapper =============
constexpr Utils::TemplateString MySQLAdapterTypeName = "MySQLAdapter";
int lua_mysql_new(lua_State* L)
{
    NewLuaObject<SQL::MySQLAdapter, MySQLAdapterTypeName>(L);
    return 1;
}
int lua_mysql_connect(lua_State* L)
{
    auto* db = (SQL::MySQLAdapter*)lua_topointer(L, 1);
    // first host
    std::string_view const host = luaL_checkstring(L, 2);
    // second user
    std::string_view const user = luaL_checkstring(L, 3);
    // third password
    std::string_view const password = luaL_checkstring(L, 4);
    // fourth database
    std::string_view const database = luaL_checkstring(L, 5);
    // fifth port (optional)
    unsigned int port = SQL::MYSQL_DEFAULT_PORT;
    if (lua_gettop(L) >= 6 && lua_isinteger(L, 6))
    {
        port = static_cast<unsigned int>(lua_tointeger(L, 6));
    }
    if (!db)
    {
        return raise_lua_error(L, "Invalid {} object", MySQLAdapterTypeName);
    }
    try
    {
        db->Connect(host, user, password, database, port);
        lua_pushboolean(L, 1);
        return 1;
    }
    catch (const RuntimeError& e)
    {
        lua_pushstring(L, e.what());
    }
    return lua_error(L);
}
int lua_mysql_execute(lua_State* L)
{
    auto* db = (SQL::MySQLAdapter*)lua_topointer(L, 1);
    std::string_view const query = luaL_checkstring(L, 2);
    if (!db)
    {
        return raise_lua_error(L, "Invalid {} object", MySQLAdapterTypeName);
    }
    try
    {
        db->Execute(std::string(query));
        lua_pushboolean(L, 1);
        return 1;
    }
    catch (const RuntimeError& e)
    {
        lua_pushstring(L, e.what());
    }
    return lua_error(L);
}
int lua_mysql_query(lua_State* L)
{
    auto* db = (SQL::MySQLAdapter*)lua_topointer(L, 1);
    std::string_view const query = luaL_checkstring(L, 2);
    if (!db)
    {
        return raise_lua_error(L, "Invalid {} object", MySQLAdapterTypeName);
    }
    try
    {
        auto rows = db->Query(std::string(query));
        lua_createtable(L, static_cast<int>(rows.size()), 0);
        for (size_t i = 0; i < rows.size(); ++i)
        {
            lua_createtable(L, 0, static_cast<int>(rows[i].size()));
            for (const auto& [key, val] : rows[i])
            {
                PushAnyToLua(L, val);
                lua_setfield(L, -2, key.c_str());
            }
            lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
        }
        return 1;
    }
    catch (const RuntimeError& e)
    {
        lua_pushstring(L, e.what());
    }
    return lua_error(L);
}
int lua_mysql_insert(lua_State* L)
{
    auto* db = (SQL::MySQLAdapter*)lua_topointer(L, 1);
    std::string_view const table = luaL_checkstring(L, 2);
    luaL_checktype(L, 3, LUA_TTABLE);
    if (!db)
    {
        return raise_lua_error(L, "Invalid {} object", MySQLAdapterTypeName);
    }
    try
    {
        std::unordered_map<std::string, std::any> data;
        lua_pushnil(L);
        while (lua_next(L, 3))
        {
            std::string const key = lua_tostring(L, -2);
            std::any val;
            if (lua_isnumber(L, -1))
            {
                val = static_cast<int64_t>(lua_tointeger(L, -1));
            }
            else if (lua_isstring(L, -1))
            {
                val = std::string(lua_tostring(L, -1));
            }
            else if (lua_isboolean(L, -1))
            {
                val = static_cast<int64_t>(lua_toboolean(L, -1) ? 1 : 0);
            }
            data[key] = val;
            lua_pop(L, 1);
        }
        db->Insert(std::string(table), data);
        lua_pushboolean(L, 1);
        return 1;
    }
    catch (const RuntimeError& e)
    {
        lua_pushstring(L, e.what());
    }
    return lua_error(L);
}
int lua_mysql_update(lua_State* L)
{
    auto* db = (SQL::MySQLAdapter*)lua_topointer(L, 1);
    std::string_view const table = luaL_checkstring(L, 2);
    luaL_checktype(L, 3, LUA_TTABLE);
    luaL_checktype(L, 4, LUA_TTABLE);
    if (!db)
    {
        return raise_lua_error(L, "Invalid {} object", MySQLAdapterTypeName);
    }
    try
    {
        auto parse_table = [](lua_State* L, int idx) -> std::unordered_map<std::string, std::any>
        {
            std::unordered_map<std::string, std::any> result;
            lua_pushnil(L);
            while (lua_next(L, idx))
            {
                std::string const key = lua_tostring(L, -2);
                std::any val;
                if (lua_isnumber(L, -1))
                {
                    val = static_cast<int64_t>(lua_tointeger(L, -1));
                }
                else if (lua_isstring(L, -1))
                {
                    val = std::string(lua_tostring(L, -1));
                }
                result[key] = val;
                lua_pop(L, 1);
            }
            return result;
        };
        auto set_data = parse_table(L, 3);
        auto where_data = parse_table(L, 4);
        db->Update(std::string(table), set_data, where_data);
        lua_pushboolean(L, 1);
        return 1;
    }
    catch (const RuntimeError& e)
    {
        lua_pushstring(L, e.what());
    }
    return lua_error(L);
}
int lua_mysql_delete(lua_State* L)
{
    auto* db = (SQL::MySQLAdapter*)lua_topointer(L, 1);
    std::string_view const table = luaL_checkstring(L, 2);
    luaL_checktype(L, 3, LUA_TTABLE);
    if (!db)
    {
        return raise_lua_error(L, "Invalid {} object", MySQLAdapterTypeName);
    }
    try
    {
        std::unordered_map<std::string, std::any> data;
        lua_pushnil(L);
        while (lua_next(L, 3))
        {
            std::string const key = lua_tostring(L, -2);
            std::any val;
            if (lua_isnumber(L, -1))
            {
                val = static_cast<int64_t>(lua_tointeger(L, -1));
            }
            else if (lua_isstring(L, -1))
            {
                val = std::string(lua_tostring(L, -1));
            }
            data[key] = val;
            lua_pop(L, 1);
        }
        db->Delete(std::string(table), data);
        lua_pushboolean(L, 1);
        return 1;
    }
    catch (const RuntimeError& e)
    {
        lua_pushstring(L, e.what());
    }
    return lua_error(L);
}
int lua_mysql_create_table(lua_State* L)
{
    auto* db = (SQL::MySQLAdapter*)lua_topointer(L, 1);
    std::string_view const table = luaL_checkstring(L, 2);
    luaL_checktype(L, 3, LUA_TTABLE);
    if (!db)
    {
        return raise_lua_error(L, "Invalid {} object", MySQLAdapterTypeName);
    }
    try
    {
        std::unordered_map<std::string, std::string> columns;
        lua_pushnil(L);
        while (lua_next(L, 3))
        {
            std::string const key = lua_tostring(L, -2);
            std::string const col_type = lua_tostring(L, -1);
            columns[key] = col_type;
            lua_pop(L, 1);
        }
        db->CreateTable(std::string(table), columns);
        lua_pushboolean(L, 1);
        return 1;
    }
    catch (const RuntimeError& e)
    {
        lua_pushstring(L, e.what());
    }
    return lua_error(L);
}
int lua_mysql_gc(lua_State* L)
{
    LuaGC<SQL::MySQLAdapter, MySQLAdapterTypeName>(L);
    return 0;
}
const luaL_Reg mysql_adapter_methods[] = {{.name = "Connect", .func = lua_mysql_connect},
                                          {.name = "Execute", .func = lua_mysql_execute},
                                          {.name = "Query", .func = lua_mysql_query},
                                          {.name = "Insert", .func = lua_mysql_insert},
                                          {.name = "Update", .func = lua_mysql_update},
                                          {.name = "Delete", .func = lua_mysql_delete},
                                          {.name = "CreateTable", .func = lua_mysql_create_table},
                                          {.name = "__gc", .func = lua_mysql_gc},
                                          {.name = nullptr, .func = nullptr}};
#endif  // HSBA_USE_MYSQL
#ifdef HSBA_USE_PGSQL
// ============= PostgreSQLAdapter Wrapper =============
// Similar implementation as MySQLAdapter can be done here
constexpr Utils::TemplateString PostgreSQLAdapterTypeName = "PostgreSQLAdapter";
int lua_pgsql_new(lua_State* L)
{
    NewLuaObject<SQL::PostgreSQLAdapter, PostgreSQLAdapterTypeName>(L);
    return 1;
}
int lua_pgsql_connect(lua_State* L)
{
    auto* db = (SQL::PostgreSQLAdapter*)lua_topointer(L, 1);
    // first host
    std::string_view const host = luaL_checkstring(L, 2);
    // second user
    std::string_view const user = luaL_checkstring(L, 3);
    // third password
    std::string_view const password = luaL_checkstring(L, 4);
    // fourth database
    std::string_view const database = luaL_checkstring(L, 5);
    // fifth port (optional)
    unsigned int port = SQL::POSTGRESQL_DEFAULT_PORT;
    if (lua_gettop(L) >= 6 && lua_isinteger(L, 6))
    {
        port = static_cast<unsigned int>(lua_tointeger(L, 6));
    }
    if (!db)
    {
        return raise_lua_error(L, "Invalid {} object", PostgreSQLAdapterTypeName);
    }
    try
    {
        db->Connect(host, user, password, database, port);
        lua_pushboolean(L, 1);
        return 1;
    }
    catch (const RuntimeError& e)
    {
        lua_pushstring(L, e.what());
    }
    return lua_error(L);
    return 0;
}
int lua_pgsql_execute(lua_State* L)
{
    auto* db = (SQL::PostgreSQLAdapter*)lua_topointer(L, 1);
    std::string_view const query = luaL_checkstring(L, 2);
    if (!db)
    {
        return raise_lua_error(L, "Invalid {} object", PostgreSQLAdapterTypeName);
    }
    try
    {
        db->Execute(std::string(query));
        lua_pushboolean(L, 1);
        return 1;
    }
    catch (const RuntimeError& e)
    {
        lua_pushstring(L, e.what());
    }
    return lua_error(L);
}
int lua_pgsql_query(lua_State* L)
{
    auto* db = (SQL::PostgreSQLAdapter*)lua_topointer(L, 1);
    std::string_view const query = luaL_checkstring(L, 2);
    if (!db)
    {
        return raise_lua_error(L, "Invalid {} object", PostgreSQLAdapterTypeName);
    }
    try
    {
        auto rows = db->Query(std::string(query));
        lua_createtable(L, static_cast<int>(rows.size()), 0);
        for (size_t i = 0; i < rows.size(); ++i)
        {
            lua_createtable(L, 0, static_cast<int>(rows[i].size()));
            for (const auto& [key, val] : rows[i])
            {
                PushAnyToLua(L, val);
                lua_setfield(L, -2, key.c_str());
            }
            lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
        }
        return 1;
    }
    catch (const RuntimeError& e)
    {
        lua_pushstring(L, e.what());
    }
    return lua_error(L);
}
int lua_pgsql_insert(lua_State* L)
{
    auto* db = (SQL::PostgreSQLAdapter*)lua_topointer(L, 1);
    std::string_view const table = luaL_checkstring(L, 2);
    luaL_checktype(L, 3, LUA_TTABLE);
    if (!db)
    {
        return raise_lua_error(L, "Invalid {} object", PostgreSQLAdapterTypeName);
    }
    try
    {
        std::unordered_map<std::string, std::any> data;
        lua_pushnil(L);
        while (lua_next(L, 3))
        {
            std::string const key = lua_tostring(L, -2);
            std::any val;
            if (lua_isnumber(L, -1))
            {
                val = static_cast<int64_t>(lua_tointeger(L, -1));
            }
            else if (lua_isstring(L, -1))
            {
                val = std::string(lua_tostring(L, -1));
            }
            else if (lua_isboolean(L, -1))
            {
                val = static_cast<int64_t>(lua_toboolean(L, -1) ? 1 : 0);
            }
            data[key] = val;
            lua_pop(L, 1);
        }
        db->Insert(std::string(table), data);
        lua_pushboolean(L, 1);
        return 1;
    }
    catch (const RuntimeError& e)
    {
        lua_pushstring(L, e.what());
    }
    return lua_error(L);
}
int lua_pgsql_update(lua_State* L)
{
    auto* db = (SQL::PostgreSQLAdapter*)lua_topointer(L, 1);
    std::string_view const table = luaL_checkstring(L, 2);
    luaL_checktype(L, 3, LUA_TTABLE);
    luaL_checktype(L, 4, LUA_TTABLE);
    if (!db)
    {
        return raise_lua_error(L, "Invalid {} object", PostgreSQLAdapterTypeName);
    }
    try
    {
        auto parse_table = [](lua_State* L, int idx) -> std::unordered_map<std::string, std::any>
        {
            std::unordered_map<std::string, std::any> result;
            lua_pushnil(L);
            while (lua_next(L, idx))
            {
                std::string const key = lua_tostring(L, -2);
                std::any val;
                if (lua_isnumber(L, -1))
                {
                    val = static_cast<int64_t>(lua_tointeger(L, -1));
                }
                else if (lua_isstring(L, -1))
                {
                    val = std::string(lua_tostring(L, -1));
                }
                result[key] = val;
                lua_pop(L, 1);
            }
            return result;
        };
        auto set_data = parse_table(L, 3);
        auto where_data = parse_table(L, 4);
        db->Update(std::string(table), set_data, where_data);
        lua_pushboolean(L, 1);
        return 1;
    }
    catch (const RuntimeError& e)
    {
        lua_pushstring(L, e.what());
    }
    return lua_error(L);
}
int lua_pgsql_delete(lua_State* L)
{
    auto* db = (SQL::PostgreSQLAdapter*)lua_topointer(L, 1);
    std::string_view const table = luaL_checkstring(L, 2);
    luaL_checktype(L, 3, LUA_TTABLE);
    if (!db)
    {
        return raise_lua_error(L, "Invalid {} object", PostgreSQLAdapterTypeName);
    }
    try
    {
        std::unordered_map<std::string, std::any> data;
        lua_pushnil(L);
        while (lua_next(L, 3))
        {
            std::string const key = lua_tostring(L, -2);
            std::any val;
            if (lua_isnumber(L, -1))
            {
                val = static_cast<int64_t>(lua_tointeger(L, -1));
            }
            else if (lua_isstring(L, -1))
            {
                val = std::string(lua_tostring(L, -1));
            }
            data[key] = val;
            lua_pop(L, 1);
        }
        db->Delete(std::string(table), data);
        lua_pushboolean(L, 1);
        return 1;
    }
    catch (const RuntimeError& e)
    {
        lua_pushstring(L, e.what());
    }
    return lua_error(L);
}
int lua_pgsql_create_table(lua_State* L)
{
    auto* db = (SQL::PostgreSQLAdapter*)lua_topointer(L, 1);
    std::string_view const table = luaL_checkstring(L, 2);
    luaL_checktype(L, 3, LUA_TTABLE);
    if (!db)
    {
        return raise_lua_error(L, "Invalid {} object", PostgreSQLAdapterTypeName);
    }
    try
    {
        std::unordered_map<std::string, std::string> columns;
        lua_pushnil(L);
        while (lua_next(L, 3))
        {
            std::string const key = lua_tostring(L, -2);
            std::string const col_type = lua_tostring(L, -1);
            columns[key] = col_type;
            lua_pop(L, 1);
        }
        db->CreateTable(std::string(table), columns);
        lua_pushboolean(L, 1);
        return 1;
    }
    catch (const RuntimeError& e)
    {
        lua_pushstring(L, e.what());
    }
    return lua_error(L);
}
int lua_pgsql_gc(lua_State* L)
{
    LuaGC<SQL::PostgreSQLAdapter, PostgreSQLAdapterTypeName>(L);
    return 0;
}
const luaL_Reg pgsql_adapter_methods[] = {{.name = "Connect", .func = lua_pgsql_connect},
                                          {.name = "Execute", .func = lua_pgsql_execute},
                                          {.name = "Query", .func = lua_pgsql_query},
                                          {.name = "Insert", .func = lua_pgsql_insert},
                                          {.name = "Update", .func = lua_pgsql_update},
                                          {.name = "Delete", .func = lua_pgsql_delete},
                                          {.name = "CreateTable", .func = lua_pgsql_create_table},
                                          {.name = "__gc", .func = lua_pgsql_gc},
                                          {.name = nullptr, .func = nullptr}};
#endif  // HSBA_USE_PGSQL

}  // namespace

void RegisterLuaZipper(lua_State* L)
{
    // Create metatable for Zipper
    luaL_newmetatable(L, static_cast<const char*>(ZipperTypeName));

    // Set up metatable methods
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, lua_zipper_gc);
    lua_setfield(L, -2, "__gc");

    // Add methods to metatable
    lua_pushcfunction(L, lua_zipper_add_file);
    lua_setfield(L, -2, "AddFile");
    lua_pushcfunction(L, lua_zipper_add_byte_file);
    lua_setfield(L, -2, "AddByteFile");
    lua_pushcfunction(L, lua_zipper_save);
    lua_setfield(L, -2, "Save");

    // Pop metatable
    lua_pop(L, 1);

    // Create Zipper global table
    lua_newtable(L);

    // Add static methods to the global table
    lua_pushcfunction(L, lua_zipper_new);
    lua_setfield(L, -2, "new");
    lua_pushcfunction(L, lua_zipper_add_file);
    lua_setfield(L, -2, "AddFile");
    lua_pushcfunction(L, lua_zipper_add_byte_file);
    lua_setfield(L, -2, "AddByteFile");
    lua_pushcfunction(L, lua_zipper_save);
    lua_setfield(L, -2, "Save");

    lua_setglobal(L, "Zipper");
}

#ifdef HSBA_USE_BIT7Z
void RegisterLuaBit7zZipper(lua_State* L)
{
    // Create metatable for Bit7zZipper
    luaL_newmetatable(L, static_cast<const char*>(Bit7zZipperTypeName));

    // Set up metatable methods
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, lua_bit7z_zipper_gc);
    lua_setfield(L, -2, "__gc");

    // Add methods to metatable
    lua_pushcfunction(L, lua_bit7z_zipper_add_file);
    lua_setfield(L, -2, "AddFile");
    lua_pushcfunction(L, lua_bit7z_zipper_add_byte_file);
    lua_setfield(L, -2, "AddByteFile");
    lua_pushcfunction(L, lua_bit7z_zipper_save);
    lua_setfield(L, -2, "Save");

    // Pop metatable
    lua_pop(L, 1);

    // Create Bit7zZipper global table
    lua_newtable(L);

    // Add static methods to the global table
    lua_pushcfunction(L, lua_bit7z_zipper_new);
    lua_setfield(L, -2, "new");
    lua_pushcfunction(L, lua_bit7z_zipper_add_file);
    lua_setfield(L, -2, "AddFile");
    lua_pushcfunction(L, lua_bit7z_zipper_add_byte_file);
    lua_setfield(L, -2, "AddByteFile");
    lua_pushcfunction(L, lua_bit7z_zipper_save);
    lua_setfield(L, -2, "Save");

    lua_setglobal(L, "Bit7zZipper");
}
#endif  // HSBA_USE_BIT7Z

void RegisterLuaSQLiteAdapter(lua_State* L)
{
    // Create metatable for SQLiteAdapter
    luaL_newmetatable(L, static_cast<const char*>(SQLiteAdapterTypeName));

    // Set up metatable methods
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, lua_sqlite_gc);
    lua_setfield(L, -2, "__gc");

    // Add methods to metatable
    lua_pushcfunction(L, lua_sqlite_connect);
    lua_setfield(L, -2, "Connect");
    lua_pushcfunction(L, lua_sqlite_execute);
    lua_setfield(L, -2, "Execute");
    lua_pushcfunction(L, lua_sqlite_query);
    lua_setfield(L, -2, "Query");
    lua_pushcfunction(L, lua_sqlite_insert);
    lua_setfield(L, -2, "Insert");
    lua_pushcfunction(L, lua_sqlite_update);
    lua_setfield(L, -2, "Update");
    lua_pushcfunction(L, lua_sqlite_delete);
    lua_setfield(L, -2, "Delete");
    lua_pushcfunction(L, lua_sqlite_create_table);
    lua_setfield(L, -2, "CreateTable");

    // Pop metatable
    lua_pop(L, 1);

    // Create SQLiteAdapter global table
    lua_newtable(L);

    // Add static methods to the global table
    lua_pushcfunction(L, lua_sqlite_new);
    lua_setfield(L, -2, "new");
    lua_pushcfunction(L, lua_sqlite_connect);
    lua_setfield(L, -2, "Connect");
    lua_pushcfunction(L, lua_sqlite_execute);
    lua_setfield(L, -2, "Execute");
    lua_pushcfunction(L, lua_sqlite_query);
    lua_setfield(L, -2, "Query");
    lua_pushcfunction(L, lua_sqlite_insert);
    lua_setfield(L, -2, "Insert");
    lua_pushcfunction(L, lua_sqlite_update);
    lua_setfield(L, -2, "Update");
    lua_pushcfunction(L, lua_sqlite_delete);
    lua_setfield(L, -2, "Delete");
    lua_pushcfunction(L, lua_sqlite_create_table);
    lua_setfield(L, -2, "CreateTable");

    lua_setglobal(L, "SQLiteAdapter");
}

#ifdef HSBA_USE_MYSQL
void RegisterLuaMySQLAdapter(lua_State* L)
{
    // Create metatable for MySQLAdapter
    luaL_newmetatable(L, static_cast<const char*>(MySQLAdapterTypeName));

    // Set up metatable methods
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, lua_mysql_gc);
    lua_setfield(L, -2, "__gc");

    // Add methods to metatable
    lua_pushcfunction(L, lua_mysql_connect);
    lua_setfield(L, -2, "Connect");
    lua_pushcfunction(L, lua_mysql_execute);
    lua_setfield(L, -2, "Execute");
    lua_pushcfunction(L, lua_mysql_query);
    lua_setfield(L, -2, "Query");
    lua_pushcfunction(L, lua_mysql_insert);
    lua_setfield(L, -2, "Insert");
    lua_pushcfunction(L, lua_mysql_update);
    lua_setfield(L, -2, "Update");
    lua_pushcfunction(L, lua_mysql_delete);
    lua_setfield(L, -2, "Delete");
    lua_pushcfunction(L, lua_mysql_create_table);
    lua_setfield(L, -2, "CreateTable");

    // Pop metatable
    lua_pop(L, 1);

    // Create MySQLAdapter global table
    lua_newtable(L);

    // Add static methods to the global table
    lua_pushcfunction(L, lua_mysql_new);
    lua_setfield(L, -2, "new");
    lua_pushcfunction(L, lua_mysql_connect);
    lua_setfield(L, -2, "Connect");
    lua_pushcfunction(L, lua_mysql_execute);
    lua_setfield(L, -2, "Execute");
    lua_pushcfunction(L, lua_mysql_query);
    lua_setfield(L, -2, "Query");
    lua_pushcfunction(L, lua_mysql_insert);
    lua_setfield(L, -2, "Insert");
    lua_pushcfunction(L, lua_mysql_update);
    lua_setfield(L, -2, "Update");
    lua_pushcfunction(L, lua_mysql_delete);
    lua_setfield(L, -2, "Delete");
    lua_pushcfunction(L, lua_mysql_create_table);
    lua_setfield(L, -2, "CreateTable");

    lua_setglobal(L, "MySQLAdapter");
}
#endif  // HSBA_USE_MYSQL

#ifdef HSBA_USE_PGSQL
void RegisterLuaPostgreSQLAdapter(lua_State* L)
{
    // Create metatable for PostgreSQLAdapter
    luaL_newmetatable(L, static_cast<const char*>(PostgreSQLAdapterTypeName));

    // Set up metatable methods
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, lua_pgsql_gc);
    lua_setfield(L, -2, "__gc");

    // Add methods to metatable
    lua_pushcfunction(L, lua_pgsql_connect);
    lua_setfield(L, -2, "Connect");
    lua_pushcfunction(L, lua_pgsql_execute);
    lua_setfield(L, -2, "Execute");
    lua_pushcfunction(L, lua_pgsql_query);
    lua_setfield(L, -2, "Query");
    lua_pushcfunction(L, lua_pgsql_insert);
    lua_setfield(L, -2, "Insert");
    lua_pushcfunction(L, lua_pgsql_update);
    lua_setfield(L, -2, "Update");
    lua_pushcfunction(L, lua_pgsql_delete);
    lua_setfield(L, -2, "Delete");
    lua_pushcfunction(L, lua_pgsql_create_table);
    lua_setfield(L, -2, "CreateTable");

    // Pop metatable
    lua_pop(L, 1);

    // Create PostgreSQLAdapter global table
    lua_newtable(L);

    // Add static methods to the global table
    lua_pushcfunction(L, lua_pgsql_new);
    lua_setfield(L, -2, "new");
    lua_pushcfunction(L, lua_pgsql_connect);
    lua_setfield(L, -2, "Connect");
    lua_pushcfunction(L, lua_pgsql_execute);
    lua_setfield(L, -2, "Execute");
    lua_pushcfunction(L, lua_pgsql_query);
    lua_setfield(L, -2, "Query");
    lua_pushcfunction(L, lua_pgsql_insert);
    lua_setfield(L, -2, "Insert");
    lua_pushcfunction(L, lua_pgsql_update);
    lua_setfield(L, -2, "Update");
    lua_pushcfunction(L, lua_pgsql_delete);
    lua_setfield(L, -2, "Delete");
    lua_pushcfunction(L, lua_pgsql_create_table);
    lua_setfield(L, -2, "CreateTable");

    lua_setglobal(L, "PostgreSQLAdapter");
}
#endif  // HSBA_USE_PGSQL

namespace
{
// ============= ParamStore Wrapper =============
constexpr Utils::TemplateString ParamStoreTypeName = "ParamStore";

// Resolve a table argument: accepts a short label ("fdm") or a full table name ("hsba_param_fdm") and returns the tag.
PipelineConfigTag ResolveTag(std::string_view name)
{
    PipelineConfigTag tag = TagFromName(name);
    if (tag == PipelineConfigTag::Unknown && name.starts_with("hsba_param_"))
    {
        tag = TagFromName(name.substr(std::string_view("hsba_param_").size()));
    }
    return tag;
}

// Normalize a table name: expand a short label to its default table name; return unknown names unchanged.
std::string ActualTable(std::string_view name)
{
    PipelineConfigTag const tag = ResolveTag(name);
    if (tag != PipelineConfigTag::Unknown)
    {
        return std::string(DefaultTableName(tag));
    }
    return std::string(name);
}

// Allocate and default-initialize the Config struct for a tag, returning a raw pointer (caller frees via ti->destroy).
void* CreateConfigByTag(PipelineConfigTag tag, Utils::TypeInfo** out_ti)
{
    switch (tag)
    {
    case PipelineConfigTag::Fdm:
        *out_ti = Utils::GetTypeInfo<HsBaFdmPipelineConfig_t>();
        return new HsBaFdmPipelineConfig_t(HsBaFdmConfigDefault());
    case PipelineConfigTag::Sla:
        *out_ti = Utils::GetTypeInfo<HsBaSlaPipelineConfig_t>();
        return new HsBaSlaPipelineConfig_t(HsBaSlaConfigDefault());
    case PipelineConfigTag::Sls:
        *out_ti = Utils::GetTypeInfo<HsBaSlsPipelineConfig_t>();
        return new HsBaSlsPipelineConfig_t(HsBaSlsConfigDefault());
    case PipelineConfigTag::Slm:
        *out_ti = Utils::GetTypeInfo<HsBaSlmPipelineConfig_t>();
        return new HsBaSlmPipelineConfig_t(HsBaSlmConfigDefault());
    case PipelineConfigTag::Lom:
        *out_ti = Utils::GetTypeInfo<HsBaLomPipelineConfig_t>();
        return new HsBaLomPipelineConfig_t(HsBaLomConfigDefault());
    case PipelineConfigTag::Tdp:
        *out_ti = Utils::GetTypeInfo<HsBaTdpPipelineConfig_t>();
        return new HsBaTdpPipelineConfig_t(HsBaTdpConfigDefault());
    case PipelineConfigTag::Waam:
        *out_ti = Utils::GetTypeInfo<HsBaWaamPipelineConfig_t>();
        return new HsBaWaamPipelineConfig_t(HsBaWaamConfigDefault());
    case PipelineConfigTag::Custom:
        *out_ti = Utils::GetTypeInfo<HsBaCustomPipelineConfig_t>();
        return new HsBaCustomPipelineConfig_t(HsBaCustomConfigDefault());
    case PipelineConfigTag::FileTransfer:
        *out_ti = Utils::GetTypeInfo<HsBaFileTransferPipelineConfig_t>();
        return new HsBaFileTransferPipelineConfig_t(HsBaFileTransferConfigDefault());
    default:
        return nullptr;
    }
}

// RAII owner for the raw config instance created by CreateConfigByTag. Save/Load/Update longjmp
// or throw out of the frame on every error path, which would otherwise leak the instance.
struct ConfigGuard
{
    Utils::TypeInfo* ti = nullptr;
    void* ptr = nullptr;
    ~ConfigGuard()
    {
        if (ptr && ti)
        {
            ti->destroy(ptr);
        }
    }
};

// Read a Lua value (stack index idx) and produce a whitelist std::any converged by the field type.
std::any LuaValueToAny(lua_State* L, int idx, Utils::TypeInfo* field_ti)
{
    if (lua_isnoneornil(L, idx))
    {
        return std::any(nullptr);
    }
    switch (ParamSchema::KindOf(field_ti))
    {
    case ColumnKind::Double:
        return std::any(static_cast<double>(lua_tonumber(L, idx)));
    case ColumnKind::Int64:
        if (lua_isboolean(L, idx))
        {
            return std::any(static_cast<int64_t>(lua_toboolean(L, idx) ? 1 : 0));
        }
        return std::any(static_cast<int64_t>(lua_tointeger(L, idx)));
    case ColumnKind::Text:
    default:
        size_t len = 0;
        const char* s = lua_tolstring(L, idx, &len);
        return std::any(std::string(s ? s : "", len));
    }
}

// Iterate a config table (index tbl) to fill the struct's reflected fields.
void FillConfigFromLua(lua_State* L, int tbl, Utils::AnyObject cfg, StringArena& arena)
{
    cfg.ForeachField(
        [L, tbl, &arena](std::string_view name, const Utils::AnyObject& child)
        {
            lua_getfield(L, tbl, std::string(name).c_str());
            std::any const v = LuaValueToAny(L, -1, child.get_type_info());
            AnyToField(v, child.get_type_info(), child.get_data(), arena);
            lua_pop(L, 1);
        });
}

// Export the struct's reflected fields into a Lua table.
void PushConfigToLua(lua_State* L, Utils::AnyObject cfg)
{
    lua_createtable(L, 0, 16);
    cfg.ForeachField(
        [L](std::string_view name, const Utils::AnyObject& child)
        {
            std::any const v = FieldToAny(name, child.get_type_info(), child.get_data());
            PushAnyToLua(L, v);
            lua_setfield(L, -2, std::string(name).c_str());
        });
}

int lua_paramstore_new(lua_State* L)
{
    if (!lua_isuserdata(L, 1))
    {
        lua_pushstring(L, "ParamStore.new expects a SQL adapter object");
        return lua_error(L);
    }
    auto* adapter = (SQL::ISQLAdapter*)lua_topointer(L, 1);
    if (!adapter)
    {
        lua_pushstring(L, "ParamStore.new: null adapter pointer");
        return lua_error(L);
    }
    try
    {
        NewLuaObject<ParamStore, ParamStoreTypeName>(L, *adapter);
        return 1;
    }
    catch (const RuntimeError& e)
    {
        lua_pushstring(L, e.what());
    }
    return lua_error(L);
}

int lua_paramstore_ensure_schema(lua_State* L)
{
    auto* store = (ParamStore*)lua_topointer(L, 1);
    if (!store)
    {
        return raise_lua_error(L, "Invalid {} object", ParamStoreTypeName);
    }
    try
    {
        store->EnsureSchema();
        lua_pushboolean(L, 1);
        return 1;
    }
    catch (const RuntimeError& e)
    {
        lua_pushstring(L, e.what());
    }
    return lua_error(L);
}

// Enable or disable at-rest encryption for subsequent Save/Load/Update calls; nil or "" disables it.
int lua_paramstore_set_crypto_key(lua_State* L)
{
    auto* store = (ParamStore*)lua_topointer(L, 1);
    if (!store)
    {
        return raise_lua_error(L, "Invalid {} object", ParamStoreTypeName);
    }
    std::string_view key;
    size_t len = 0;
    const char* s = lua_isnil(L, 2) ? nullptr : lua_tolstring(L, 2, &len);
    if (s)
    {
        key = std::string_view(s, len);
    }
    store->SetCryptoKey(key);
    lua_pushboolean(L, 1);
    return 1;
}

// ParamStore methods. Every binding that can throw follows one shape: the body keeps the try/catch and all
// resource-owning locals, pushes either its results or the error text, and returns -1 on failure; the
// registration wraps it in guarded<>, whose clean frame is the only one that calls lua_error. lua_error's
// longjmp is unwound frame by frame and MSVC x64 runs the destructors those frames registered, so a frame
// whose locals were already released by a thrown exception must never sit on that path.
int lua_paramstore_save(lua_State* L)
{
    auto* store = (ParamStore*)lua_topointer(L, 1);
    std::string_view const table = luaL_checkstring(L, 2);
    std::string_view const key = luaL_checkstring(L, 3);
    luaL_checktype(L, 4, LUA_TTABLE);
    if (!store)
    {
        return raise_lua_error(L, "Invalid {} object", ParamStoreTypeName);
    }
    try
    {
        ConfigGuard owner;
        owner.ptr = CreateConfigByTag(ResolveTag(table), &owner.ti);
        if (!owner.ptr)
        {
            throw InvalidArgumentError(std::format("ParamStore.Save: unknown config table '{}'", table));
        }
        StringArena arena;
        Utils::AnyObject const cfg(owner.ti, owner.ptr);
        FillConfigFromLua(L, 4, cfg, arena);
        int64_t const id = store->Save(DefaultTableName(owner.ti), key, cfg);
        lua_pushinteger(L, static_cast<lua_Integer>(id));
        return 1;
    }
    catch (const RuntimeError& e)
    {
        lua_pushstring(L, e.what());
        return -1;
    }
}

int lua_paramstore_load(lua_State* L)
{
    auto* store = (ParamStore*)lua_topointer(L, 1);
    std::string_view const table = luaL_checkstring(L, 2);
    std::string_view const key = luaL_checkstring(L, 3);
    if (!store)
    {
        return raise_lua_error(L, "Invalid {} object", ParamStoreTypeName);
    }
    try
    {
        ConfigGuard owner;
        owner.ptr = CreateConfigByTag(ResolveTag(table), &owner.ti);
        if (!owner.ptr)
        {
            throw InvalidArgumentError(std::format("ParamStore.Load: unknown config table '{}'", table));
        }
        StringArena arena;
        Utils::AnyObject const cfg(owner.ti, owner.ptr);
        bool const ok = store->Load(DefaultTableName(owner.ti), key, cfg, arena);
        if (!ok)
        {
            lua_pushboolean(L, 0);
            lua_pushstring(L, "not found");
            return 2;
        }
        // Success contract: returns (true, configTable), symmetric with the failure path (false, errString).
        lua_pushboolean(L, 1);
        PushConfigToLua(L, cfg);
        return 2;
    }
    catch (const RuntimeError& e)
    {
        lua_pushstring(L, e.what());
        return -1;
    }
}

int lua_paramstore_list(lua_State* L)
{
    auto* store = (ParamStore*)lua_topointer(L, 1);
    std::string_view const table = luaL_checkstring(L, 2);
    std::string_view const where_json = lua_isstring(L, 3) ? std::string_view(lua_tostring(L, 3)) : std::string_view{};
    if (!store)
    {
        return raise_lua_error(L, "Invalid {} object", ParamStoreTypeName);
    }
    try
    {
        auto keys = store->List(ActualTable(table), std::string(where_json));
        lua_createtable(L, static_cast<int>(keys.size()), 0);
        int i = 1;
        for (const auto& k : keys)
        {
            lua_pushstring(L, k.c_str());
            lua_rawseti(L, -2, i++);
        }
        return 1;
    }
    catch (const RuntimeError& e)
    {
        lua_pushstring(L, e.what());
        return -1;
    }
}

int lua_paramstore_update(lua_State* L)
{
    auto* store = (ParamStore*)lua_topointer(L, 1);
    std::string_view const table = luaL_checkstring(L, 2);
    std::string_view const key = luaL_checkstring(L, 3);
    luaL_checktype(L, 4, LUA_TTABLE);
    luaL_checktype(L, 5, LUA_TTABLE);
    if (!store)
    {
        return raise_lua_error(L, "Invalid {} object", ParamStoreTypeName);
    }
    try
    {
        std::vector<std::string> changed;
        const int n = static_cast<int>(lua_rawlen(L, 5));
        for (int i = 1; i <= n; ++i)
        {
            lua_rawgeti(L, 5, i);
            if (lua_isstring(L, -1))
            {
                changed.emplace_back(lua_tostring(L, -1));
            }
            lua_pop(L, 1);
        }
        ConfigGuard owner;
        owner.ptr = CreateConfigByTag(ResolveTag(table), &owner.ti);
        if (!owner.ptr)
        {
            throw InvalidArgumentError(std::format("ParamStore.Update: unknown config table '{}'", table));
        }
        StringArena arena;
        Utils::AnyObject const cfg(owner.ti, owner.ptr);
        FillConfigFromLua(L, 4, cfg, arena);
        bool const ok = store->Update(DefaultTableName(owner.ti), key, cfg, changed);
        lua_pushboolean(L, ok ? 1 : 0);
        return 1;
    }
    catch (const RuntimeError& e)
    {
        lua_pushstring(L, e.what());
        return -1;
    }
}

int lua_paramstore_delete(lua_State* L)
{
    auto* store = (ParamStore*)lua_topointer(L, 1);
    std::string_view const table = luaL_checkstring(L, 2);
    std::string_view const key = luaL_checkstring(L, 3);
    if (!store)
    {
        return raise_lua_error(L, "Invalid {} object", ParamStoreTypeName);
    }
    try
    {
        bool const ok = store->Delete(ActualTable(table), key);
        lua_pushboolean(L, ok ? 1 : 0);
        return 1;
    }
    catch (const RuntimeError& e)
    {
        lua_pushstring(L, e.what());
        return -1;
    }
}

int lua_paramstore_gc(lua_State* L)
{
    LuaGC<ParamStore, ParamStoreTypeName>(L);
    return 0;
}
}  // namespace

void RegisterLuaParamStore(lua_State* L)
{
    // Static registration guard: lets the pipeline consume the same lua_State across multiple stages.
    static constexpr const char kRegistryKey[] = "HsBa.ParamStoreRegistered";
    lua_getfield(L, LUA_REGISTRYINDEX, kRegistryKey);
    if (!lua_isnil(L, -1))
    {
        lua_pop(L, 1);
        return;
    }
    lua_pop(L, 1);

    // Idempotently trigger reflection registration so TypeInfo::fields is populated.
    RegisterPipelineConfigTypes();

    // metatable: instance methods
    luaL_newmetatable(L, static_cast<const char*>(ParamStoreTypeName));
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, lua_paramstore_gc);
    lua_setfield(L, -2, "__gc");
    lua_pushcfunction(L, lua_paramstore_ensure_schema);
    lua_setfield(L, -2, "EnsureSchema");
    lua_pushcfunction(L, lua_paramstore_set_crypto_key);
    lua_setfield(L, -2, "SetCryptoKey");
    // Each worker returns -1 after pushing the error text; guarded() raises from a frame that owns nothing.
    lua_pushcfunction(L, guarded<lua_paramstore_save>);
    lua_setfield(L, -2, "Save");
    lua_pushcfunction(L, guarded<lua_paramstore_load>);
    lua_setfield(L, -2, "Load");
    lua_pushcfunction(L, guarded<lua_paramstore_list>);
    lua_setfield(L, -2, "List");
    lua_pushcfunction(L, guarded<lua_paramstore_update>);
    lua_setfield(L, -2, "Update");
    lua_pushcfunction(L, guarded<lua_paramstore_delete>);
    lua_setfield(L, -2, "Delete");
    lua_pop(L, 1);

    // global table: ParamStore.new
    lua_newtable(L);
    lua_pushcfunction(L, lua_paramstore_new);
    lua_setfield(L, -2, "new");
    lua_setglobal(L, "ParamStore");

    lua_pushboolean(L, 1);
    lua_setfield(L, LUA_REGISTRYINDEX, kRegistryKey);
}

}  // namespace HsBa::Slicer