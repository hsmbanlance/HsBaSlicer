#define BOOST_TEST_MODULE filename_test
#include <boost/test/included/unit_test.hpp>

#include "base/string_helper.hpp"

#if _WIN32
#include <Windows.h>
#endif // _WIN32

BOOST_AUTO_TEST_SUITE(string_utils_test)

BOOST_AUTO_TEST_CASE(test_trim)
{
	std::string org = "   abc   ";
	BOOST_REQUIRE_EQUAL(HsBa::Slicer::Utils::trim(org), "abc");
	BOOST_REQUIRE_EQUAL(HsBa::Slicer::Utils::trim_left(org), "abc   ");
	BOOST_REQUIRE_EQUAL(HsBa::Slicer::Utils::trim_right(org), "   abc");
}

BOOST_AUTO_TEST_CASE(test_split)
{
	constexpr std::string_view org = "Hello**Ranges**split";
	auto res = HsBa::Slicer::Utils::split<std::string_view>(org, "**");
	std::vector<std::string> hope{ "Hello","Ranges","split" };
	for (size_t i = 0; i != 3; ++i)
	{
		BOOST_REQUIRE_EQUAL(res[i], hope[i]);
	}
	BOOST_REQUIRE_EQUAL(res.size(), hope.size());
}

BOOST_AUTO_TEST_CASE(test_regex_split)
{
	constexpr std::string_view org = "HelloD_Regexd_split";
	auto res = HsBa::Slicer::Utils::regex_split<std::string_view>(org, "[Dd]_");
	std::vector<std::string> hope{ "Hello","Regex","split" };
	for (size_t i = 0; i != 3; ++i)
	{
		BOOST_REQUIRE_EQUAL(res[i], hope[i]);
	}
	BOOST_REQUIRE_EQUAL(res.size(), hope.size());
	const std::string org2 = "HelloD_Regexd_split";
	auto res2 = HsBa::Slicer::Utils::regex_split<std::string>(org2, "[Dd]_");
	for (size_t i = 0; i != 3; ++i)
	{
		BOOST_REQUIRE_EQUAL(res2[i], hope[i]);
	}
	BOOST_REQUIRE_EQUAL(res2.size(), hope.size());
}


BOOST_AUTO_TEST_CASE(test_upper_lower)
{
	std::string org = "Abc";
	BOOST_REQUIRE_EQUAL(HsBa::Slicer::Utils::to_lower(org), "abc");
	BOOST_REQUIRE_EQUAL(HsBa::Slicer::Utils::to_upper(org), "ABC");
}

BOOST_AUTO_TEST_SUITE_END()