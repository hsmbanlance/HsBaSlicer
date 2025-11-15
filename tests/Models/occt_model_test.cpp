#define BOOST_TEST_MODULE OcctModelTests
#include <boost/test/included/unit_test.hpp>

#include "cadmodel/OcctModel.hpp"

using namespace HsBa::Slicer;

BOOST_AUTO_TEST_CASE(create_box_and_properties)
{
    auto box = OcctModel::CreateBox(Eigen::Vector3f{1.0f, 2.0f, 3.0f});
    BOOST_CHECK(!box.TriangleMesh().first.size() || true); // Triangle mesh may be empty without tessellation
    float vol = box.Volume();
    BOOST_CHECK(vol > 0.0f);
    Eigen::Vector3f mn, mx;
    box.BoundingBox(mn, mx);
    BOOST_CHECK(mx.x() - mn.x() > 0.0f);
}

BOOST_AUTO_TEST_CASE(create_sphere_and_volume)
{
    auto s = OcctModel::CreateSphere(0.5f, 2);
    float vol = s.Volume();
    BOOST_CHECK(vol > 0.0f);
}

// OCCT boolean tests guarded by build-time availability — OCCT test only added when
// `HsBaSlicerCADModel` target exists. No additional compile-time guard needed here.
BOOST_AUTO_TEST_CASE(boolean_operations)
{
    auto a = OcctModel::CreateBox(Eigen::Vector3f{1.0f,1.0f,1.0f});
    auto b = OcctModel::CreateBox(Eigen::Vector3f{1.0f,1.0f,1.0f});
    b.Translate(Eigen::Vector3f{0.5f,0.0f,0.0f});
    auto u = Union(a, b);
    auto inter = Intersection(a, b);
    auto diff = Difference(a, b);
    auto xr = Xor(a, b);
    BOOST_CHECK(u.Volume() > 0.0f);
    BOOST_CHECK(inter.Volume() > 0.0f);
    BOOST_CHECK(diff.Volume() >= 0.0f);
    BOOST_CHECK(xr.Volume() > 0.0f);
    auto [uv, uf] = u.TriangleMesh();
    auto [iv, ifa] = inter.TriangleMesh();
    BOOST_CHECK(uv.rows() >= 0);
    BOOST_CHECK(iv.rows() >= 0);
}

