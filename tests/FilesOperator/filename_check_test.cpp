#define BOOST_TEST_MODULE filename_test
#include <boost/test/included/unit_test.hpp>

#include "base/filename_check.hpp"
#include "base/ModelFormat.hpp"

#if _WIN32
#include <Windows.h>
#endif // _WIN32

BOOST_AUTO_TEST_SUITE(filename_check)

BOOST_AUTO_TEST_CASE(test_check_enable_filename)
{
	BOOST_TEST_MESSAGE("Running check enable filename test");
#if _WIN32
	SetConsoleOutputCP(65001);//for windows set console cp to utf-8
	SetConsoleCP(65001);
#endif // _WIN32
	BOOST_REQUIRE(HsBa::Slicer::StringEnableFileName("abc.sl"));
	BOOST_REQUIRE(HsBa::Slicer::StringEnableFileName("加😊.sl"));
	BOOST_REQUIRE(!HsBa::Slicer::StringEnableFileName("加?.sl"));
	BOOST_REQUIRE(!HsBa::Slicer::StringEnableFileName("/加?.sl"));
	BOOST_REQUIRE(HsBa::Slicer::StringWithNoASCII("abc"));
	BOOST_REQUIRE(!HsBa::Slicer::StringWithNoASCII("abc加?"));
	BOOST_REQUIRE(HsBa::Slicer::StringEnableFileNameWithPath("mm\\xsd//d"));
	BOOST_REQUIRE(!HsBa::Slicer::StringEnableFileNameWithPath("mm\\xsd//"));
	BOOST_REQUIRE(!HsBa::Slicer::StringEnableFileNameWithPath("mm/xsd\\"));
}

BOOST_AUTO_TEST_CASE(test_filename_ext)
{
	BOOST_TEST_MESSAGE("Running filename ext test");
#if _WIN32
	SetConsoleOutputCP(65001);//for windows set console cp to utf-8
	SetConsoleCP(65001);
#endif // _WIN32
	BOOST_REQUIRE(HsBa::Slicer::GetExtName("err.a").find('a')!=std::string::npos);
	BOOST_REQUIRE(HsBa::Slicer::IsMeshFormat("err.stl"));
	BOOST_REQUIRE(HsBa::Slicer::IsBrepFormat("err.stp"));
	BOOST_REQUIRE(HsBa::Slicer::IsPointCloudFormat("err.xyz"));
}

BOOST_AUTO_TEST_SUITE_END()