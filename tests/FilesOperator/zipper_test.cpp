#define BOOST_TEST_MODULE filename_test
#include <boost/test/included/unit_test.hpp>

#include <filesystem>

#include "fileoperator/zipper.hpp"
#include "fileoperator/unzipper.hpp"
#include "fileoperator/LuaAdapter.hpp"
#include <lua.hpp>

#if _WIN32
#include <Windows.h>
#endif // _WIN32

#include <iostream>

void PrintZipLog(double rate, std::string_view filename)
{
	std::cout << "Zipper Log: " << filename << " - " << rate * 100.0 << "% compressed" << std::endl;
}

BOOST_AUTO_TEST_SUITE(zipper_check)

BOOST_AUTO_TEST_CASE(test_zipper)
{
	BOOST_TEST_MESSAGE("Running Zipper test");
	// Set console output to UTF-8 for Windows
#if _WIN32
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);
#endif // _WIN32
	if (std::filesystem::exists("test.zip")) {
		std::filesystem::remove("test.zip");
	}
	// Create a Zipper instance
	HsBa::Slicer::Zipper zipper(HsBa::Slicer::MinizCompression::Tight);
	zipper += PrintZipLog;
	// Add files to the zipper
	zipper.AddByteFile("test1.txt", "This is a test file 1.");
	zipper.AddByteFile("test2.txt", "This is a test file 2.");
	// Save the zipped file
	std::string zip_path = "test.zip";
	BOOST_REQUIRE_NO_THROW(zipper.Save(zip_path));
	// Check if the zip file was created
	BOOST_REQUIRE(std::filesystem::exists(zip_path));
	// Extract the zip file
	std::unordered_map<std::string, std::string> extracted_files;
	BOOST_REQUIRE_NO_THROW(extracted_files = HsBa::Slicer::MiniZExtractFileToBuffer(zip_path));
	// Check if the extracted files match the original files
	BOOST_REQUIRE_EQUAL(extracted_files.size(), 2);
	BOOST_REQUIRE_EQUAL(extracted_files["test1.txt"], "This is a test file 1.");
	BOOST_REQUIRE_EQUAL(extracted_files["test2.txt"], "This is a test file 2.");
	// Clean up
	std::filesystem::remove(zip_path);
	BOOST_TEST_MESSAGE("Zipper test completed successfully");
}

BOOST_AUTO_TEST_CASE(test_unzipper)
{
	BOOST_TEST_MESSAGE("Running Zipper test");
	// Set console output to UTF-8 for Windows
#if _WIN32
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);
#endif // _WIN32
	if (std::filesystem::exists("test.zip")) {
		std::filesystem::remove("test.zip");
	}
	// Create a Zipper instance
	HsBa::Slicer::Zipper zipper(HsBa::Slicer::MinizCompression::Tight);
	zipper += PrintZipLog;
	// Add files to the zipper
	zipper.AddByteFile("test1.txt", "This is a test file 1.");
	zipper.AddByteFile("test2.txt", "This is a test file 2.");
	// Save the zipped file
	std::string zip_path = "test.zip";
	BOOST_REQUIRE_NO_THROW(zipper.Save(zip_path));
	// Check if the zip file was created
	BOOST_REQUIRE(std::filesystem::exists(zip_path));
	{
		// Check if the extracted files match the original files
		auto unzipper = HsBa::Slicer::Unzipper::Create();
		(*unzipper) += [](std::string_view zipper, std::string_view part) {
			std::cout << "UnZipper Log:" << "In zipper " << zipper << " Get " << part << "\n";
			};
		unzipper->ReadFromFile(zip_path);
		auto test1_is = unzipper->GetStream("test1.txt");
		std::string out;
		std::getline(*test1_is, out);
		HsBa::Slicer::Unzipper::SetMaxMemSize(10);
		auto test2_is = unzipper->GetStream("test2.txt");
		std::getline(*test2_is, out);
		BOOST_REQUIRE_EQUAL(out, "This is a test file 2.");
		test2_is->seekg(std::ios_base::beg);
		std::getline(*test2_is, out);
		BOOST_REQUIRE_EQUAL(out, "This is a test file 2.");
	}
	// Clean up
	std::filesystem::remove(zip_path);
	BOOST_TEST_MESSAGE("UnZipper test completed successfully");
}

BOOST_AUTO_TEST_CASE(test_zipper_lua_integration)
{
	BOOST_TEST_MESSAGE("Running Zipper Lua Integration test");
#if _WIN32
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);
#endif // _WIN32
	if (std::filesystem::exists("lua_test.zip")) {
		std::filesystem::remove("lua_test.zip");
	}
	// Create Lua state
	auto L = HsBa::Slicer::MakeUniqueLuaState();
	luaL_openlibs(L.get());
	
	// Register Zipper to Lua
	HsBa::Slicer::RegisterLuaZipper(L.get());
	
	// Execute Lua script to test Zipper
	const char* lua_code = R"(
		local zipper = Zipper.new()
		Zipper.AddByteFile(zipper, "lua_test1.txt", "Lua test file 1")
		Zipper.AddByteFile(zipper, "lua_test2.txt", "Lua test file 2")
		Zipper.Save(zipper, "lua_test.zip")
	)";
	
	int ret = luaL_dostring(L.get(), lua_code);
	BOOST_REQUIRE_MESSAGE(ret == 0, lua_tostring(L.get(), -1));
	
	// Verify the zip file was created and has correct content
	BOOST_REQUIRE(std::filesystem::exists("lua_test.zip"));
	auto extracted_files = HsBa::Slicer::MiniZExtractFileToBuffer("lua_test.zip");
	BOOST_REQUIRE_EQUAL(extracted_files.size(), 2);
	BOOST_REQUIRE_EQUAL(extracted_files["lua_test1.txt"], "Lua test file 1");
	BOOST_REQUIRE_EQUAL(extracted_files["lua_test2.txt"], "Lua test file 2");
	
	// Clean up
	std::filesystem::remove("lua_test.zip");
	BOOST_TEST_MESSAGE("Zipper Lua Integration test completed successfully");
}

BOOST_AUTO_TEST_SUITE_END()