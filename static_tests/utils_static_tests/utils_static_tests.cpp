#include <string>

// test for json xml yaml serialization

enum class TestEnum
{
    Value1,
    Value2,
    Value3
};

struct TestStruct
{
    TestEnum enumValue;
    int intValue;
    double doubleValue;
    std::string stringValue;
};


#include "utils/struct_json.hpp"
#include "utils/struct_xml.hpp"
#include "utils/struct_yaml.hpp"

using namespace HsBa::Slicer::Utils::TemplateStringLiterals;

// add reflectable for TestClass

class TestClass
{
public:
    TestClass() = default;
    TestClass(TestEnum enumValue, int intValue, double doubleValue, std::string stringValue)
        : enumValue(enumValue), intValue(intValue), doubleValue(doubleValue), stringValue(stringValue)
    {
    }

private:
    TestEnum enumValue;
    int intValue;
    double doubleValue;
    std::string stringValue;

public:
    using FieldList = std::tuple<
        HsBa::Slicer::Utils::StaticReflect::FieldInfo<TestClass, TestEnum, "enumValue"_ts, &TestClass::enumValue>,
        HsBa::Slicer::Utils::StaticReflect::FieldInfo<TestClass, int, "intValue"_ts, &TestClass::intValue>,
        HsBa::Slicer::Utils::StaticReflect::FieldInfo<TestClass, double, "doubleValue"_ts, &TestClass::doubleValue>,
        HsBa::Slicer::Utils::StaticReflect::FieldInfo<TestClass, std::string, "stringValue"_ts,
                                                      &TestClass::stringValue>>;
    using MethodList = std::tuple<>;
    constexpr static auto ClassName = "TestClass";
};

void JsonXmlYamlTests()
{
    TestStruct testStruct{TestEnum::Value2, 42, 3.14, "Hello World"};

    // Test JSON serialization
    auto jsonStr_struct = HsBa::Slicer::Utils::to_json(testStruct);
    TestStruct deserializedJsonStruct = HsBa::Slicer::Utils::from_json<TestStruct>(jsonStr_struct);

    // Test XML serialization
    auto xmlStr_struct = HsBa::Slicer::Utils::to_xml(testStruct);
    TestStruct deserializedXmlStruct = HsBa::Slicer::Utils::read_xml_from_string<TestStruct>("");

    // Test YAML serialization
    auto yamlStr_struct = HsBa::Slicer::Utils::to_yaml(testStruct);
    TestStruct deserializedYamlStruct = HsBa::Slicer::Utils::read_yaml_from_string<TestStruct>("");

    TestClass testObj(TestEnum::Value2, 42, 3.14, "Hello World");

    // Test JSON serialization
    auto jsonStr = HsBa::Slicer::Utils::to_json(testObj);
    TestClass deserializedJsonObj = HsBa::Slicer::Utils::from_json<TestClass>(jsonStr);

    // Test XML serialization
    auto xmlStr = HsBa::Slicer::Utils::to_xml(testObj);
    TestClass deserializedXmlObj = HsBa::Slicer::Utils::read_xml_from_string<TestClass>("");

    // Test YAML serialization
    auto yamlStr = HsBa::Slicer::Utils::to_yaml(testObj);
    TestClass deserializedYamlObj = HsBa::Slicer::Utils::read_yaml_from_string<TestClass>("");
}

#include "utils/LuaNewObject.hpp"

void LuaNewObjectTests()
{
    TestStruct testStruct{TestEnum::Value2, 42, 3.14, "Hello World"};
    auto luaState = HsBa::Slicer::MakeUniqueLuaState();


    auto* luaObj = HsBa::Slicer::NewLuaObject<TestStruct, "TestStruct">(luaState.get(), testStruct);
    HsBa::Slicer::LuaGC<TestStruct, "TestStruct">(luaState.get());
}

#include "utils/LuaAnyObject.hpp"

void LuaAnyObjectTests()
{
    auto luaState = HsBa::Slicer::MakeUniqueLuaState();
    HsBa::Slicer::LuaInt intType;
    HsBa::Slicer::LuaDouble doubleType;
    std::vector<HsBa::Slicer::LuaAnyObjectNewCastBase*> addedTypes = {&intType, &doubleType};
    HsBa::Slicer::RegisterAnyObject(luaState.get(), addedTypes);
}
