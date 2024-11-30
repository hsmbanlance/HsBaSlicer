#define BOOST_TEST_MODULE boost_std_convert
#include <boost/test/included/unit_test.hpp>

#include "base/boost_std_convert.hpp"

BOOST_AUTO_TEST_SUITE(boost_std_convert)

BOOST_AUTO_TEST_CASE(test_from_variant)
{
	BOOST_TEST_MESSAGE("Running from_variant test");
	std::variant<int, double> std_v = 1;
	auto boost_v = HsBa::Slicer::Utils::CopyToBoostVariant(std_v);
	auto boost_v2 = HsBa::Slicer::Utils::CopyToBoostVariant2(std_v);
	BOOST_REQUIRE_MESSAGE(boost::get<int>(boost_v) == 1,"Failed to convert from std::variant to boost::variant");
	BOOST_REQUIRE_MESSAGE(boost::variant2::get<int>(boost_v2) == 1,"Failed to convert from std::variant to boost::variant2");
}

BOOST_AUTO_TEST_CASE(test_from_boost_variant)
{
	BOOST_TEST_MESSAGE("Running from_boost_variant test");
	boost::variant<int, double> boost_v = 1;
	auto std_v = HsBa::Slicer::Utils::CopyToVariant(boost_v);
	BOOST_REQUIRE_MESSAGE(std::get<int>(std_v) == 1,"Failed to convert from boost::variant to std::variant");
	auto boost_v2 = HsBa::Slicer::Utils::CopyToBoostVariant2(std_v);
	BOOST_REQUIRE_MESSAGE(boost::variant2::get<int>(boost_v2) == 1,"Failed to convert from boost::variant to boost::variant2");
}

BOOST_AUTO_TEST_CASE(test_from_boost_variant2)
{
	BOOST_TEST_MESSAGE("Running from_boost_variant2 test");
	boost::variant2::variant<int, double> boost_v2 = 1;
	auto std_v = HsBa::Slicer::Utils::CopyToVariant(boost_v2);
	BOOST_REQUIRE_MESSAGE(std::get<int>(std_v) == 1,"Failed to convert from boost::variant2 to std::variant");
	auto boost_v = HsBa::Slicer::Utils::CopyToBoostVariant(boost_v2);
	BOOST_REQUIRE_MESSAGE(boost::get<int>(boost_v) == 1,"Failed to convert from boost::variant2 to boost::variant");
}

BOOST_AUTO_TEST_CASE(test_from_optional)
{
	BOOST_TEST_MESSAGE("Running from_optional test");
	std::optional<int> o = std::nullopt;
	auto boost_o = HsBa::Slicer::Utils::CopyToBoostOptional(o);
	BOOST_REQUIRE_MESSAGE(!boost_o.has_value(),"Failed to convert from std::optional to boost::optional");
}

BOOST_AUTO_TEST_CASE(test_from_boost_optional)
{
	BOOST_TEST_MESSAGE("Running from_boost_optional test");
	boost::optional<int> boost_o = 1;
	auto o = HsBa::Slicer::Utils::CopyToOptional(boost_o);
	BOOST_REQUIRE_MESSAGE(o.value() == 1,"Failed to convert from boost::optional to std::optional");
}

BOOST_AUTO_TEST_SUITE_END()