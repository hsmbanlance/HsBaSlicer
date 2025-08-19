#define BOOST_TEST_MODULE configmap_test
#include <boost/test/included/unit_test.hpp>

#include <format>

#include <Eigen/Core>

#include "fileoperator/rw_ptree.hpp"
#include "base/eigen_translator.hpp"

BOOST_AUTO_TEST_SUITE(configmap)

BOOST_AUTO_TEST_CASE(test_add_int_double_bool_string_to_variantconfigmap)
{
	BOOST_TEST_MESSAGE("Running add_int_double_bool_string_to_variantconfigmap test");
	HsBa::Slicer::Config::VariantConfigMap<int, double, bool, std::string> map;
	map.AddOrChangeValue("int", 1);
	map.AddOrChangeValue("double", 1.0);
	map.AddOrChangeValue("bool", true);
	map.AddOrChangeValue("string", std::string{ "a" });
	BOOST_REQUIRE(map.GetOptional<int>("int").value() == 1);
	BOOST_REQUIRE(map.GetOptional<double>("double").value() == 1.0);
	BOOST_REQUIRE(map.GetOptional<bool>("bool").value());
	BOOST_REQUIRE(map.GetOptional<std::string>("string").value() == "a");
}

BOOST_AUTO_TEST_CASE(test_variantconfigmap_to_boost_ptree)
{
	BOOST_TEST_MESSAGE("Running variantconfigmap_to_boost_ptree test");
	HsBa::Slicer::Config::VariantConfigMap<int, double, bool, std::string> map;
	map.AddOrChangeValue("int", 1);
	map.AddOrChangeValue("double", 1.0);
	map.AddOrChangeValue("bool", true);
	map.AddOrChangeValue("string", std::string{ "a" });
	auto ptree = map.ToPtree();
	BOOST_REQUIRE(ptree.get<int>("int") == 1);
	BOOST_REQUIRE(ptree.get<double>("double") == 1.0);
	BOOST_REQUIRE(ptree.get<bool>("bool"));
	BOOST_REQUIRE(ptree.get<std::string>("string") == "a");
}

BOOST_AUTO_TEST_CASE(test_add_int_double_bool_string_to_anyconfigmap)
{
	BOOST_TEST_MESSAGE("Running add_int_double_bool_string_to_anyconfigmap test");
	HsBa::Slicer::Config::AnyConfigMap map;
	map.AddOrChangeValue("int", 1);
	map.AddOrChangeValue("double", 1.0);
	map.AddOrChangeValue("bool", true);
	map.AddOrChangeValue("string", std::string{ "a" });
	BOOST_REQUIRE(map.GetOptional<int>("int").value() == 1);
	BOOST_REQUIRE(map.GetOptional<double>("double").value() == 1.0);
	BOOST_REQUIRE(map.GetOptional<bool>("bool").value());
	BOOST_REQUIRE(map.GetOptional<std::string>("string").value() == "a");
}

BOOST_AUTO_TEST_CASE(test_anyconfigmap_to_boost_ptree)
{
	BOOST_TEST_MESSAGE("Running anyconfigmap_to_boost_ptree test");
	HsBa::Slicer::Config::AnyConfigMap map;
	map.AddOrChangeValue("int", 1);
	map.AddOrChangeValue("double", 1.0);
	map.AddOrChangeValue("bool", true);
	map.AddOrChangeValue("string", std::string{ "a" });
	auto ptree = map.ToPtree<int, double, bool, std::string>();
	BOOST_REQUIRE(ptree.get<int>("int") == 1);
	BOOST_REQUIRE(ptree.get<double>("double") == 1.0);
	BOOST_REQUIRE(ptree.get<bool>("bool"));
	BOOST_REQUIRE(ptree.get<std::string>("string") == "a");
}

BOOST_AUTO_TEST_CASE(test_variantconfig_map_to_anyconfigmap)
{
	BOOST_TEST_MESSAGE("Running variantconfigmap_to_anyconfigmap test");
	HsBa::Slicer::Config::VariantConfigMap<int, double, bool, std::string> vmap;
	vmap.AddOrChangeValue("int", 1);
	vmap.AddOrChangeValue("double", 1.0);
	vmap.AddOrChangeValue("bool", true);
	vmap.AddOrChangeValue("string", std::string{ "a" });
	auto map = vmap.ToAnyMap();
	BOOST_REQUIRE(map.GetOptional<int>("int").value() == 1);
	BOOST_REQUIRE(map.GetOptional<double>("double").value() == 1.0);
	BOOST_REQUIRE(map.GetOptional<bool>("bool").value());
	BOOST_REQUIRE(map.GetOptional<std::string>("string").value() == "a");
}

BOOST_AUTO_TEST_CASE(test_anyconfig_map_to_variantconfigmap)
{
	BOOST_TEST_MESSAGE("Running anyconfigmap_to_variantconfigmap test");
	HsBa::Slicer::Config::AnyConfigMap any_map;
	any_map.AddOrChangeValue("int", 1);
	any_map.AddOrChangeValue("double", 1.0);
	any_map.AddOrChangeValue("bool", true);
	any_map.AddOrChangeValue("string", std::string{ "a" });
	auto map = any_map.ToVariantConfigMap<int, double, bool, std::string>();
	BOOST_REQUIRE(map.GetOptional<int>("int").value() == 1);
	BOOST_REQUIRE(map.GetOptional<double>("double").value() == 1.0);
	BOOST_REQUIRE(map.GetOptional<bool>("bool").value());
	BOOST_REQUIRE(map.GetOptional<std::string>("string").value() == "a");
}

BOOST_AUTO_TEST_CASE(test_variantconfig_from_ptree)
{
	BOOST_TEST_MESSAGE("Running variantconfigmap_from_ptree test");
	boost::property_tree::ptree ptree;
	ptree.add("int", 1);
	ptree.add("double", 1.0);
	ptree.add("bool", true);
	ptree.add("string", std::string{ "a" });
	HsBa::Slicer::Config::VariantConfigMap<int,double,bool,std::string> map;
	map.AddValueInPtree<int>(ptree, "int");
	map.AddValueInPtree<double>(ptree, "double");
	map.AddValueInPtree<bool>(ptree, "bool");
	map.AddValueInPtree<std::string>(ptree, "string");
	BOOST_REQUIRE(map.GetOptional<int>("int").value() == 1);
	BOOST_REQUIRE(map.GetOptional<double>("double").value() == 1.0);
	BOOST_REQUIRE(map.GetOptional<bool>("bool").value());
	BOOST_REQUIRE(map.GetOptional<std::string>("string").value() == "a");
	BOOST_REQUIRE(!map.GetOptional<double>("bool").has_value());
	BOOST_REQUIRE(!map.GetOptional<bool>("bool.b").has_value());
}

BOOST_AUTO_TEST_CASE(test_anyconfig_from_ptree)
{
	BOOST_TEST_MESSAGE("Running anyconfigmap_from_ptree test");
	boost::property_tree::ptree ptree;
	ptree.add("int", 1);
	ptree.add("double", 1.0);
	ptree.add("bool", true);
	ptree.add("string", std::string{ "a" });
	HsBa::Slicer::Config::AnyConfigMap map;
	map.AddValueInPtree<int>(ptree, "int");
	map.AddValueInPtree<double>(ptree, "double");
	map.AddValueInPtree<bool>(ptree, "bool");
	map.AddValueInPtree<std::string>(ptree, "string");
	BOOST_REQUIRE(map.GetOptional<int>("int").value() == 1);
	BOOST_REQUIRE(map.GetOptional<double>("double").value() == 1.0);
	BOOST_REQUIRE(map.GetOptional<bool>("bool").value());
	BOOST_REQUIRE(map.GetOptional<std::string>("string").value() == "a");
	BOOST_REQUIRE(!map.GetOptional<double>("bool").has_value());
	BOOST_REQUIRE(!map.GetOptional<bool>("bool.b").has_value());
}

BOOST_AUTO_TEST_CASE(test_eigenvector_toptree)
{
	BOOST_TEST_MESSAGE("Running eigenvector_toptree test");
	boost::property_tree::ptree ptree;
	ptree.add<Eigen::Vector2f>("vector2f", Eigen::Vector2f{ 0.12f,-1e6f }, HsBa::Slicer::Utils::EigenVector2fTranslator{});
	auto v2f = ptree.get<Eigen::Vector2f>("vector2f", Eigen::Vector2f{}, HsBa::Slicer::Utils::EigenVector2fTranslator{});
	BOOST_REQUIRE(v2f == Eigen::Vector2f( 0.12f,-1e6f ));
	ptree.add<Eigen::Vector2i>("vector2i", Eigen::Vector2i{ INT_MAX,0 }, HsBa::Slicer::Utils::EigenVector2iTranslator{});
	auto v2i = ptree.get<Eigen::Vector2i>("vector2i", Eigen::Vector2i{}, HsBa::Slicer::Utils::EigenVector2iTranslator{});
	BOOST_REQUIRE(v2i == Eigen::Vector2i(INT_MAX, 0));
}

BOOST_AUTO_TEST_CASE(test_prtee_add_variantconfigmap_eigenvector)
{
	BOOST_TEST_MESSAGE("Running prtee_add_variantconfigmap_eigenvector test");
	boost::property_tree::ptree ptree;
	ptree.add("vector3d", Eigen::Vector3d{ 0.0,0.0,0.0 }, HsBa::Slicer::Utils::EigenVector3dTranslator{});
	HsBa::Slicer::Config::VariantConfigMap<Eigen::Vector3d> map;
	map.AddValueInPtree<Eigen::Vector3d>(ptree, "vector3d", HsBa::Slicer::Utils::EigenVector3dTranslator{});
	BOOST_REQUIRE(map.GetOptional<Eigen::Vector3d>("vector3d").value() == Eigen::Vector3d(0.0, 0.0, 0.0));
}

BOOST_AUTO_TEST_CASE(test_prtee_add_anyconfigmap_eigenvector)
{
	BOOST_TEST_MESSAGE("Running prtee_add_anyconfigmap_eigenvector test");
	boost::property_tree::ptree ptree;
	ptree.add("vector3d", Eigen::Vector3d{ 0.0,0.0,0.0 }, HsBa::Slicer::Utils::EigenVector3dTranslator{});
	HsBa::Slicer::Config::AnyConfigMap map;
	map.AddValueInPtree<Eigen::Vector3d>(ptree, "vector3d", HsBa::Slicer::Utils::EigenVector3dTranslator{});
	BOOST_REQUIRE(map.GetOptional<Eigen::Vector3d>("vector3d").value() == Eigen::Vector3d(0.0, 0.0, 0.0));
}

BOOST_AUTO_TEST_CASE(test_configmap_eigenvector_toptree)
{
	BOOST_TEST_MESSAGE("Running configmap_eigenvector_toptree test");
	HsBa::Slicer::Config::VariantConfigMap<Eigen::Vector4i> map;
	map.AddOrChangeValue<Eigen::Vector4i>("v4i", Eigen::Vector4i(1,1,1,1));
	auto ptree = map.ToPtree();
	HsBa::Slicer::Config::ChangeTranslator<Eigen::Vector4i>(ptree, "v4i", HsBa::Slicer::Utils::EigenVector4iTranslator{});
	auto v4i = ptree.get<Eigen::Vector4i>("v4i", Eigen::Vector4i{}, HsBa::Slicer::Utils::EigenVector4iTranslator{});
	BOOST_REQUIRE(v4i == Eigen::Vector4i(1, 1, 1, 1));
}

BOOST_AUTO_TEST_SUITE_END()