#define BOOST_TEST_MODULE filename_test
#include <boost/test/included/unit_test.hpp>

#include <filesystem>
#include <chrono>
#include <thread>

#include "fileoperator/sql_adapter.hpp"
#include "fileoperator/LuaAdapter.hpp"
#include <lua.hpp>

#if _WIN32
#include <Windows.h>
#endif // _WIN32

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
#endif // _WIN32
	if (std::filesystem::exists("test.db")) {
		std::filesystem::remove("test.db");
	}
	HsBa::Slicer::SQL::SQLiteAdapter db;
	db += PrintSqlLog;
	BOOST_REQUIRE_NO_THROW(db.Connect("test.db"));
	BOOST_REQUIRE(db.IsConnected());
	BOOST_REQUIRE_NO_THROW(db.Execute("CREATE TABLE IF NOT EXISTS test (id INTEGER PRIMARY KEY, name TEXT)"));
	BOOST_REQUIRE_NO_THROW(db | HsBa::Slicer::SQL::SQLInsert("test", { {"name", std::string("Alice")} }));
	BOOST_REQUIRE_NO_THROW(db | HsBa::Slicer::SQL::SQLInsert("test", { {"name", std::string("Bob")} }));
	HsBa::Slicer::SQL::ISQLAdapter::Rows rows;
	BOOST_REQUIRE_NO_THROW(rows = db | HsBa::Slicer::SQL::SQLSelect("test", { "id", "name" }));
	BOOST_REQUIRE_EQUAL(rows.size(), 2);
	auto test_name = std::any_cast<std::string>(rows[0].at("name"));
	BOOST_REQUIRE_EQUAL(test_name, std::string("Alice"));
	BOOST_REQUIRE_NO_THROW(db.Execute("UPDATE test SET name = 'Charlie' WHERE id = 1"));
	BOOST_REQUIRE_NO_THROW(rows = db | HsBa::Slicer::SQL::SQLSelect("test", { "id", "name" }));
	BOOST_REQUIRE_EQUAL(rows.size(), 2);
	test_name = std::any_cast<std::string>(rows[0].at("name"));
	BOOST_REQUIRE_EQUAL(test_name, std::string("Charlie"));
	BOOST_REQUIRE_NO_THROW(db.Execute("DELETE FROM test WHERE id = 2"));
	BOOST_REQUIRE_NO_THROW(rows = db | HsBa::Slicer::SQL::SQLSelect("test", { "id", "name" }));
	BOOST_REQUIRE_EQUAL(rows.size(), 1);
}

BOOST_AUTO_TEST_CASE(test_sqlite_adapter_lua_integration)
{
	BOOST_TEST_MESSAGE("Running SQLite adapter Lua Integration test");
#if _WIN32
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);
#endif // _WIN32
	if (std::filesystem::exists("lua_test.db")) 
	{
		std::filesystem::remove("lua_test.db");
	}
	// Create Lua state
	lua_State* L = luaL_newstate();
	luaL_openlibs(L);
	
	// Register SQLiteAdapter to Lua
	HsBa::Slicer::RegisterLuaSQLiteAdapter(L);
	
	// Execute Lua script to test SQLiteAdapter
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
		db:Close()
	)";

	
	int ret = luaL_dostring(L, lua_code);
	BOOST_REQUIRE_MESSAGE(ret == 0, lua_tostring(L, -1));
	
	// Clean up
	lua_close(L);
	
	// Add small delay to ensure file is fully released

	auto safe_remove = [](const std::filesystem::path& p) {
    	for (int i = 0; i < 50; ++i) {          // 最多 5 s
        	try 
			{
            	std::filesystem::remove(p);
            	return true;
        	} catch (const std::filesystem::filesystem_error&) 
			{
            	std::this_thread::sleep_for(std::chrono::milliseconds(100));
        	}
    	}
    	return false;
	};

	if (std::filesystem::exists("lua_test.db")) 
	{
    	BOOST_REQUIRE_MESSAGE(safe_remove("lua_test.db"),
                          "Cannot delete lua_test.db, still locked");
	}
	BOOST_TEST_MESSAGE("SQLite adapter Lua Integration test completed successfully");
}

BOOST_AUTO_TEST_SUITE_END()