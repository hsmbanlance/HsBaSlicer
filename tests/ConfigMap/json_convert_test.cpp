#define BOOST_TEST_MODULE filename_test
#include <boost/test/included/unit_test.hpp>

#include "utils/struct_json.hpp"

#if _WIN32
#include <Windows.h>
#endif // _WIN32

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

class NoJson {
public:
	NoJson(int){}
};

struct ThrowStruct {
	Person p;
	NoJson no;
};

class WithJson {
	int x = 0;
	int y = 0;
public:
	void to_json(rapidjson::Value& json, rapidjson::Document::AllocatorType& doc) const
	{
		json.SetObject();
		json.AddMember(rapidjson::StringRef("x"), x, doc);
		json.AddMember(rapidjson::StringRef("x"), x, doc);
	}
	static WithJson from_json(const rapidjson::Value& json)
	{
		WithJson v;
		if (json.HasMember("x") && json["x"].IsNumber())
		{
			v.x = json["x"].GetInt();
		}
		if (json.HasMember("y") && json["y"].IsNumber())
		{
			v.x = json["y"].GetInt();
		}
		return v;
	}
	bool operator==(const WithJson& o) const
	{
		return o.x == x && o.y == y;
	}
	friend std::ostream& operator<<(std::ostream& os, const WithJson& o)
	{
		return os << "x:" << o.x << ",y:" << o.y << ".";
	}
};

struct PersonMove {
	Person p;
	WithJson json;
};

BOOST_AUTO_TEST_SUITE(json_convert_test)

namespace Testing {
	struct TestStruct {
		int id;
		std::string name;
		double value;
	};
} // Testing

BOOST_AUTO_TEST_CASE(json_convert_simple_struct)
{
	BOOST_TEST_MESSAGE("Running JSON convert test");
	// Set console output to UTF-8 for Windows
#if _WIN32
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);
#endif // _WIN32
	// Create an instance of the struct
	Testing::TestStruct test_instance{ 1, "Test", 3.14 };
	// Convert to JSON
	auto json_doc = HsBa::Slicer::Utils::to_json(test_instance);
	BOOST_REQUIRE(json_doc.IsObject());
	BOOST_REQUIRE(json_doc.HasMember("id"));
	BOOST_REQUIRE(json_doc.HasMember("name"));
	BOOST_REQUIRE(json_doc.HasMember("value"));
	// Convert back from JSON
	auto converted_instance = HsBa::Slicer::Utils::from_json<Testing::TestStruct>(json_doc);
	BOOST_REQUIRE_EQUAL(converted_instance.id, test_instance.id);
	BOOST_REQUIRE_EQUAL(converted_instance.name, test_instance.name);
	BOOST_REQUIRE_EQUAL(converted_instance.value, test_instance.value);
	BOOST_TEST_MESSAGE("JSON convert test completed successfully");
}

BOOST_AUTO_TEST_CASE(json_convert_complex_struct)
{
	// Create instances of Person
	Person person1{ 30, "Alice",Sexual_Female };
	Person person2{ 32, "Bob", Sexual_Male };
	// Create a Couple instance
	Couple couple{ person1, person2 };
	// Convert Couple to JSON
	auto json_doc = HsBa::Slicer::Utils::to_json(couple);
	BOOST_REQUIRE(json_doc.IsObject());
	BOOST_REQUIRE(json_doc.HasMember("person1"));
	BOOST_REQUIRE(json_doc.HasMember("person2"));
	// Convert back from JSON
	Couple converted_couple = HsBa::Slicer::Utils::from_json<Couple>(json_doc);
	BOOST_REQUIRE_EQUAL(converted_couple.person1.age, couple.person1.age);
	BOOST_REQUIRE_EQUAL(converted_couple.person1.name, couple.person1.name);
	BOOST_REQUIRE_EQUAL(converted_couple.person1.sexuality, couple.person1.sexuality);
	BOOST_REQUIRE_EQUAL(converted_couple.person2.age, couple.person2.age);
	BOOST_REQUIRE_EQUAL(converted_couple.person2.name, couple.person2.name);
	BOOST_REQUIRE_EQUAL(converted_couple.person2.sexuality, couple.person2.sexuality);
	BOOST_TEST_MESSAGE("Complex JSON convert test completed successfully");
}

BOOST_AUTO_TEST_CASE(json_convert_nested_struct)
{
	// Create instances of Person
	Person person1{ 30, "Alice", Sexual_Female };
	Person person2{ 32, "Bob", Sexual_Male };
	Person child1{ 5, "Charlie", Sexual_Unknown };
	Person child2{ 3, "Daisy", Sexual_Female };
	// Create a Couple instance
	Couple couple{ person1, person2 };
	// Create a Family instance
	Family family{ couple, { child1, child2 } };
	// Convert Family to JSON
	auto json_doc = HsBa::Slicer::Utils::to_json(family);
	BOOST_REQUIRE(json_doc.IsObject());
	BOOST_REQUIRE(json_doc.HasMember("couple"));
	BOOST_REQUIRE(json_doc.HasMember("children"));
	// Convert back from JSON
	Family converted_family = HsBa::Slicer::Utils::from_json<Family>(json_doc);
	BOOST_REQUIRE_EQUAL(converted_family.couple.person1.age, family.couple.person1.age);
	BOOST_REQUIRE_EQUAL(converted_family.couple.person1.name, family.couple.person1.name);
	BOOST_REQUIRE_EQUAL(converted_family.couple.person1.sexuality, family.couple.person1.sexuality);
	BOOST_REQUIRE_EQUAL(converted_family.couple.person2.age, family.couple.person2.age);
	BOOST_REQUIRE_EQUAL(converted_family.couple.person2.name, family.couple.person2.name);
	BOOST_REQUIRE_EQUAL(converted_family.couple.person2.sexuality, family.couple.person2.sexuality);
	BOOST_REQUIRE_EQUAL(converted_family.children.size(), family.children.size());
	for (size_t i = 0; i < family.children.size(); ++i)
	{
		BOOST_REQUIRE_EQUAL(converted_family.children[i].age, family.children[i].age);
		BOOST_REQUIRE_EQUAL(converted_family.children[i].name, family.children[i].name);
		BOOST_REQUIRE_EQUAL(converted_family.children[i].sexuality, family.children[i].sexuality);
	}
	BOOST_TEST_MESSAGE("Nested JSON convert test completed successfully");
}

BOOST_AUTO_TEST_CASE(json_convert_with_json)
{
	// Create an instance of WithJson
	WithJson with_json_instance;
	// Convert to JSON
	auto json_doc = HsBa::Slicer::Utils::to_json(with_json_instance);
	BOOST_REQUIRE(json_doc.IsObject());
	// Convert back from JSON
	auto converted_json = HsBa::Slicer::Utils::from_json<WithJson>(json_doc);
	BOOST_REQUIRE_EQUAL(with_json_instance, converted_json);
	BOOST_TEST_MESSAGE("WithJson JSON convert test completed successfully");
}

BOOST_AUTO_TEST_CASE(struct_with_json_convert_with_json)
{
	// Create an instance of WithJson
	WithJson with_json_instance;
	PersonMove person_move_instance{ { 25, "Eve", Sexual_Male }, with_json_instance };
	// Convert to JSON
	auto json_doc = HsBa::Slicer::Utils::to_json(person_move_instance);
	BOOST_REQUIRE(json_doc.IsObject());
	BOOST_REQUIRE(json_doc.HasMember("p"));
	BOOST_REQUIRE(json_doc.HasMember("json"));
	// Convert back from JSON
	PersonMove converted_person_move = HsBa::Slicer::Utils::from_json<PersonMove>(json_doc);
	BOOST_REQUIRE_EQUAL(converted_person_move.p.age, person_move_instance.p.age);
	BOOST_REQUIRE_EQUAL(converted_person_move.p.name, person_move_instance.p.name);
	BOOST_REQUIRE_EQUAL(converted_person_move.p.sexuality, person_move_instance.p.sexuality);
	BOOST_REQUIRE_EQUAL(converted_person_move.json , person_move_instance.json);
	BOOST_TEST_MESSAGE("PersonMove JSON convert test completed successfully");
}

BOOST_AUTO_TEST_CASE(json_stream)
{
	// Create an instance of the struct
	Testing::TestStruct test_instance{ 1, "Test", 3.14 };
	// Write to JSON stream
	std::ostringstream os;
	HsBa::Slicer::Utils::write_json(os, test_instance);
	BOOST_REQUIRE(!os.str().empty());
	// Read from JSON stream
	std::istringstream is(os.str());
	auto converted_instance = HsBa::Slicer::Utils::read_json<Testing::TestStruct>(is);
	BOOST_REQUIRE_EQUAL(converted_instance.id, test_instance.id);
	BOOST_REQUIRE_EQUAL(converted_instance.name, test_instance.name);
	BOOST_REQUIRE_EQUAL(converted_instance.value, test_instance.value);
	BOOST_TEST_MESSAGE("JSON stream test completed successfully");
}

BOOST_AUTO_TEST_CASE(test_json_throw)
{
	static_assert(!std::is_aggregate_v<NoJson>);
	Person p{ .age = 0,.name = "UnDefine",.sexuality = Sexual_Unknown };
	NoJson no{ 1 };
	ThrowStruct th{ p,no };
	BOOST_REQUIRE_THROW(HsBa::Slicer::Utils::to_json(th), HsBa::Slicer::RuntimeError);
	BOOST_TEST_MESSAGE("Member must be aggregate or number or string or enum or which has to_string and from_string methods");
}

BOOST_AUTO_TEST_SUITE_END()
