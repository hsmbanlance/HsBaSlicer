#define BOOST_TEST_MODULE OcctModelTests
#include <boost/test/included/unit_test.hpp>

#include "cadmodel/OcctModel.hpp"

using namespace HsBa::Slicer;

BOOST_AUTO_TEST_CASE(create_box_and_properties)
{
    auto box = OcctModel::CreateBox(Eigen::Vector3f{1.0f, 2.0f, 3.0f});
    // OCCT 实体体积精确为 1×2×3 = 6.0
    BOOST_CHECK_CLOSE(box.Volume(), 6.0f, 1e-3);
    Eigen::Vector3f mn, mx;
    box.BoundingBox(mn, mx);
    // CreateBox 以原点为角点，包围盒尺寸精确为 (1, 2, 3)
    BOOST_CHECK_SMALL(mn.x(), 1e-6f);
    BOOST_CHECK_SMALL(mn.y(), 1e-6f);
    BOOST_CHECK_SMALL(mn.z(), 1e-6f);
    BOOST_CHECK_CLOSE(mx.x(), 1.0f, 1e-3);
    BOOST_CHECK_CLOSE(mx.y(), 2.0f, 1e-3);
    BOOST_CHECK_CLOSE(mx.z(), 3.0f, 1e-3);
}

BOOST_AUTO_TEST_CASE(create_sphere_and_volume)
{
    auto s = OcctModel::CreateSphere(0.5f, 2);
    // OCCT 球体体积精确为 4/3·π·r³
    const float expectedVolume = 4.0f / 3.0f * 3.14159265358979f * 0.5f * 0.5f * 0.5f;
    BOOST_CHECK_CLOSE(s.Volume(), expectedVolume, 1e-2);
}

// OCCT boolean tests guarded by build-time availability — OCCT test only added when
// `HsBaSlicerCADModel` target exists. No additional compile-time guard needed here.
BOOST_AUTO_TEST_CASE(boolean_operations)
{
    auto a = OcctModel::CreateBox(Eigen::Vector3f{1.0f, 1.0f, 1.0f});
    auto b = OcctModel::CreateBox(Eigen::Vector3f{1.0f, 1.0f, 1.0f});
    b.Translate(Eigen::Vector3f{0.5f, 0.0f, 0.0f});
    // 两个单位立方体 x 方向重叠 0.5：并集 1.5、交集 0.5、差集 0.5、异或 1.0
    auto u = Union(a, b);
    auto inter = Intersection(a, b);
    auto diff = Difference(a, b);
    auto xr = Xor(a, b);
    BOOST_CHECK_CLOSE(u.Volume(), 1.5f, 1e-2);
    BOOST_CHECK_CLOSE(inter.Volume(), 0.5f, 1e-2);
    BOOST_CHECK_CLOSE(diff.Volume(), 0.5f, 1e-2);
    BOOST_CHECK_CLOSE(xr.Volume(), 1.0f, 1e-2);
}

BOOST_AUTO_TEST_CASE(create_prime_single_polygon)
{
    // Create a simple triangle polygon
    PolygonD triangle;
    triangle.push_back({0.0, 0.0});
    triangle.push_back({1.0, 0.0});
    triangle.push_back({0.5, 1.0});

    // Extrude along Z axis with height 2.0
    auto prism = OcctModel::CreatePrime(triangle, Eigen::Vector3f{0.0f, 0.0f, 2.0f});

    // Check volume: triangle area = 0.5, height = 2.0, volume = 1.0
    float vol = prism.Volume();
    BOOST_CHECK_CLOSE(vol, 1.0f, 0.01);  // Allow 1% tolerance

    // Check bounding box
    Eigen::Vector3f mn, mx;
    prism.BoundingBox(mn, mx);
    BOOST_CHECK_CLOSE(mn.z(), 0.0f, 0.01);
    BOOST_CHECK_CLOSE(mx.z(), 2.0f, 0.01);

    // Note: TriangleMesh() requires tessellation which is not automatically performed
    // for CAD primitives. Volume and BoundingBox are the primary validation methods.
}

BOOST_AUTO_TEST_CASE(create_prime_polygon_with_hole)
{
    // Outer square
    PolygonD outer;
    outer.push_back({0.0, 0.0});
    outer.push_back({2.0, 0.0});
    outer.push_back({2.0, 2.0});
    outer.push_back({0.0, 2.0});

    // Inner hole (square) - NOTE: For OCCT, holes should have opposite orientation
    PolygonD hole;
    hole.push_back({0.5, 0.5});
    hole.push_back({0.5, 1.5});  // Reversed order for proper hole detection
    hole.push_back({1.5, 1.5});
    hole.push_back({1.5, 0.5});

    PolygonsD paths;
    paths.push_back(outer);
    paths.push_back(hole);

    // Extrude with height 1.5
    auto prism = OcctModel::CreatePrime(paths, Eigen::Vector3f{0.0f, 0.0f, 1.5f});

    // 外方 2×2 减去洞 1×1，底面积 3.0 × 高度 1.5 = 体积精确 4.5
    BOOST_CHECK_CLOSE(prism.Volume(), 4.5f, 1e-2);

    Eigen::Vector3f mn, mx;
    prism.BoundingBox(mn, mx);
    BOOST_CHECK_CLOSE(mx.x() - mn.x(), 2.0f, 1e-2);
    BOOST_CHECK_CLOSE(mx.y() - mn.y(), 2.0f, 1e-2);
    BOOST_CHECK_CLOSE(mx.z() - mn.z(), 1.5f, 1e-2);

    // Note: TriangleMesh() requires explicit tessellation for CAD models
}

BOOST_AUTO_TEST_CASE(create_prime_non_planar_direction)
{
    // Create a rectangle
    PolygonD rect;
    rect.push_back({0.0, 0.0});
    rect.push_back({1.0, 0.0});
    rect.push_back({1.0, 1.0});
    rect.push_back({0.0, 1.0});

    // Extrude in diagonal direction
    auto prism = OcctModel::CreatePrime(rect, Eigen::Vector3f{1.0f, 1.0f, 1.0f});

    // 斜拉伸体积 = 底面积 × z 方向分量 = 1.0
    BOOST_CHECK_CLOSE(prism.Volume(), 1.0f, 1e-2);

    // 包围盒：底面 [0,1]² 沿 (1,1,1) 拉伸，范围精确为 [0,2]×[0,2]×[0,1]
    Eigen::Vector3f mn, mx;
    prism.BoundingBox(mn, mx);
    BOOST_CHECK_SMALL(mn.x(), 1e-6f);
    BOOST_CHECK_SMALL(mn.y(), 1e-6f);
    BOOST_CHECK_SMALL(mn.z(), 1e-6f);
    BOOST_CHECK_CLOSE(mx.x(), 2.0f, 1e-2);
    BOOST_CHECK_CLOSE(mx.y(), 2.0f, 1e-2);
    BOOST_CHECK_CLOSE(mx.z(), 1.0f, 1e-2);
}
