#define BOOST_TEST_MODULE CgalModelTests
#include <boost/test/included/unit_test.hpp>

#include <cmath>
#include <numbers>

#include "meshmodel/CgalModel.hpp"

using namespace HsBa::Slicer;

BOOST_AUTO_TEST_CASE(create_box_and_trianglemesh)
{
    auto box = CgalModel::CreateBox(Eigen::Vector3f{1.0f, 1.0f, 1.0f});
    auto [v, f] = box.TriangleMesh();
    // 单位立方体：8 个顶点，6 个面三角化后 12 个三角形
    BOOST_CHECK_EQUAL(v.rows(), 8);
    BOOST_CHECK_EQUAL(f.rows(), 12);
    BOOST_CHECK_CLOSE(box.Volume(), 1.0f, 1e-3);
    Eigen::Vector3f mn, mx;
    box.BoundingBox(mn, mx);
    BOOST_CHECK_CLOSE(mn.x(), -0.5f, 1e-3);
    BOOST_CHECK_CLOSE(mn.y(), -0.5f, 1e-3);
    BOOST_CHECK_CLOSE(mn.z(), -0.5f, 1e-3);
    BOOST_CHECK_CLOSE(mx.x(), 0.5f, 1e-3);
    BOOST_CHECK_CLOSE(mx.y(), 0.5f, 1e-3);
    BOOST_CHECK_CLOSE(mx.z(), 0.5f, 1e-3);
}

BOOST_AUTO_TEST_CASE(create_torus_and_properties)
{
    auto t = CgalModel::CreateTorus(1.0f, 0.25f, 16, 8);
    auto [v, f] = t.TriangleMesh();
    BOOST_CHECK(v.rows() > 0);
    BOOST_CHECK(f.rows() > 0);
    // 理论体积 2π²Rr² ≈ 1.2337，离散网格略小，允许 25% 容差
    const float expectedVolume = 2.0f * std::numbers::pi_v<float> * std::numbers::pi_v<float> * 1.0f * 0.25f * 0.25f;
    BOOST_CHECK_CLOSE(t.Volume(), expectedVolume, 25.0);
}

// Boolean operations for CGAL guarded by compile-time macro to allow skipping in Debug.
#ifndef DISABLE_BOOLEAN_OPERATIONS_TESTS
BOOST_AUTO_TEST_CASE(boolean_operations)
{
    std::cout << "Starting CGAL boolean operations test..." << std::endl;
    auto a = CgalModel::CreateBox(Eigen::Vector3f{1.0f, 1.0f, 1.0f});
    auto b = CgalModel::CreateBox(Eigen::Vector3f{1.0f, 1.0f, 1.0f});
    b.Translate(Eigen::Vector3f{0.4f, 0.0f, 0.0f});
    // 两个单位立方体 x 方向重叠 0.6：并集 1.4、交集 0.6、差集 0.4、异或 0.8
    auto u = Union(a, b);
    auto inter = Intersection(a, b);
    auto diff = Difference(a, b);
    auto xr = Xor(a, b);
    BOOST_CHECK_CLOSE(u.Volume(), 1.4f, 1e-2);
    BOOST_CHECK_CLOSE(inter.Volume(), 0.6f, 1e-2);
    BOOST_CHECK_CLOSE(diff.Volume(), 0.4f, 1e-2);
    BOOST_CHECK_CLOSE(xr.Volume(), 0.8f, 1e-2);
    auto [uv, uf] = u.TriangleMesh();
    BOOST_CHECK(uv.rows() > 0);
    BOOST_CHECK(uf.rows() > 0);
    BOOST_CHECK(uv.rows() >= 8);  // 并集体至少 8 个顶点
}
#endif  // !DISABLE_BOOLEAN_OPERATIONS_TESTS

#ifndef DISABLE_ADVANCE_OPERATIONS_TESTS
BOOST_AUTO_TEST_CASE(create_prime_single_polygon)
{
    // Create a simple triangle polygon
    PolygonD triangle;
    triangle.push_back({0.0, 0.0});
    triangle.push_back({1.0, 0.0});
    triangle.push_back({0.5, 1.0});

    try
    {
        // Extrude along Z axis with height 2.0
        auto prism = CgalModel::CreatePrime(triangle, Eigen::Vector3f{0.0f, 0.0f, 2.0f});

        // Check mesh is valid and closed
        auto [v, f] = prism.TriangleMesh();
        BOOST_CHECK(v.rows() > 0);
        BOOST_CHECK(f.rows() > 0);

        // 三角形底面积 0.5 × 高度 2.0 = 体积 1.0（CGAL 精确，取绝对值容忍面朝向差异）
        BOOST_CHECK_CLOSE(std::abs(prism.Volume()), 1.0f, 1e-2);

        // Verify bounding box
        Eigen::Vector3f mn, mx;
        prism.BoundingBox(mn, mx);
        BOOST_CHECK_CLOSE(mn.x(), 0.0f, 1e-2);
        BOOST_CHECK_CLOSE(mn.y(), 0.0f, 1e-2);
        BOOST_CHECK_CLOSE(mn.z(), 0.0f, 1e-2);
        BOOST_CHECK_CLOSE(mx.x(), 1.0f, 1e-2);
        BOOST_CHECK_CLOSE(mx.y(), 1.0f, 1e-2);
        BOOST_CHECK_CLOSE(mx.z(), 2.0f, 1e-2);
    }
    catch (const std::exception& e)
    {
        std::cout << "Exception caught: " << e.what() << std::endl;
        BOOST_FAIL("CreatePrime failed for simple triangle");
    }
}


// use cgal
BOOST_AUTO_TEST_CASE(create_prime_polygon_with_hole)
{
    // Outer square
    PolygonD outer;
    outer.push_back({0.0, 0.0});
    outer.push_back({2.0, 0.0});
    outer.push_back({2.0, 2.0});
    outer.push_back({0.0, 2.0});

    // Inner hole (square)
    PolygonD hole;
    hole.push_back({0.5, 0.5});
    hole.push_back({1.5, 0.5});
    hole.push_back({1.5, 1.5});
    hole.push_back({0.5, 1.5});

    PolygonsD paths;
    paths.push_back(outer);
    paths.push_back(hole);

    try
    {
        // Extrude with height 1.5
        auto prism = CgalModel::CreatePrime(paths, Eigen::Vector3f{0.0f, 0.0f, 1.5f});

        // Check mesh validity
        auto [v, f] = prism.TriangleMesh();
        BOOST_CHECK(v.rows() > 0);
        BOOST_CHECK(f.rows() > 0);

        // 外方 2×2 减去洞 1×1，底面积 3.0 × 高度 1.5 = 体积 4.5
        BOOST_CHECK_CLOSE(std::abs(prism.Volume()), 4.5f, 1e-2);

        Eigen::Vector3f mn, mx;
        prism.BoundingBox(mn, mx);
        BOOST_CHECK_CLOSE(mx.x() - mn.x(), 2.0f, 1e-2);
        BOOST_CHECK_CLOSE(mx.y() - mn.y(), 2.0f, 1e-2);
        BOOST_CHECK_CLOSE(mx.z() - mn.z(), 1.5f, 1e-2);
    }
    catch (const std::exception& e)
    {
        std::cout << "Exception in create_prime_polygon_with_hole: " << e.what() << std::endl;
        BOOST_FAIL("CreatePrime failed for polygon with hole");
    }
}

BOOST_AUTO_TEST_CASE(create_prime_complex_polygon)
{
    // Create an L-shaped polygon
    PolygonD l_shape;
    l_shape.push_back({0.0, 0.0});
    l_shape.push_back({2.0, 0.0});
    l_shape.push_back({2.0, 1.0});
    l_shape.push_back({1.0, 1.0});
    l_shape.push_back({1.0, 2.0});
    l_shape.push_back({0.0, 2.0});

    // Extrude with height 3.0
    auto prism = CgalModel::CreatePrime(l_shape, Eigen::Vector3f{0.0f, 0.0f, 3.0f});

    // Check mesh properties
    auto [v, f] = prism.TriangleMesh();
    BOOST_CHECK(v.rows() > 0);
    BOOST_CHECK(f.rows() > 0);

    // L 形面积 = 2*1 + 1*1 = 3.0，体积 = 3.0 * 3.0 = 9.0（CGAL 精确）
    BOOST_CHECK_CLOSE(std::abs(prism.Volume()), 9.0f, 1e-2);

    Eigen::Vector3f mn, mx;
    prism.BoundingBox(mn, mx);
    BOOST_CHECK_CLOSE(mx.x() - mn.x(), 2.0f, 1e-2);
    BOOST_CHECK_CLOSE(mx.y() - mn.y(), 2.0f, 1e-2);
    BOOST_CHECK_CLOSE(mx.z() - mn.z(), 3.0f, 1e-2);
}
#endif  // !DISABLE_ADVANCE_OPERATIONS_TESTS