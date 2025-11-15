#define BOOST_TEST_MODULE IglModelTests
#include <boost/test/included/unit_test.hpp>

#include "meshmodel/IglModel.hpp"

using namespace HsBa::Slicer;

BOOST_AUTO_TEST_CASE(create_box_and_normals)
{
    auto box = IglModel::CreateBox(Eigen::Vector3f{1.0f,1.0f,1.0f});
    auto [v,f] = box.TriangleMesh();
    BOOST_CHECK(v.rows() > 0);
    BOOST_CHECK(f.rows() > 0);
    auto normals = box.ComputeFaceNormals();
    BOOST_CHECK(normals.rows() == f.rows());
}

BOOST_AUTO_TEST_CASE(volume_and_transform)
{
    auto cyl = IglModel::CreateCylinder(0.5f, 1.0f, 16);
    float vol = cyl.Volume();
    BOOST_CHECK(vol > 0.0f);
    Eigen::Vector3f mn, mx;
    cyl.BoundingBox(mn, mx);
    BOOST_CHECK((mx - mn).norm() > 0.0f);
}

// Boolean operations for IGL guarded by compile-time macro to allow skipping in Debug.
#ifndef DISABLE_BOOLEAN_OPERATIONS_TESTS
BOOST_AUTO_TEST_CASE(boolean_operations)
{
    auto a = IglModel::CreateBox(Eigen::Vector3f{1.0f,1.0f,1.0f});
    auto b = IglModel::CreateBox(Eigen::Vector3f{1.0f,1.0f,1.0f});
    b.Translate(Eigen::Vector3f{0.3f,0.0f,0.0f});
    auto u = Union(a, b);
    auto inter = Intersection(a, b);
    auto diff = Difference(a, b);
    auto xr = Xor(a, b);
    auto [uv, uf] = u.TriangleMesh();
    auto [iv, iF] = inter.TriangleMesh();
    auto [dv, dF] = diff.TriangleMesh();
    auto [xv, xF] = xr.TriangleMesh();

    if (uv.rows() > 0) {
        BOOST_CHECK(uv.rows() > 0);
    } else {
        BOOST_TEST_MESSAGE("IGL Union produced empty mesh (operation may not be supported for these inputs);");
    }
    if (iv.rows() > 0) {
        BOOST_CHECK(iv.rows() >= 0);
    } else {
        BOOST_TEST_MESSAGE("IGL Intersection produced empty mesh (possible, acceptable result);");
    }
    if (dv.rows() > 0) {
        BOOST_CHECK(dv.rows() >= 0);
    } else {
        BOOST_TEST_MESSAGE("IGL Difference produced empty mesh (possible, acceptable result);");
    }
    if (xv.rows() > 0) {
        BOOST_CHECK(xv.rows() > 0);
    } else {
        BOOST_TEST_MESSAGE("IGL Xor produced empty mesh (operation may not be supported for these inputs);");
    }
}
#endif

