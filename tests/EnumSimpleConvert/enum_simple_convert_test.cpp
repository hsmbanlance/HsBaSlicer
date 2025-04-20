#define BOOST_TEST_MODULE enum_simple_convert
#include <boost/test/included/unit_test.hpp>

#include "base/template_helper.hpp"

BOOST_AUTO_TEST_SUITE(enum_simple_convert)

namespace {
	enum class TestEnum
	{
		First,
		Second,
		Third
	};

	enum CStyleEnum
	{
		CStyleFirst,
		CStyleSecond,
		CStyleThird
	};
}

BOOST_AUTO_TEST_CASE(test_enum_name_template)
{
	BOOST_TEST_MESSAGE("Running enum_name test");
	BOOST_REQUIRE_MESSAGE(HsBa::Slicer::Utils::EnumName<TestEnum::First>() == "First", "Failed to get enum name");
	BOOST_REQUIRE_MESSAGE(HsBa::Slicer::Utils::EnumName<CStyleEnum::CStyleFirst>() == "CStyleFirst", "Failed to get enum name");
}

BOOST_AUTO_TEST_CASE(test_enum_name_args)
{
	BOOST_TEST_MESSAGE("Running enum_name_args test");
	BOOST_REQUIRE_MESSAGE(HsBa::Slicer::Utils::EnumName(TestEnum::First) == "First", "Failed to get enum name");
	BOOST_REQUIRE_MESSAGE(HsBa::Slicer::Utils::EnumName(CStyleEnum::CStyleFirst) == "CStyleFirst", "Failed to get enum name");
}

BOOST_AUTO_TEST_CASE(test_enum_value_form_string)
{
	BOOST_TEST_MESSAGE("Running enum_value_form_string test");
	BOOST_REQUIRE_MESSAGE(HsBa::Slicer::Utils::EnumFromName<TestEnum>("First") == TestEnum::First, "Failed to get enum value");
	BOOST_REQUIRE_MESSAGE(HsBa::Slicer::Utils::EnumFromName<CStyleEnum>("CStyleFirst") == CStyleEnum::CStyleFirst, "Failed to get enum value");
}

BOOST_AUTO_TEST_SUITE_END()