#define BOOST_TEST_MODULE filename_test
#include <boost/test/included/unit_test.hpp>

#include "base/ModelFormat.hpp"
#include "base/filename_check.hpp"

#if _WIN32
#include <Windows.h>
#endif  // _WIN32

BOOST_AUTO_TEST_SUITE(filename_check)

BOOST_AUTO_TEST_CASE(test_check_enable_filename)
{
    BOOST_TEST_MESSAGE("Running check enable filename test");
#if _WIN32
    SetConsoleOutputCP(65001);  // for windows set console cp to utf-8
    SetConsoleCP(65001);
#endif  // _WIN32
    BOOST_REQUIRE(HsBa::Slicer::StringIsValidFileName("abc.sl"));
    BOOST_REQUIRE(HsBa::Slicer::StringIsValidFileName("加😊.sl"));
    BOOST_REQUIRE(!HsBa::Slicer::StringIsValidFileName("加?.sl"));
    BOOST_REQUIRE(!HsBa::Slicer::StringIsValidFileName("/加?.sl"));
    BOOST_REQUIRE(HsBa::Slicer::StringIsOnlyASCII("abc"));
    BOOST_REQUIRE(!HsBa::Slicer::StringIsOnlyASCII("abc加?"));
    BOOST_REQUIRE(HsBa::Slicer::StringIsValidPath("mm\\xsd//d"));
    BOOST_REQUIRE(!HsBa::Slicer::StringIsValidPath("mm\\xsd//"));
    BOOST_REQUIRE(!HsBa::Slicer::StringIsValidPath("mm/xsd\\"));
}

BOOST_AUTO_TEST_CASE(test_filename_ext)
{
    BOOST_TEST_MESSAGE("Running filename ext test");
#if _WIN32
    SetConsoleOutputCP(65001);  // for windows set console cp to utf-8
    SetConsoleCP(65001);
#endif  // _WIN32
    BOOST_REQUIRE(HsBa::Slicer::GetExtName("err.a").find('a') != std::string::npos);
    BOOST_REQUIRE(HsBa::Slicer::IsMeshFormat("err.stl"));
    BOOST_REQUIRE(HsBa::Slicer::IsBrepFormat("err.stp"));
    BOOST_REQUIRE(HsBa::Slicer::IsPointCloudFormat("err.xyz"));
}

// The wstring overloads mirror the std::string versions already covered above;
// these cases drive the wide-character implementations that were previously 0%.
BOOST_AUTO_TEST_CASE(test_wstring_overloads)
{
    using namespace HsBa::Slicer;
    // StringIsOnlyASCII(wstring): true iff every code unit is within the ASCII range
    BOOST_TEST(StringIsOnlyASCII(std::wstring(L"abc")));
    BOOST_TEST(!StringIsOnlyASCII(std::wstring(L"abc\u52a0")));
    // StringIsValidFileName(wstring): no illegal path characters and not empty
    BOOST_TEST(StringIsValidFileName(std::wstring(L"abc.sl")));
    BOOST_TEST(StringIsValidFileName(std::wstring(L"\u52a0.sl")));
    BOOST_TEST(!StringIsValidFileName(std::wstring(L"")));
    BOOST_TEST(!StringIsValidFileName(std::wstring(L"\u52a0?.sl")));
    BOOST_TEST(!StringIsValidFileName(std::wstring(L"/abc.sl")));
    // StringIsValidPath(wstring): separators allowed, last segment non-empty
    BOOST_TEST(StringIsValidPath(std::wstring(L"mm\\xsd//d")));
    BOOST_TEST(!StringIsValidPath(std::wstring(L"mm\\xsd//")));
    BOOST_TEST(!StringIsValidPath(std::wstring(L"mm/xsd\\")));
    BOOST_TEST(!StringIsValidPath(std::wstring(L"mm:xsd")));
}

// Combined ASCII+validity helpers. StringIsOnlyASCII reports "is pure ASCII", so
// the WithNonASCII helpers reduce to (!pure_ascii && valid), i.e. they are true for
// a valid name/path that carries at least one non-ASCII code unit.
BOOST_AUTO_TEST_CASE(test_ascii_combined_overloads)
{
    using namespace HsBa::Slicer;
    BOOST_TEST(StringIsValidFileNameWithNonASCII(std::string("\u52a0.sl")));
    BOOST_TEST(!StringIsValidFileNameWithNonASCII(std::string("abc.sl")));
    BOOST_TEST(!StringIsValidFileNameWithNonASCII(std::string("\u52a0?.sl")));
    BOOST_TEST(StringIsValidPathWithNonASCII(std::string("\u52a0/xsd")));
    BOOST_TEST(!StringIsValidPathWithNonASCII(std::string("mm/xsd")));
    // wstring mirrors the same composed behaviour
    BOOST_TEST(StringIsValidFileNameWithNonASCII(std::wstring(L"\u52a0.sl")));
    BOOST_TEST(!StringIsValidFileNameWithNonASCII(std::wstring(L"abc.sl")));
    BOOST_TEST(StringIsValidPathWithNonASCII(std::wstring(L"\u52a0\\xsd")));
    BOOST_TEST(!StringIsValidPathWithNonASCII(std::wstring(L"mm/xsd")));
}

BOOST_AUTO_TEST_SUITE_END()