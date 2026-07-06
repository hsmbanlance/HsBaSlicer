#define BOOST_TEST_MODULE struct_xml_test
#include <boost/test/included/unit_test.hpp>

#include "utils/struct_json.hpp"  // for comparison
#include "utils/struct_xml.hpp"

#include <optional>
#include <string>
#include <vector>

using namespace HsBa::Slicer::Utils;

struct SimpleStruct
{
    int id;
    std::string name;
    double value;
};

struct NestedStruct
{
    SimpleStruct simple;
    std::vector<int> numbers;
    std::optional<std::string> optional_str;
};

enum class TestEnum
{
    Value1,
    Value2,
    Value3
};

struct EnumStruct
{
    TestEnum enum_value;
    std::string desc;
};

BOOST_AUTO_TEST_SUITE(struct_xml_test)

BOOST_AUTO_TEST_CASE(test_simple_struct)
{
    SimpleStruct original{42, "test", 3.14};
    auto xml_str = write_xml(original);

    BOOST_TEST_MESSAGE("Generated XML: " << xml_str);

    // Parse back
    auto parsed = read_xml_from_string<SimpleStruct>(xml_str);

    BOOST_REQUIRE_EQUAL(parsed.id, original.id);
    BOOST_REQUIRE_EQUAL(parsed.name, original.name);
    BOOST_REQUIRE_CLOSE(parsed.value, original.value, 1e-6);
}

BOOST_AUTO_TEST_CASE(test_nested_struct)
{
    NestedStruct original{{1, "nested", 2.71}, {10, 20, 30}, "optional value"};

    auto xml_str = write_xml(original);
    BOOST_TEST_MESSAGE("Generated XML: " << xml_str);

    auto parsed = read_xml_from_string<NestedStruct>(xml_str);

    BOOST_REQUIRE_EQUAL(parsed.simple.id, original.simple.id);
    BOOST_REQUIRE_EQUAL(parsed.simple.name, original.simple.name);
    BOOST_REQUIRE_CLOSE(parsed.simple.value, original.simple.value, 1e-6);

    BOOST_REQUIRE_EQUAL(parsed.numbers.size(), original.numbers.size());
    for (size_t i = 0; i < parsed.numbers.size(); ++i)
    {
        BOOST_REQUIRE_EQUAL(parsed.numbers[i], original.numbers[i]);
    }

    BOOST_REQUIRE(parsed.optional_str.has_value());
    BOOST_REQUIRE_EQUAL(*parsed.optional_str, *original.optional_str);
}

BOOST_AUTO_TEST_CASE(test_enum_struct)
{
    EnumStruct original{TestEnum::Value2, "description"};
    auto xml_str = write_xml(original);
    BOOST_TEST_MESSAGE("Generated XML: " << xml_str);

    auto parsed = read_xml_from_string<EnumStruct>(xml_str);

    BOOST_REQUIRE_EQUAL(static_cast<int>(parsed.enum_value), static_cast<int>(original.enum_value));
    BOOST_REQUIRE_EQUAL(parsed.desc, original.desc);
}

BOOST_AUTO_TEST_CASE(test_optional_null)
{
    NestedStruct original{{1, "nested", 2.71}, {10, 20, 30}, std::nullopt};

    auto xml_str = write_xml(original);
    BOOST_TEST_MESSAGE("Generated XML: " << xml_str);

    auto parsed = read_xml_from_string<NestedStruct>(xml_str);

    BOOST_REQUIRE(!parsed.optional_str.has_value());
}

BOOST_AUTO_TEST_CASE(test_vector_of_structs)
{
    std::vector<SimpleStruct> original{{1, "first", 1.0}, {2, "second", 2.0}, {3, "third", 3.0}};

    auto xml_str = write_xml(original);
    BOOST_TEST_MESSAGE("Generated XML: " << xml_str);

    auto parsed = read_xml_from_string<std::vector<SimpleStruct>>(xml_str);

    BOOST_REQUIRE_EQUAL(parsed.size(), original.size());
    for (size_t i = 0; i < parsed.size(); ++i)
    {
        BOOST_REQUIRE_EQUAL(parsed[i].id, original[i].id);
        BOOST_REQUIRE_EQUAL(parsed[i].name, original[i].name);
        BOOST_REQUIRE_CLOSE(parsed[i].value, original[i].value, 1e-6);
    }
}

BOOST_AUTO_TEST_CASE(test_write_to_file)
{
    SimpleStruct original{123, "file_test", 9.87};
    std::string filename = "test_struct.xml";

    write_xml(filename, original);

    // Read back
    auto parsed = read_xml_from_file<SimpleStruct>(filename);

    BOOST_REQUIRE_EQUAL(parsed.id, original.id);
    BOOST_REQUIRE_EQUAL(parsed.name, original.name);
    BOOST_REQUIRE_CLOSE(parsed.value, original.value, 1e-6);

    // Clean up
    std::remove(filename.c_str());
}

BOOST_AUTO_TEST_SUITE_END()