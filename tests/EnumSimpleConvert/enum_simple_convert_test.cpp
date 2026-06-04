#define BOOST_TEST_MODULE enum_simple_convert
#include <boost/test/included/unit_test.hpp>

#include "base/template_helper.hpp"

enum class EnumWithInvidValue
{
    Invalid = -1,
    Unknown = 0,
    First = 1,
    Second = 2,
    Third = 3,
    UnDefined = 255,
};
template <>
struct magic_enum::customize::enum_range<EnumWithInvidValue>
{
    static constexpr int min = static_cast<int>(EnumWithInvidValue::Unknown);
    static constexpr int max = static_cast<int>(EnumWithInvidValue::UnDefined);
};


BOOST_AUTO_TEST_SUITE(enum_simple_convert)

namespace
{
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
}  // namespace

BOOST_AUTO_TEST_CASE(test_enum_name)
{
    BOOST_TEST_MESSAGE("Running enum_name test");
    BOOST_REQUIRE_MESSAGE(HsBa::Slicer::Utils::EnumName<TestEnum::First>() == "First", "Failed to get enum name");
    BOOST_REQUIRE_MESSAGE(HsBa::Slicer::Utils::EnumName<CStyleEnum::CStyleFirst>() == "CStyleFirst",
                          "Failed to get enum name");
}

BOOST_AUTO_TEST_CASE(test_enum_name_args)
{
    BOOST_TEST_MESSAGE("Running enum_name_args test");
    BOOST_REQUIRE_MESSAGE(HsBa::Slicer::Utils::EnumName(TestEnum::First) == "First", "Failed to get enum name");
    BOOST_REQUIRE_MESSAGE(HsBa::Slicer::Utils::EnumName(CStyleEnum::CStyleFirst) == "CStyleFirst",
                          "Failed to get enum name");
}

BOOST_AUTO_TEST_CASE(test_enum_value_form_string)
{
    BOOST_TEST_MESSAGE("Running enum_value_form_string test");
    BOOST_REQUIRE_MESSAGE(HsBa::Slicer::Utils::EnumFromName<TestEnum>("First") == TestEnum::First,
                          "Failed to get enum value");
    BOOST_REQUIRE_MESSAGE(HsBa::Slicer::Utils::EnumFromName<CStyleEnum>("CStyleFirst") == CStyleEnum::CStyleFirst,
                          "Failed to get enum value");
}

BOOST_AUTO_TEST_CASE(test_enum_all_valid_enum)
{
    BOOST_TEST_MESSAGE("Running all_valid_enum test");
    auto result = HsBa::Slicer::Utils::AllValidEnum<TestEnum>(TestEnum::First,
                                                              []
                                                              {
                                                                  BOOST_TEST_MESSAGE("Function called");
                                                                  return 42;
                                                              });
    BOOST_REQUIRE_MESSAGE(result == 42, "Failed to get valid enum value");
    auto resultInvalid = HsBa::Slicer::Utils::AllValidEnum<TestEnum>(static_cast<TestEnum>(-1),
                                                                     []
                                                                     {
                                                                         BOOST_TEST_MESSAGE("Function called");
                                                                         return 42;
                                                                     });
    BOOST_REQUIRE_MESSAGE(resultInvalid == 0, "Failed to get invalid enum value");
    resultInvalid = HsBa::Slicer::Utils::AllValidEnum<EnumWithInvidValue>(EnumWithInvidValue::Invalid,
                                                                          []
                                                                          {
                                                                              BOOST_TEST_MESSAGE("Function called");
                                                                              return 42;
                                                                          });
    BOOST_REQUIRE_MESSAGE(resultInvalid == 0, "Failed to get invalid enum value");
    resultInvalid = HsBa::Slicer::Utils::AllValidEnum<EnumWithInvidValue>(EnumWithInvidValue::UnDefined,
                                                                          []
                                                                          {
                                                                              BOOST_TEST_MESSAGE("Function called");
                                                                              return 42;
                                                                          });
    BOOST_REQUIRE_MESSAGE(resultInvalid == 0, "Failed to get invalid enum value");
    resultInvalid = HsBa::Slicer::Utils::AllValidEnum<EnumWithInvidValue>(EnumWithInvidValue::Unknown,
                                                                          []
                                                                          {
                                                                              BOOST_TEST_MESSAGE("Function called");
                                                                              return 42;
                                                                          });
    BOOST_REQUIRE_MESSAGE(resultInvalid == 0, "Failed to get invalid enum value");
    result = HsBa::Slicer::Utils::AllValidEnum<EnumWithInvidValue>(EnumWithInvidValue::First,
                                                                   []
                                                                   {
                                                                       BOOST_TEST_MESSAGE("Function called");
                                                                       return 42;
                                                                   });
    BOOST_REQUIRE_MESSAGE(result == 42, "Failed to get valid enum value");

    result = HsBa::Slicer::Utils::AllValidEnum<EnumWithInvidValue>(
        EnumWithInvidValue::Second,
        []
        {
            BOOST_TEST_MESSAGE("Function called");
            return 42;
        },
        []
        {
            BOOST_TEST_MESSAGE("Default function called");
            return -1;
        });
    BOOST_REQUIRE_MESSAGE(result == 42, "Failed to get valid enum value with default");
    resultInvalid = HsBa::Slicer::Utils::AllValidEnum<EnumWithInvidValue>(
        EnumWithInvidValue::Invalid,
        []
        {
            BOOST_TEST_MESSAGE("Function called");
            return 42;
        },
        []
        {
            BOOST_TEST_MESSAGE("Default function called");
            return -1;
        });
    BOOST_REQUIRE_MESSAGE(resultInvalid == -1, "Failed to get invalid enum value with default");
}

BOOST_AUTO_TEST_SUITE_END()