#define BOOST_TEST_MODULE polygon_fill_test
#include <boost/test/included/unit_test.hpp>

#include <filesystem>

#undef Polygon
#include "2D/IntPolygon.hpp"
#include "2D/PolygonFill.hpp"

using namespace HsBa::Slicer;

BOOST_AUTO_TEST_CASE(line_and_zigzag_fill_basic)
{
    // simple square polygon
    PolygonD polyd;
    polyd.emplace_back(Point2{0, 0});
    polyd.emplace_back(Point2{10000, 0});
    polyd.emplace_back(Point2{10000, 10000});
    polyd.emplace_back(Point2{0, 10000});

    auto poly = Polygons{Integerization(polyd)};

    // LineFill: 10000×10000 正方形、间距 1000、角度 0 → 每条线均为 2 点线段且位于多边形内
    auto lines = LineFill(poly, 1000.0, 0.0, 200.0);
    auto linesd = UnIntegerization(lines);
    BOOST_CHECK_GE(lines.size(), 9u);
    BOOST_CHECK_LE(lines.size(), 11u);
    for (const auto& l : lines)
    {
        BOOST_CHECK_EQUAL(l.size(), 2u);
        // 水平填充线两端应落在正方形左右边界上（角度 0），同一线上 y 相同
        const int64_t xmin = std::min(l.front().x, l.back().x);
        const int64_t xmax = std::max(l.front().x, l.back().x);
        BOOST_CHECK_EQUAL(xmin, 0);
        BOOST_CHECK_EQUAL(xmax, 10000);
        for (const auto& pt : l)
        {
            BOOST_CHECK_EQUAL(pt.y, l.front().y);
        }
    }

    // ZigzagFill: ensure produced pieces lie inside the original polygon (no exterior paths)
    auto simplezig = SimpleZigzagFill(poly, 1000.0, 0.0, 200.0);
    BOOST_CHECK(!simplezig.empty());
    for (const auto& pz : simplezig)
    {
        // each path's vertices should be inside or on the boundary of the original polygon
        for (const auto& pt : pz)
        {
            Clipper2Lib::Point64 pv{pt.x, pt.y};
            auto res = PointInPolygons(pv, poly);
            BOOST_CHECK(res != Clipper2Lib::PointInPolygonResult::IsOutside);
        }
    }

    auto zig = ZigzagFill(poly, 1000.0, 0.0, 200.0);
    BOOST_CHECK(!zig.empty());
    for (const auto& pz : zig)
    {
        // each path's vertices should be inside or on the boundary of the original polygon
        for (const auto& pt : pz)
        {
            Clipper2Lib::Point64 pv{pt.x, pt.y};
            auto res = PointInPolygons(pv, poly);
            BOOST_CHECK(res != Clipper2Lib::PointInPolygonResult::IsOutside);
        }
    }
}

BOOST_AUTO_TEST_CASE(normalize_self_intersecting_polygon)
{
    HsBa::Slicer::Polygon poly;
    poly.emplace_back(HsBa::Slicer::Point2{0, 0});
    poly.emplace_back(HsBa::Slicer::Point2{10000, 10000});
    poly.emplace_back(HsBa::Slicer::Point2{0, 10000});
    poly.emplace_back(HsBa::Slicer::Point2{10000, 0});

    auto simple_polys = HsBa::Slicer::NormalizeToSimplePolygons(poly);
    // 自交领结应拆分为两个三角形
    BOOST_REQUIRE_EQUAL(simple_polys.size(), 2u);
    double totalArea = 0.0;
    for (const auto& simple_poly : simple_polys)
    {
        BOOST_CHECK_EQUAL(simple_poly.size(), 3u);
        totalArea += std::abs(Area(simple_poly));
    }
    // 自交点在中心 (5000,5000)，拆出的两个三角形底边 10000、高 5000，
    // 各 0.5×10000×5000 = 2.5e7，总和 5e7
    BOOST_CHECK_CLOSE(totalArea, 5.0e7, 1e-3);
}

BOOST_AUTO_TEST_CASE(composite_and_lua_custom)
{
    // base polygon
    PolygonD polyd;
    polyd.emplace_back(Point2{0, 0});
    polyd.emplace_back(Point2{10000, 0});
    polyd.emplace_back(Point2{10000, 10000});
    polyd.emplace_back(Point2{0, 10000});

    auto poly = Polygons{Integerization(polyd)};

    // CompositeOffsetFill: 2 outward, 2 inward using Line mode
    auto comp = CompositeOffsetFill(poly, 1000.0, 500.0, 2, 2, FillMode::Line, 45.0, 150.0);
    BOOST_CHECK(!comp.empty());
    // Expect at least one returned path to be a 2-point line
    bool anyLine = false;
    for (const auto& p : comp)
        if (p.size() == 2)
        {
            anyLine = true;
            break;
        }
    BOOST_CHECK(anyLine);

    auto hybrid = HybridFill(poly, 1000.0, 500.0, 2, 2, FillMode::Zigzag, 45.0, 150.0);
    BOOST_CHECK(!hybrid.empty());
    // 每条路径至少 2 个点
    for (const auto& p : hybrid)
    {
        BOOST_CHECK_GE(p.size(), 2u);
    }

    // LuaCustomFill: 脚本固定返回两条对角线，端点坐标精确可验
    std::filesystem::path script_path = std::filesystem::path(__FILE__).parent_path() / "custom_fill.lua";
    std::string script = script_path.string();
    auto luares = LuaCustomFill(poly, script, "generate_fill", 100.0);
    BOOST_REQUIRE_EQUAL(luares.size(), 2u);
    for (const auto& p : luares)
    {
        BOOST_CHECK_EQUAL(p.size(), 2u);
    }
    // 第一条对角线：(1000,1000) → (9000,9000)（整数化坐标）
    BOOST_CHECK_EQUAL(luares[0].front().x, 1000 * integerization);
    BOOST_CHECK_EQUAL(luares[0].front().y, 1000 * integerization);
    BOOST_CHECK_EQUAL(luares[0].back().x, 9000 * integerization);
    BOOST_CHECK_EQUAL(luares[0].back().y, 9000 * integerization);
    const char* luaSrc = R"(
local w = 10000
local margin = 1000
function customFill(poly, thickness)
    return {
        { { x = margin, y = margin }, { x = w - margin, y = w - margin } },
        { { x = margin, y = w - margin }, { x = w - margin, y = margin } }
    }
end
)";
    luares = LuaCustomFillString(poly, luaSrc, "customFill", 0.1);
    BOOST_REQUIRE_EQUAL(luares.size(), 2u);
    for (const auto& p : luares)
    {
        BOOST_CHECK_EQUAL(p.size(), 2u);
    }
    // 第二条对角线：(1000,9000) → (9000,1000)（整数化坐标）
    BOOST_CHECK_EQUAL(luares[1].front().x, 1000 * integerization);
    BOOST_CHECK_EQUAL(luares[1].front().y, 9000 * integerization);
    BOOST_CHECK_EQUAL(luares[1].back().x, 9000 * integerization);
    BOOST_CHECK_EQUAL(luares[1].back().y, 1000 * integerization);
}
