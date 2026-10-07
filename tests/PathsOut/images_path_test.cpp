#define BOOST_TEST_MODULE images_path_test
#include <boost/test/included/unit_test.hpp>

#include "paths/imagespath.hpp"
#include <filesystem>
#include <fstream>
#include <string>

#include "base/error.hpp"

using namespace HsBa::Slicer;

struct DisableCrt
{
    DisableCrt()
    {
#if defined(_MSC_VER) && defined(_DEBUG)
        _CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) & ~_CRTDBG_LEAK_CHECK_DF);
#endif  // defined(_MSC_VER) && defined(_DEBUG)
    }
};

BOOST_AUTO_TEST_SUITE(images_path_test)


BOOST_AUTO_TEST_CASE(test_to_string_with_encoding)
{
    [[maybe_unused]]
    static DisableCrt crt_;
    // create ImagesPath with dummy config
    ImagesPath ip("cfgfile", "{}");
    // add two small images as base64 strings ("abc" -> "YWJj", bytes {0x01,0x02} -> "AQI=")
    std::string img1 = "YWJj";  // base64 of "abc"
    std::string img2 = "AQI=";  // base64 of bytes 0x01,0x02
    ip.AddImage("img1.png", img1);
    ip.AddImage("img2.bin", img2);

    // script to decode base64 images and return combined string
    std::string script = R"lua(
local out = {}
for i=1, #images do
  local it = images[i]
  local dec = Cipher.base64_decode(it.data)
  table.insert(out, "#" .. it.path)
  table.insert(out, dec)
end
return table.concat(out, "\n")
)lua";

    auto out = ip.ToString(script);
    std::cout << "Images ToString(script decoded) =>\n" << out << "\n";
    // Expect decoded payload 'abc' to appear
    BOOST_CHECK_NE(out.find("abc"), std::string::npos);
}

BOOST_AUTO_TEST_CASE(test_save_creates_file)
{
    ImagesPath ip(
        "cfgfile", "{}",
        {[](double rate, std::string_view path) { std::cout << "Callback: " << rate << "%, " << path << "\n"; }});
    std::string img = "eA==";  // base64 of 'x'
    ip.AddImage("one.png", img);

    auto tmp = std::filesystem::temp_directory_path() / "images_out_test.zip";
    std::error_code ec;
    std::filesystem::remove(tmp, ec);

    // script that creates a zip using the provided zipper and writes to output_path
    std::string script = R"lua(
local z = Zipper.new()
Zipper.AddByteFile(z,"one.png", Cipher.base64_decode(images[1].data))
Zipper.Save(z,output_path)
return output_path
)lua";

    ip.Save(tmp, script);
    BOOST_CHECK(std::filesystem::exists(tmp));

    // cleanup
    std::filesystem::remove(tmp, ec);
}

// The no-script ToString serializes the config sidecar plus each (base64-decoded)
// image and drives every registered progress callback; previously uncovered.
BOOST_AUTO_TEST_CASE(test_native_to_string_and_callbacks)
{
    bool callback_hit = false;
    ImagesPath ip("cfgfile", "{}", {[](double, std::string_view) {}});
    // second callback flips a flag to prove callbacks are invoked by ToString()
    ip = ImagesPath("cfgfile", "{}", {[&](double, std::string_view) { callback_hit = true; }});
    ip.AddImage("one.png", "eA==");  // base64 of 'x'

    auto out = ip.ToString();
    BOOST_CHECK(callback_hit);
    BOOST_CHECK_NE(out.find("#cfgfile"), std::string::npos);
    BOOST_CHECK_NE(out.find("#one.png"), std::string::npos);
    BOOST_CHECK_NE(out.find("x"), std::string::npos);
}

// Native Save (no Lua) writes a zip archive carrying the config plus images.
BOOST_AUTO_TEST_CASE(test_native_save_creates_archive)
{
    ImagesPath ip("cfgfile", "{}");
    ip.AddImage("a.png", "eA==");

    auto tmp = std::filesystem::temp_directory_path() / "hsba_images_native.zip";
    std::error_code ec;
    std::filesystem::remove(tmp, ec);
    ip.Save(tmp);
    BOOST_CHECK(std::filesystem::exists(tmp));
    std::filesystem::remove(tmp, ec);
}

// The funcName overload injects a 'funcName' global before running the script and
// falls back to the native serialization when the script is empty.
BOOST_AUTO_TEST_CASE(test_to_string_funcname_and_empty_fallback)
{
    ImagesPath ip("cfg", "data");
    BOOST_CHECK_EQUAL(ip.ToString(std::string_view("return funcName"), "myFn"), "myFn");
    // empty script -> native ToString
    BOOST_CHECK_EQUAL(ip.ToString(std::string_view(""), "myFn"), ip.ToString());
}

// Empty script on the (script, lua_reg) overload short-circuits to native output.
BOOST_AUTO_TEST_CASE(test_to_string_empty_script_fallback)
{
    ImagesPath ip("cfg", "data");
    BOOST_CHECK_EQUAL(ip.ToString(std::string_view("")), ip.ToString());
}

// A non-null lua_reg callback is invoked to register extra globals for the script.
BOOST_AUTO_TEST_CASE(test_to_string_lua_reg_invoked)
{
    ImagesPath ip("cfg", "data");
    bool reg_hit = false;
    auto lua_reg = [&](lua_State*) { reg_hit = true; };
    ip.ToString("return 'ok'", lua_reg);
    BOOST_CHECK(reg_hit);
}

// The script-file ToString overload reads a Lua file from disk and runs it.
BOOST_AUTO_TEST_CASE(test_to_string_from_script_file)
{
    ImagesPath ip("cfg", "data");
    auto lua_file = std::filesystem::temp_directory_path() / "hsba_images_str.lua";
    std::error_code ec;
    std::filesystem::remove(lua_file, ec);
    {
        std::ofstream ofs(lua_file);
        ofs << "return 'from_file:' .. funcName";
    }
    auto out = ip.ToString(lua_file, "fn1");
    BOOST_CHECK_EQUAL(out, "from_file:fn1");
    std::filesystem::remove(lua_file, ec);
}

// Missing script files (both Save and ToString families) surface as RuntimeError.
BOOST_AUTO_TEST_CASE(test_missing_script_files_throw)
{
    ImagesPath ip("cfg", "data");
    auto missing = std::filesystem::path("hsba_no_such_script_file_xyz.lua");
    BOOST_CHECK_THROW(ip.ToString(missing, "fn"), HsBa::Slicer::RuntimeError);
    auto tmp = std::filesystem::temp_directory_path() / "hsba_images_missing.lua.out";
    BOOST_CHECK_THROW(ip.Save(tmp, missing, "fn"), HsBa::Slicer::RuntimeError);
}

// Lua load and runtime errors are wrapped into RuntimeError by every script overload.
BOOST_AUTO_TEST_CASE(test_lua_errors_throw)
{
    ImagesPath ip("cfg", "data");
    BOOST_CHECK_THROW(ip.ToString("%%% not lua"), HsBa::Slicer::RuntimeError);
    BOOST_CHECK_THROW(ip.ToString("error('images boom')"), HsBa::Slicer::RuntimeError);
    BOOST_CHECK_THROW(ip.ToString(std::string_view("%%% not lua"), "fn"), HsBa::Slicer::RuntimeError);
    BOOST_CHECK_THROW(ip.ToString(std::string_view("error('images boom')"), "fn"), HsBa::Slicer::RuntimeError);
}

// The funcName and script-file Save overloads write the script's returned string.
// Save now injects a 'funcName' global (parity with the ToString family), so the
// scripts below reference funcName to prove the injection works end to end.
BOOST_AUTO_TEST_CASE(test_save_funcname_and_script_file_variants)
{
    ImagesPath ip("cfg", "data");
    std::error_code ec;

    auto tmp1 = std::filesystem::temp_directory_path() / "hsba_images_fn.out";
    std::filesystem::remove(tmp1, ec);
    ip.Save(tmp1, std::string_view("return 'FN' .. funcName"), "x");
    BOOST_CHECK(std::filesystem::exists(tmp1));
    {
        std::ifstream ifs(tmp1, std::ios::binary);
        BOOST_CHECK_EQUAL(std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>()),
                          "FNx");
    }
    std::filesystem::remove(tmp1, ec);

    auto lua_file = std::filesystem::temp_directory_path() / "hsba_images_save.lua";
    std::filesystem::remove(lua_file, ec);
    {
        std::ofstream ofs(lua_file);
        ofs << "return 'FN' .. funcName";
    }
    auto tmp2 = std::filesystem::temp_directory_path() / "hsba_images_file.out";
    std::filesystem::remove(tmp2, ec);
    ip.Save(tmp2, lua_file, "y");
    BOOST_CHECK(std::filesystem::exists(tmp2));
    {
        std::ifstream ifs(tmp2, std::ios::binary);
        BOOST_CHECK_EQUAL(std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>()),
                          "FNy");
    }
    std::filesystem::remove(tmp2, ec);
    std::filesystem::remove(lua_file, ec);
}

// The (script) ToString overload registers the Cipher Lua library before running
// the chunk, so a script that round-trips through every Cipher.* binding drives
// all four l_base64_encode/decode/hex_encode/decode C functions and RegisterLuaCipher.
BOOST_AUTO_TEST_CASE(test_cipher_lua_bindings_via_script)
{
    ImagesPath ip("cfg", "data");
    const std::string script = R"lua(
local e1 = Cipher.base64_encode("abc")
local d1 = Cipher.base64_decode("YWJj")
local h1 = Cipher.hex_encode("AB")
local h2 = Cipher.hex_decode("4142")
return e1 .. "|" .. d1 .. "|" .. h1 .. "|" .. h2
)lua";
    BOOST_CHECK_EQUAL(ip.ToString(script), "YWJj|abc|4142|AB");

    // base64_encode("") is now valid (encodes to ""), so it no longer raises. Use a
    // binding that still throws: an invalid hex char makes Encoder::hex_decode raise
    // InvalidArgumentError; the l_hex_decode binding re-raises it as a Lua error and
    // ToString wraps the failed lua_pcall into a RuntimeError.
    BOOST_CHECK_THROW(ip.ToString("return Cipher.hex_decode(\"ZZ\")"), HsBa::Slicer::RuntimeError);
}

BOOST_AUTO_TEST_SUITE_END()
