#define BOOST_TEST_MODULE filename_test
#include <boost/test/included/unit_test.hpp>

#include <filesystem>

#include "fileoperator/LuaAdapter.hpp"
#include "fileoperator/unzipper.hpp"
#include "fileoperator/zipper.hpp"
#include <lua.hpp>

#if _WIN32
#include <Windows.h>
#endif  // _WIN32

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
#endif  // _WIN32
    if (std::filesystem::exists("test.zip"))
    {
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
#endif  // _WIN32
    if (std::filesystem::exists("test.zip"))
    {
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
        (*unzipper) += [](std::string_view zipper, std::string_view part)
        { std::cout << "UnZipper Log:" << "In zipper " << zipper << " Get " << part << "\n"; };
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
#endif  // _WIN32
    if (std::filesystem::exists("lua_test.zip"))
    {
        std::filesystem::remove("lua_test.zip");
    }
    // Create Lua state
    auto L = HsBa::Slicer::MakeUniqueLuaState();
    luaL_openlibs(L.get());

    // Register Zipper to Lua
    HsBa::Slicer::RegisterLuaZipper(L.get());

    // Execute Lua script to test Zipper with static method calls
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

BOOST_AUTO_TEST_CASE(test_zipper_lua_method_call)
{
    BOOST_TEST_MESSAGE("Running Zipper Lua Method Call test");
#if _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif  // _WIN32
    if (std::filesystem::exists("lua_method_test.zip"))
    {
        std::filesystem::remove("lua_method_test.zip");
    }
    // Create Lua state
    auto L = HsBa::Slicer::MakeUniqueLuaState();
    luaL_openlibs(L.get());

    // Register Zipper to Lua
    HsBa::Slicer::RegisterLuaZipper(L.get());

    // Execute Lua script to test Zipper with method call syntax
    const char* lua_code = R"(
		local zipper = Zipper.new()
		zipper:AddByteFile("lua_method_test1.txt", "Lua method test file 1")
		zipper:AddByteFile("lua_method_test2.txt", "Lua method test file 2")
		zipper:Save("lua_method_test.zip")
	)";

    int ret = luaL_dostring(L.get(), lua_code);
    BOOST_REQUIRE_MESSAGE(ret == 0, lua_tostring(L.get(), -1));

    // Verify the zip file was created and has correct content
    BOOST_REQUIRE(std::filesystem::exists("lua_method_test.zip"));
    auto extracted_files = HsBa::Slicer::MiniZExtractFileToBuffer("lua_method_test.zip");
    BOOST_REQUIRE_EQUAL(extracted_files.size(), 2);
    BOOST_REQUIRE_EQUAL(extracted_files["lua_method_test1.txt"], "Lua method test file 1");
    BOOST_REQUIRE_EQUAL(extracted_files["lua_method_test2.txt"], "Lua method test file 2");

    // Clean up
    std::filesystem::remove("lua_method_test.zip");
    BOOST_TEST_MESSAGE("Zipper Lua Method Call test completed successfully");
}

BOOST_AUTO_TEST_CASE(test_zipper_lua_both_call_conventions)
{
    BOOST_TEST_MESSAGE("Running Zipper Lua Both Call Conventions test");
#if _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif  // _WIN32
    if (std::filesystem::exists("lua_both_test.zip"))
    {
        std::filesystem::remove("lua_both_test.zip");
    }
    // Create Lua state
    auto L = HsBa::Slicer::MakeUniqueLuaState();
    luaL_openlibs(L.get());

    // Register Zipper to Lua
    HsBa::Slicer::RegisterLuaZipper(L.get());

    // Execute Lua script to test Zipper with both calling conventions
    const char* lua_code = R"(
		local zipper = Zipper.new()
		-- Using method call syntax
		zipper:AddByteFile("method_syntax.txt", "Method syntax content")
		-- Using static call syntax
		Zipper.AddByteFile(zipper, "static_syntax.txt", "Static syntax content")
		zipper:Save("lua_both_test.zip")
	)";

    int ret = luaL_dostring(L.get(), lua_code);
    BOOST_REQUIRE_MESSAGE(ret == 0, lua_tostring(L.get(), -1));

    // Verify the zip file was created and has correct content
    BOOST_REQUIRE(std::filesystem::exists("lua_both_test.zip"));
    auto extracted_files = HsBa::Slicer::MiniZExtractFileToBuffer("lua_both_test.zip");
    BOOST_REQUIRE_EQUAL(extracted_files.size(), 2);
    BOOST_REQUIRE_EQUAL(extracted_files["method_syntax.txt"], "Method syntax content");
    BOOST_REQUIRE_EQUAL(extracted_files["static_syntax.txt"], "Static syntax content");

    // Clean up
    std::filesystem::remove("lua_both_test.zip");
    BOOST_TEST_MESSAGE("Zipper Lua Both Call Conventions test completed successfully");
}

// Error paths of the Zipper Lua binding. The frame calling lua_error must own nothing to destroy:
// MSVC x64 runs the destructors of every frame its longjmp unwinds (a pre-destroyed local is freed
// twice), POSIX runs none (a live local leaks). Arguments exceed the small string optimization so
// that either failure shows up - as a leak under ASAN or as heap corruption in a Release stress run.
BOOST_AUTO_TEST_CASE(test_zipper_lua_error_paths)
{
    BOOST_TEST_MESSAGE("Running Zipper Lua Error Path test");
    auto L = HsBa::Slicer::MakeUniqueLuaState();
    luaL_openlibs(L.get());
    HsBa::Slicer::RegisterLuaZipper(L.get());

    const char* lua_code = R"(
        local name = string.rep("n", 64)
        local path = string.rep("p", 64)
        local function probe(f)
            local ok, err = pcall(f)
            return tostring(ok) .. "|" .. tostring(err)
        end
        _G.r1 = probe(function() return Zipper.AddFile(nil, name, path) end)
        _G.r2 = probe(function() return Zipper.AddByteFile(nil, name, path) end)
        _G.r3 = probe(function() return Zipper.Save(nil, path) end)
        _G.r4 = probe(function() return Zipper.AddFile(42, name, path) end)
        local zipper = Zipper.new()
        zipper:AddFile(name, path)
        _G.r5 = probe(function() return zipper:AddFile(name, path) end)
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

    for (const char* key : {"r1", "r2", "r3", "r4"})
    {
        const std::string result = read_global(key);
        BOOST_CHECK_MESSAGE(result.rfind("false|", 0) == 0, key << " -> " << result);
        BOOST_CHECK_MESSAGE(result.find("Invalid Zipper object") != std::string::npos, key << " -> " << result);
    }
    // staging the same entry name twice is a RuntimeError raised from inside the wrapper body
    const std::string duplicate = read_global("r5");
    BOOST_CHECK_MESSAGE(duplicate.rfind("false|", 0) == 0, duplicate);
    BOOST_CHECK_MESSAGE(duplicate.find("Duplicate name files") != std::string::npos, duplicate);
    BOOST_TEST_MESSAGE("Zipper Lua Error Path test completed successfully");
}

BOOST_AUTO_TEST_SUITE_END()