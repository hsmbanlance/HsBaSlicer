#define BOOST_TEST_MODULE anyvisit_test
#include <boost/test/included/unit_test.hpp>

#include <base/any_visit.hpp>

BOOST_AUTO_TEST_SUITE(any_visit)

BOOST_AUTO_TEST_CASE(test_std_any_one_arg)
{
	BOOST_TEST_MESSAGE("Running std_any_one_arg test");
	std::any any = 42;
	int v = HsBa::Slicer::Utils::Visit<int>([](int i)->int {
		return i + 1; }, any);
	BOOST_REQUIRE_MESSAGE(v == 43, "Visit failed");
	any = std::string{ "4" };
	v = HsBa::Slicer::Utils::Visit<int>([](int i)->int {
		return i + 1; }, any);
	BOOST_REQUIRE_MESSAGE(v == int{}, "Visit any without type in template and return not default value");
}

BOOST_AUTO_TEST_CASE(test_std_any_two_arg)
{
	BOOST_TEST_MESSAGE("Running std_any_two_arg test");
	std::any any = 42;
	auto v = HsBa::Slicer::Utils::Visit<int, double>([](auto&& i)->int {
		if (typeid(i) == typeid(int))	return 42;
		return 43;
		}, any);
	BOOST_REQUIRE_MESSAGE(v == 42, "Visit don't pick int");
	any = 42.0;
	v = HsBa::Slicer::Utils::Visit<int, double>([](auto&& i)->int {
		if (typeid(i) == typeid(int))	return 42;
		return 43;
		}, any);
	BOOST_REQUIRE_MESSAGE(v == 43, "Visit don't pick not int");
	any = 42.0f;
	v = HsBa::Slicer::Utils::Visit<int, double>([](auto&& i)->int {
		if (typeid(i) == typeid(int))	return 42;
		return 43;
		}, any);
	BOOST_REQUIRE_MESSAGE(v == int{}, "Visit any without type in template and return not default value");
}

BOOST_AUTO_TEST_CASE(test_std_any_without_typeid)
{
	BOOST_TEST_MESSAGE("Running std_any_without_typeid test");
	auto callback = [](auto&& i, int ar) ->int {return static_cast<int>(i + ar); };
	std::any any = 42;
	int v = HsBa::Slicer::Utils::Visit<int, double>(callback, any, 1);
	BOOST_REQUIRE_MESSAGE(v == 43, "Visit failed");
	any = 42.0;
	v = HsBa::Slicer::Utils::Visit<int, double>(callback, any, 1);
	BOOST_REQUIRE_MESSAGE(v == 43, "Visit failed");
	any = 42.0f;
	v = HsBa::Slicer::Utils::Visit<int, double>(callback, any, 1);
	BOOST_REQUIRE_MESSAGE(v == int{}, "Visit any without type in template and return not default value");
}

BOOST_AUTO_TEST_CASE(test_boost_any_one_arg)
{
	BOOST_TEST_MESSAGE("Running boost_any_one_arg test");
	boost::any any = 42;
	int v = HsBa::Slicer::Utils::Visit<int>([](int i)->int {
		return i + 1; }, any);
	BOOST_REQUIRE_MESSAGE(v == 43, "Visit failed");
	any = std::string{ "4" };
	v = HsBa::Slicer::Utils::Visit<int>([](int i)->int {
		return i + 1; }, any);
	BOOST_REQUIRE_MESSAGE(v == int{}, "Visit any without type in template and return not default value");
}

BOOST_AUTO_TEST_CASE(test_boost_any_two_arg)
{
	BOOST_TEST_MESSAGE("Running boost_any_two_arg test");
	boost::any any = 42;
	auto v = HsBa::Slicer::Utils::Visit<int, double>([](auto&& i)->int {
		if (typeid(i) == typeid(int))	return 42;
		return 43;
		}, any);
	BOOST_REQUIRE_MESSAGE(v == 42, "Visit don't pick int");
	any = 42.0;
	v = HsBa::Slicer::Utils::Visit<int, double>([](auto&& i)->int {
		if (typeid(i) == typeid(int))	return 42;
		return 43;
		}, any);
	BOOST_REQUIRE_MESSAGE(v == 43, "Visit don't pick not int");
	any = 42.0f;
	v = HsBa::Slicer::Utils::Visit<int, double>([](auto&& i)->int {
		if (typeid(i) == typeid(int))	return 42;
		return 43;
		}, any);
	BOOST_REQUIRE_MESSAGE(v == int{}, "Visit any without type in template and return not default value");
}

BOOST_AUTO_TEST_CASE(test_boost_any_without_typeid)
{
	BOOST_TEST_MESSAGE("Running boost_any_without_typeid test");
	auto callback = [](auto&& i, int ar) ->int {return static_cast<int>(i + ar); };
	boost::any any = 42;
	int v = HsBa::Slicer::Utils::Visit<int, double>(callback, any, 1);
	BOOST_REQUIRE_MESSAGE(v == 43, "Visit failed");
	any = 42.0;
	v = HsBa::Slicer::Utils::Visit<int, double>(callback, any, 1);
	BOOST_REQUIRE_MESSAGE(v == 43, "Visit failed");
	any = 42.0f;
	v = HsBa::Slicer::Utils::Visit<int, double>(callback, any, 1);
	BOOST_REQUIRE_MESSAGE(v == int{}, "Visit any without type in template and return not default value");
}

BOOST_AUTO_TEST_CASE(test_std_any_pointer)
{
	BOOST_TEST_MESSAGE("Running std_any_pointer test");
	int a_int = 42;
	std::any any = &a_int;
	HsBa::Slicer::Utils::Visit<int*>([](auto&& i) {++(*i); }, any);
	BOOST_REQUIRE_MESSAGE(*std::any_cast<int*>(any) == 43,"Visit don't change c style pointer");
	any = std::make_shared<int>(42);
	HsBa::Slicer::Utils::Visit<std::shared_ptr<int>>([](auto&& i) {++(*i); }, any);
	BOOST_REQUIRE_MESSAGE(*std::any_cast<std::shared_ptr<int>>(any) == 43, "Visit don't change shared pointer");
}

BOOST_AUTO_TEST_CASE(test_boost_any_pointer)
{
	BOOST_TEST_MESSAGE("Running boost_any_pointer test");
	int a_int = 42;
	boost::any any = &a_int;
	HsBa::Slicer::Utils::Visit<int*>([](auto&& i) {++(*i); }, any);
	BOOST_REQUIRE_MESSAGE(*boost::any_cast<int*>(any) == 43, "Visit don't change c style pointer");
	any = std::make_shared<int>(42);
	HsBa::Slicer::Utils::Visit<std::shared_ptr<int>>([](auto&& i) {++(*i); }, any);
	BOOST_REQUIRE_MESSAGE(*boost::any_cast<std::shared_ptr<int>>(any) == 43, "Visit don't change shared pointer");
}

BOOST_AUTO_TEST_SUITE_END()