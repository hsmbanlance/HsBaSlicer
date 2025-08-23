#define BOOST_TEST_MODULE filename_test
#include <boost/test/included/unit_test.hpp>

#include <filesystem>

#include "fileoperator/sql_adapter.hpp"

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

BOOST_AUTO_TEST_SUITE_END()