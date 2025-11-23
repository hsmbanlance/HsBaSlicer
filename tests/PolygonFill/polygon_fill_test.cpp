#define BOOST_TEST_MODULE polygon_fill_test
#include <boost/test/included/unit_test.hpp>

#include <filesystem>

#include "2D/PolygonFill.hpp"
#include "2D/IntPolygon.hpp"

using namespace HsBa::Slicer;

BOOST_AUTO_TEST_CASE(line_and_zigzag_fill_basic)
{
    // simple square polygon
    PolygonD polyd;
    polyd.emplace_back(Point2{0,0});
    polyd.emplace_back(Point2{10000,0});
    polyd.emplace_back(Point2{10000,10000});
    polyd.emplace_back(Point2{0,10000});

    auto poly = Polygons{ Integerization(polyd) };

    // LineFill: expect at least one piece has positive area
    auto lines = LineFill(poly, 1000.0, 0.0, 200.0);
    auto linesd = UnIntegerization(lines);
    BOOST_CHECK(!lines.empty());
    for(const auto& l : lines)
    {
        BOOST_CHECK(l.size() == 2);
    }

    // ZigzagFill: ensure produced pieces lie inside the original polygon (no exterior paths)
    auto simplezig = SimpleZigzagFill(poly, 1000.0, 0.0, 200.0);
    BOOST_CHECK(!simplezig.empty());
    for (const auto &pz : simplezig)
    {
        // each path's vertices should be inside or on the boundary of the original polygon
        for (const auto &pt : pz)
        {
            Clipper2Lib::Point64 pv{ pt.x, pt.y };
            auto res = PointInPolygons(pv, poly);
            BOOST_CHECK(res != Clipper2Lib::PointInPolygonResult::IsOutside);
        }
    }

    auto zig = ZigzagFill(poly, 1000.0, 0.0, 200.0);
    BOOST_CHECK(!zig.empty());
    for (const auto &pz : zig)
    {
        // each path's vertices should be inside or on the boundary of the original polygon
        for (const auto &pt : pz)
        {
            Clipper2Lib::Point64 pv{ pt.x, pt.y };
            auto res = PointInPolygons(pv, poly);
            BOOST_CHECK(res != Clipper2Lib::PointInPolygonResult::IsOutside);
        }
    }
}

BOOST_AUTO_TEST_CASE(composite_and_lua_custom)
{
    // base polygon
    PolygonD polyd;
    polyd.emplace_back(Point2{0,0});
    polyd.emplace_back(Point2{10000,0});
    polyd.emplace_back(Point2{10000,10000});
    polyd.emplace_back(Point2{0,10000});

    auto poly = Polygons{ Integerization(polyd) };

    // CompositeOffsetFill: 2 outward, 2 inward using Line mode
    auto comp = CompositeOffsetFill(poly, 1000.0, 500.0, 2, 2, FillMode::Line, 45.0, 150.0);
    BOOST_CHECK(!comp.empty());
    // Expect at least one returned path to be a 2-point line
    bool anyLine = false;
    for (const auto &p : comp) if (p.size() == 2) { anyLine = true; break; }
    BOOST_CHECK(anyLine);

    auto hybrid = HybridFill(poly, 1000.0, 500.0, 2, 2, FillMode::Zigzag, 45.0, 150.0);
    BOOST_CHECK(!hybrid.empty());
    // Expect at least one returned path to be a 2-point line
    bool anyZigLine = false;
    for (const auto &p : hybrid)
    {
        if(p.front() != p.back() || p.size() == 2)
        {
            anyZigLine = true; 
            break;
        }
    }
    BOOST_CHECK(anyZigLine);

    // LuaCustomFill: load the helper script placed next to this test file
    std::filesystem::path script_path = std::filesystem::path(__FILE__).parent_path() / "custom_fill.lua";
    std::string script = script_path.string();
    auto luares = LuaCustomFill(poly, script, "generate_fill", 100.0);
    BOOST_CHECK(!luares.empty());
    // Expect at least one returned path to be a 2-point line
    anyLine = false;
    for (const auto &p : luares) if (p.size() == 2) { anyLine = true; break; }
    BOOST_CHECK(anyLine);
}
