#define BOOST_TEST_MODULE enum_simple_convert
#include <boost/test/included/unit_test.hpp>

#include <format>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

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

BOOST_AUTO_TEST_CASE(template_string_and_utils_runtime)
{
    namespace U = HsBa::Slicer::Utils;
    using namespace U::TemplateStringLiterals;

    // construction from a shorter literal (zero padded) and cross-size comparison
    constexpr U::TemplateString<char, 6> key{"name"};
    BOOST_CHECK((key == U::TemplateString<char, 5>{"name"}));

    // conversions: a padded buffer stops at the NUL, a full buffer uses the whole size
    U::TemplateString<char, 8> padded{"abc"};
    BOOST_CHECK(std::string(static_cast<std::string_view>(padded)) == "abc");
    BOOST_CHECK(padded.ToString() == "abc");

    U::TemplateString<char, 3> full{std::string_view("xyz")};
    BOOST_CHECK(std::string(static_cast<std::string_view>(full)) == "xyz");
    BOOST_CHECK(full.ToStringView() == std::string_view("xyz", 3));

    // case transform and split
    U::TemplateString<char, 5> word{"mix"};
    BOOST_CHECK(word.ToUpper().ToString().substr(0, 3) == "MIX");
    auto parts = U::TemplateString<char, 6>("a-b").Split<std::vector<std::string>>("-");
    BOOST_REQUIRE_EQUAL(parts.size(), 2u);
    BOOST_CHECK(parts[0] == "a");
    BOOST_CHECK(parts[1] == "b");

    // bounds-checked access
    BOOST_CHECK_EQUAL(full.at(1), 'y');
    BOOST_CHECK_THROW(full.at(3), std::out_of_range);

    // concatenation
    auto cat = U::TemplateString<char, 3>("ab") + U::TemplateString<char, 2>("c");
    BOOST_CHECK(std::string(static_cast<std::string_view>(cat)).substr(0, 3) == "abc");

    // stream insertion
    std::ostringstream os;
    os << padded;
    BOOST_CHECK(os.str().substr(0, 3) == "abc");

    // user-defined literal plus std::format, quoted and plain
    auto lit = "tag"_ts;
    BOOST_CHECK(std::format("{}", lit) == "tag");
    BOOST_CHECK(std::format("{:#}", lit) == "\"tag\"");

    // Invoke / AsyncInvoke
    BOOST_CHECK(U::Invoke([](int a) { return a * 2; }, 21) == 42);
    BOOST_CHECK(U::AsyncInvoke([] { return 5; }).get() == 5);

    // named raw pointer
    int value = 99;
    U::NamedRawPtr<"p", int> raw{&value};
    BOOST_CHECK(static_cast<bool>(raw));
    BOOST_CHECK_EQUAL(raw.Get(), &value);
    BOOST_CHECK_EQUAL(*raw, 99);
    BOOST_CHECK(raw.Name() == std::string_view("p"));
    U::NamedRawPtr<"n", int> nullPtr{};
    BOOST_CHECK(!static_cast<bool>(nullPtr));

    // named smart pointer
    U::NamedPtr<"sp", int, std::shared_ptr> smart{std::make_shared<int>(7)};
    BOOST_CHECK(static_cast<bool>(smart));
    BOOST_CHECK_EQUAL(*smart, 7);
    BOOST_CHECK(smart.Name() == std::string_view("sp"));

    // recursive YCombinator (present only without explicit-this-parameter)
#ifndef __cpp_explicit_this_parameter
    auto fact = U::YCombinator{
        [](auto&& self, int n) -> int
        { return n <= 1 ? 1 : n * self(n - 1); }};
    BOOST_CHECK_EQUAL(fact(5), 120);
#endif

    // non-copyable array fill
    struct MoveOnly
    {
        int v;
        MoveOnly(int x) : v(x)
        {
        }
        MoveOnly(MoveOnly&&) = default;
        MoveOnly(const MoveOnly&) = delete;
    };
    auto arr = U::MakeNonCopyableArray<MoveOnly, 3>(4);
    BOOST_CHECK_EQUAL(arr.size(), 3u);
    BOOST_CHECK_EQUAL(arr[0].v, 4);

    // EnumFromName on an unknown name yields the default value
    BOOST_CHECK(U::EnumFromName<TestEnum>("NotAValue") == TestEnum{});
}

BOOST_AUTO_TEST_SUITE_END()