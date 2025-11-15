#define BOOST_TEST_MODULE CgalModelTests
#include <boost/test/included/unit_test.hpp>

#include "meshmodel/CgalModel.hpp"

using namespace HsBa::Slicer;

BOOST_AUTO_TEST_CASE(create_box_and_trianglemesh)
{
    auto box = CgalModel::CreateBox(Eigen::Vector3f{1.0f,1.0f,1.0f});
    auto [v,f] = box.TriangleMesh();
    BOOST_CHECK(v.rows() > 0);
    BOOST_CHECK(f.rows() > 0);
    float vol = box.Volume();
    BOOST_CHECK(vol > 0.0f);
}

BOOST_AUTO_TEST_CASE(create_torus_and_properties)
{
    auto t = CgalModel::CreateTorus(1.0f, 0.25f, 16, 8);
    auto [v,f] = t.TriangleMesh();
    BOOST_CHECK(v.rows() > 0);
    BOOST_CHECK(f.rows() > 0);
}

// Boolean operations for CGAL guarded by compile-time macro to allow skipping in Debug.
#ifndef DISABLE_BOOLEAN_OPERATIONS_TESTS
BOOST_AUTO_TEST_CASE(boolean_operations)
{
    std::cout << "Starting CGAL boolean operations test..." << std::endl;
    auto a = CgalModel::CreateBox(Eigen::Vector3f{1.0f,1.0f,1.0f});
    auto b = CgalModel::CreateBox(Eigen::Vector3f{1.0f,1.0f,1.0f});
    b.Translate(Eigen::Vector3f{0.4f,0.0f,0.0f});
    auto u = Union(a, b);
    //auto inter = Intersection(a, b);
    //auto diff = Difference(a, b);
    //auto xr = Xor(a, b);
    BOOST_CHECK(u.Volume() > 0.0f);
    //BOOST_CHECK(inter.Volume() > 0.0f);
    //BOOST_CHECK(diff.Volume() >= 0.0f);
    //BOOST_CHECK(xr.Volume() > 0.0f);
    auto [uv, uf] = u.TriangleMesh();
    //auto [iv, ifa] = inter.TriangleMesh();
    BOOST_CHECK(uv.rows() > 0);
    //BOOST_CHECK(iv.rows() >= 0);
}
#endif

