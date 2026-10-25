#define BOOST_TEST_MODULE filename_test
#include <boost/test/included/unit_test.hpp>

#include <chrono>
#include <filesystem>
#include <thread>

#include "fileoperator/LuaAdapter.hpp"
#include "fileoperator/sql_adapter.hpp"
#include <lua.hpp>

#if _WIN32
#include <Windows.h>
#endif  // _WIN32

#include <iostream>

void PrintSqlLog(std::string_view type, std::string_view sql)
{
    std::cout << "SQL Log [" << type << "]: " << sql << std::endl;
}

BOOST_AUTO_TEST_SUITE(sqlite_check)

BOOST_AUTO_TEST_CASE(test_sqlite_adapter)
{
    BOOST_TEST_MESSAGE("Running SQLite adapter test");
    // Set console output to UTF-8 for Windows
#if _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif  // _WIN32
    if (std::filesystem::exists("test.db"))
    {
        std::filesystem::remove("test.db");
    }
    {
        HsBa::Slicer::SQL::SQLiteAdapter db;
        db += PrintSqlLog;
        BOOST_REQUIRE_NO_THROW(db.Connect("test.db"));
        BOOST_REQUIRE(db.IsConnected());
        BOOST_REQUIRE_NO_THROW(db.Execute("CREATE TABLE IF NOT EXISTS test (id INTEGER PRIMARY KEY, name TEXT)"));
        BOOST_REQUIRE_NO_THROW(db | HsBa::Slicer::SQL::SQLInsert("test", {{"name", std::string("Alice")}}));
        BOOST_REQUIRE_NO_THROW(db | HsBa::Slicer::SQL::SQLInsert("test", {{"name", std::string("Bob")}}));
        HsBa::Slicer::SQL::ISQLAdapter::Rows rows;
        BOOST_REQUIRE_NO_THROW(rows = db | HsBa::Slicer::SQL::SQLSelect("test", {"id", "name"}));
        BOOST_REQUIRE_EQUAL(rows.size(), 2);
        auto test_name = std::any_cast<std::string>(rows[0].at("name"));
        BOOST_REQUIRE_EQUAL(test_name, std::string("Alice"));
        BOOST_REQUIRE_NO_THROW(db.Execute("UPDATE test SET name = 'Charlie' WHERE id = 1"));
        BOOST_REQUIRE_NO_THROW(rows = db | HsBa::Slicer::SQL::SQLSelect("test", {"id", "name"}));
        BOOST_REQUIRE_EQUAL(rows.size(), 2);
        test_name = std::any_cast<std::string>(rows[0].at("name"));
        BOOST_REQUIRE_EQUAL(test_name, std::string("Charlie"));
        BOOST_REQUIRE_NO_THROW(db.Execute("DELETE FROM test WHERE id = 2"));
        BOOST_REQUIRE_NO_THROW(rows = db | HsBa::Slicer::SQL::SQLSelect("test", {"id", "name"}));
        BOOST_REQUIRE_EQUAL(rows.size(), 1);
    }  // ~SQLiteAdapter closes the handle, so the file is no longer locked below
    // 临时数据库必须在用例结束时清理，避免污染仓库工作目录
    BOOST_REQUIRE_MESSAGE(!std::filesystem::exists("test.db") || std::filesystem::remove("test.db"),
                          "Cannot delete test.db");
    BOOST_TEST_MESSAGE("SQLite adapter test completed successfully");
}

BOOST_AUTO_TEST_CASE(test_sqlite_adapter_lua_integration)
{
    BOOST_TEST_MESSAGE("Running SQLite adapter Lua Integration test");
#if _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif  // _WIN32
    if (std::filesystem::exists("lua_test.db"))
    {
        std::filesystem::remove("lua_test.db");
    }
    // Create Lua state
    auto L = HsBa::Slicer::MakeUniqueLuaState();
    luaL_openlibs(L.get());

    // Register SQLiteAdapter to Lua
    HsBa::Slicer::RegisterLuaSQLiteAdapter(L.get());

    // Execute Lua script to test SQLiteAdapter with method call syntax
    const char* lua_code = R"(
		local SQLiteAdapter = SQLiteAdapter          
		local db = SQLiteAdapter.new()               

		db:Connect("lua_test.db")
		db:CreateTable("users", {id = "INTEGER PRIMARY KEY", name = "TEXT", age = "INTEGER"})

		db:Insert("users", {name = "Alice", age = 30})
		db:Insert("users", {name = "Bob",   age = 25})

		local rows = db:Query("SELECT * FROM users ORDER BY id")
		assert(#rows == 2,            "Expected 2 rows")
		assert(rows[1].name == "Alice", "Expected Alice")
		assert(rows[2].name == "Bob",   "Expected Bob")
	)";


    int ret = luaL_dostring(L.get(), lua_code);
    BOOST_REQUIRE_MESSAGE(ret == 0, lua_tostring(L.get(), -1));

    // Explicitly destroy Lua state so SQLite DB is closed before deletion
    L.reset();

    auto safe_remove = [](const std::filesystem::path& p)
    {
        for (int i = 0; i < 50; ++i)
        {  // 最多 5 s
            try
            {
                std::filesystem::remove(p);
                return true;
            }
            catch (const std::filesystem::filesystem_error&)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        return false;
    };

    if (std::filesystem::exists("lua_test.db"))
    {
        BOOST_REQUIRE_MESSAGE(safe_remove("lua_test.db"), "Cannot delete lua_test.db, still locked");
    }
    BOOST_TEST_MESSAGE("SQLite adapter Lua Integration test completed successfully");
}

BOOST_AUTO_TEST_CASE(test_sqlite_adapter_lua_static_call)
{
    BOOST_TEST_MESSAGE("Running SQLite adapter Lua Static Call test");
#if _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif  // _WIN32
    if (std::filesystem::exists("lua_static_test.db"))
    {
        std::filesystem::remove("lua_static_test.db");
    }
    // Create Lua state
    auto L = HsBa::Slicer::MakeUniqueLuaState();
    luaL_openlibs(L.get());

    // Register SQLiteAdapter to Lua
    HsBa::Slicer::RegisterLuaSQLiteAdapter(L.get());

    // Execute Lua script to test SQLiteAdapter with static call syntax
    const char* lua_code = R"(
		local db = SQLiteAdapter.new()
		SQLiteAdapter.Connect(db, "lua_static_test.db")
		SQLiteAdapter.CreateTable(db, "users", {id = "INTEGER PRIMARY KEY", name = "TEXT", age = "INTEGER"})
		SQLiteAdapter.Insert(db, "users", {name = "Alice", age = 30})
		SQLiteAdapter.Insert(db, "users", {name = "Bob",   age = 25})
		local rows = SQLiteAdapter.Query(db, "SELECT * FROM users ORDER BY id")
		assert(#rows == 2,            "Expected 2 rows")
		assert(rows[1].name == "Alice", "Expected Alice")
		assert(rows[2].name == "Bob",   "Expected Bob")
	)";


    int ret = luaL_dostring(L.get(), lua_code);
    BOOST_REQUIRE_MESSAGE(ret == 0, lua_tostring(L.get(), -1));

    // Explicitly destroy Lua state so SQLite DB is closed before deletion
    L.reset();

    auto safe_remove = [](const std::filesystem::path& p)
    {
        for (int i = 0; i < 50; ++i)
        {  // 最多 5 s
            try
            {
                std::filesystem::remove(p);
                return true;
            }
            catch (const std::filesystem::filesystem_error&)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        return false;
    };

    if (std::filesystem::exists("lua_static_test.db"))
    {
        BOOST_REQUIRE_MESSAGE(safe_remove("lua_static_test.db"), "Cannot delete lua_static_test.db, still locked");
    }
    BOOST_TEST_MESSAGE("SQLite adapter Lua Static Call test completed successfully");
}

BOOST_AUTO_TEST_CASE(test_sqlite_adapter_lua_both_call_conventions)
{
    BOOST_TEST_MESSAGE("Running SQLite adapter Lua Both Call Conventions test");
#if _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif  // _WIN32
    if (std::filesystem::exists("lua_both_test.db"))
    {
        std::filesystem::remove("lua_both_test.db");
    }
    // Create Lua state
    auto L = HsBa::Slicer::MakeUniqueLuaState();
    luaL_openlibs(L.get());

    // Register SQLiteAdapter to Lua
    HsBa::Slicer::RegisterLuaSQLiteAdapter(L.get());

    // Execute Lua script to test SQLiteAdapter with both calling conventions
    const char* lua_code = R"(
		local db = SQLiteAdapter.new()
		-- Using method call syntax
		db:Connect("lua_both_test.db")
		db:CreateTable("users", {id = "INTEGER PRIMARY KEY", name = "TEXT", age = "INTEGER"})
		db:Insert("users", {name = "Alice", age = 30})
		-- Using static call syntax
		SQLiteAdapter.Insert(db, "users", {name = "Bob",   age = 25})
		local rows = db:Query("SELECT * FROM users ORDER BY id")
		assert(#rows == 2,            "Expected 2 rows")
		assert(rows[1].name == "Alice", "Expected Alice")
		assert(rows[2].name == "Bob",   "Expected Bob")
	)";


    int ret = luaL_dostring(L.get(), lua_code);
    BOOST_REQUIRE_MESSAGE(ret == 0, lua_tostring(L.get(), -1));

    // Explicitly destroy Lua state so SQLite DB is closed before deletion
    L.reset();

    auto safe_remove = [](const std::filesystem::path& p)
    {
        for (int i = 0; i < 50; ++i)
        {  // 最多 5 s
            try
            {
                std::filesystem::remove(p);
                return true;
            }
            catch (const std::filesystem::filesystem_error&)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        return false;
    };

    if (std::filesystem::exists("lua_both_test.db"))
    {
        BOOST_REQUIRE_MESSAGE(safe_remove("lua_both_test.db"), "Cannot delete lua_both_test.db, still locked");
    }
    BOOST_TEST_MESSAGE("SQLite adapter Lua Both Call Conventions test completed successfully");
}

// Error paths of the SQLiteAdapter Lua binding. The frame calling lua_error must own nothing to
// destroy: MSVC x64 runs the destructors of every frame its longjmp unwinds (a pre-destroyed local
// is freed twice), POSIX runs none (a live local leaks). Arguments exceed the small string
// optimization so that either failure is observable.
BOOST_AUTO_TEST_CASE(test_sqlite_adapter_lua_error_paths)
{
    BOOST_TEST_MESSAGE("Running SQLite adapter Lua Error Path test");
    auto L = HsBa::Slicer::MakeUniqueLuaState();
    luaL_openlibs(L.get());
    HsBa::Slicer::RegisterLuaSQLiteAdapter(L.get());

    const std::filesystem::path db_path = std::filesystem::temp_directory_path() / "hsba_lua_error_paths.db";
    std::error_code ec;
    std::filesystem::remove(db_path, ec);
    const std::string db_name = db_path.string();
    lua_pushlstring(L.get(), db_name.data(), db_name.size());
    lua_setglobal(L.get(), "DB_PATH");

    const char* lua_code = R"(
        local name = string.rep("t", 64)
        local function probe(f)
            local ok, err = pcall(f)
            return tostring(ok) .. "|" .. tostring(err)
        end
        _G.r1 = probe(function() return SQLiteAdapter.Connect(nil, name) end)
        _G.r2 = probe(function() return SQLiteAdapter.Execute(nil, name) end)
        _G.r3 = probe(function() return SQLiteAdapter.Query(nil, name) end)
        _G.r4 = probe(function() return SQLiteAdapter.Insert(nil, name, {k = name}) end)
        _G.r5 = probe(function() return SQLiteAdapter.Update(nil, name, {k = name}, {k = name}) end)
        _G.r6 = probe(function() return SQLiteAdapter.Delete(nil, name, {k = name}) end)
        _G.r7 = probe(function() return SQLiteAdapter.CreateTable(nil, name, {k = "text"}) end)
        local db = SQLiteAdapter.new()
        db:Connect(DB_PATH)
        db:CreateTable("tbl", {id = "integer", label = "text"})
        _G.r8 = probe(function() return db:Execute("this is not valid sql") end)
    )";

    int ret = luaL_dostring(L.get(), lua_code);
    BOOST_REQUIRE_MESSAGE(ret == LUA_OK, lua_tostring(L.get(), -1));

    auto read_global = [L = L.get()](const char* key) -> std::string
    {
        lua_getglobal(L, key);
        const char* value = lua_tostring(L, -1);
        std::string result = value ? value : "";
        lua_pop(L, 1);
        return result;
    };

    for (const char* key : {"r1", "r2", "r3", "r4", "r5", "r6", "r7"})
    {
        const std::string result = read_global(key);
        BOOST_CHECK_MESSAGE(result.rfind("false|", 0) == 0, key << " -> " << result);
        BOOST_CHECK_MESSAGE(result.find("Invalid SQLiteAdapter object") != std::string::npos, key << " -> " << result);
    }
    // invalid SQL is reported through the catch handler of the wrapper
    const std::string bad_sql = read_global("r8");
    BOOST_CHECK_MESSAGE(bad_sql.rfind("false|", 0) == 0, bad_sql);
    BOOST_CHECK_MESSAGE(bad_sql.size() > 6, bad_sql);

    std::filesystem::remove(db_path, ec);
    BOOST_TEST_MESSAGE("SQLite adapter Lua Error Path test completed successfully");
}

BOOST_AUTO_TEST_SUITE_END()