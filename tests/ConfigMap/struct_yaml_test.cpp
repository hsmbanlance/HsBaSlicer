#define BOOST_TEST_MODULE struct_yaml_test
#include <boost/test/included/unit_test.hpp>
#include <string>
#include <vector>

#include "utils/struct_yaml.hpp"
#include <base/static_reflect.hpp>

using namespace HsBa::Slicer::Utils::TemplateStringLiterals;

namespace Testing {
	struct TestStruct {
		int id;
		std::string name;
		double value;
	};
}

enum Sexual {
	Sexual_Unknown,
	Sexual_Female,
	Sexual_Male,
	Sexual_Undefined
};

std::ostream& operator<< (std::ostream & os, Sexual sex)
{
	return os << HsBa::Slicer::Utils::EnumName(sex);
}

struct Person {
	int age;
	std::string name;
	Sexual sexuality;
};

struct Couple {
	Person person1;
	Person person2;
};

struct Family {
	Couple couple;
	std::vector<Person> children;
};

struct ReflectPerson {
	int age;
	std::string name;
	Sexual sexuality;
	using FieldList = std::tuple<
		HsBa::Slicer::Utils::StaticReflect::FieldInfo<ReflectPerson, int, "age"_ts, &ReflectPerson::age>,
		HsBa::Slicer::Utils::StaticReflect::FieldInfo<ReflectPerson, std::string, "name"_ts, &ReflectPerson::name>,
		HsBa::Slicer::Utils::StaticReflect::FieldInfo<ReflectPerson, Sexual, "sexuality"_ts, &ReflectPerson::sexuality>>;
	using MethodList = std::tuple<>;
	constexpr static auto ClassName = "ReflectPerson"_ts;
};

BOOST_AUTO_TEST_SUITE(struct_yaml_test)

BOOST_AUTO_TEST_CASE(yaml_convert_simple_struct)
{
	Testing::TestStruct test_instance{ 1, "Test", 3.14 };
	auto yaml_node = HsBa::Slicer::Utils::to_yaml(test_instance);
	BOOST_REQUIRE(yaml_node.IsMap());
	BOOST_REQUIRE(yaml_node["id"]);
	BOOST_REQUIRE(yaml_node["name"]);
	BOOST_REQUIRE(yaml_node["value"]);
	auto converted = HsBa::Slicer::Utils::from_yaml<Testing::TestStruct>(yaml_node);
	BOOST_REQUIRE_EQUAL(converted.id, test_instance.id);
	BOOST_REQUIRE_EQUAL(converted.name, test_instance.name);
	BOOST_REQUIRE_EQUAL(converted.value, test_instance.value);
}

BOOST_AUTO_TEST_CASE(yaml_convert_nested_struct)
{
	Person person1{ 30, "Alice", Sexual_Female };
	Person person2{ 32, "Bob", Sexual_Male };
	Person child1{ 5, "Charlie", Sexual_Unknown };
	Person child2{ 3, "Daisy", Sexual_Female };
	Couple couple{ person1, person2 };
	Family family{ couple, { child1, child2 } };
	auto yaml_node = HsBa::Slicer::Utils::to_yaml(family);
	BOOST_REQUIRE(yaml_node.IsMap());
	BOOST_REQUIRE(yaml_node["couple"]);
	BOOST_REQUIRE(yaml_node["children"]);
	Family converted = HsBa::Slicer::Utils::from_yaml<Family>(yaml_node);
	BOOST_REQUIRE_EQUAL(converted.couple.person1.age, family.couple.person1.age);
	BOOST_REQUIRE_EQUAL(converted.couple.person1.name, family.couple.person1.name);
	BOOST_REQUIRE_EQUAL(converted.couple.person1.sexuality, family.couple.person1.sexuality);
	BOOST_REQUIRE_EQUAL(converted.children.size(), family.children.size());
	for (size_t i = 0; i < family.children.size(); ++i)
	{
		BOOST_REQUIRE_EQUAL(converted.children[i].age, family.children[i].age);
		BOOST_REQUIRE_EQUAL(converted.children[i].name, family.children[i].name);
		BOOST_REQUIRE_EQUAL(converted.children[i].sexuality, family.children[i].sexuality);
	}
}

BOOST_AUTO_TEST_CASE(yaml_convert_reflectable_struct)
{
	ReflectPerson rp{ 28, "Fiona", Sexual_Female };
	auto yaml_node = HsBa::Slicer::Utils::to_yaml(rp);
	BOOST_REQUIRE(yaml_node.IsMap());
	BOOST_REQUIRE(yaml_node["age"]);
	BOOST_REQUIRE(yaml_node["name"]);
	BOOST_REQUIRE(yaml_node["sexuality"]);
	auto converted = HsBa::Slicer::Utils::from_yaml<ReflectPerson>(yaml_node);
	BOOST_REQUIRE_EQUAL(converted.age, rp.age);
	BOOST_REQUIRE_EQUAL(converted.name, rp.name);
	BOOST_REQUIRE_EQUAL(converted.sexuality, rp.sexuality);
}

BOOST_AUTO_TEST_CASE(yaml_stream_roundtrip)
{
	Testing::TestStruct test_instance{ 2, "StreamTest", 7.25 };
	std::ostringstream os;
	HsBa::Slicer::Utils::write_yaml(os, test_instance);
	BOOST_REQUIRE(!os.str().empty());
	std::istringstream is(os.str());
	auto converted = HsBa::Slicer::Utils::read_yaml<Testing::TestStruct>(is);
	BOOST_REQUIRE_EQUAL(converted.id, test_instance.id);
	BOOST_REQUIRE_EQUAL(converted.name, test_instance.name);
	BOOST_REQUIRE_EQUAL(converted.value, test_instance.value);
}

BOOST_AUTO_TEST_SUITE_END()
