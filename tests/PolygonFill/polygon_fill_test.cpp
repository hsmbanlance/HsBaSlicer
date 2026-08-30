#define BOOST_TEST_MODULE polygon_fill_test
#include <boost/test/included/unit_test.hpp>

#include <filesystem>
#include <fstream>

#undef Polygon
#include "2D/IntPolygon.hpp"
#include "2D/PolygonFill.hpp"
#include "LibHsBaSlicer/Fill/polygon_fill.hpp"
#include "LibHsBaSlicer/Path/path_optimizer.hpp"

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

// ---------------------------------------------------------------------------
// 路径输出前置优化：独立多边形区域作为图顶点（Lua 脚本嵌入方案）
// ---------------------------------------------------------------------------
namespace
{

// 生成 count 条水平线段（区域填充路径），首点从左到右
class RegionHelper
{
public:
    static PolygonsD MakeLines(double x0, double y0, double width, int count, double spacing)
    {
        PolygonsD lines;
        for (int i = 0; i < count; ++i)
        {
            PolygonD p;
            p.push_back(Point2D{x0, y0 + i * spacing});
            p.push_back(Point2D{x0 + width, y0 + i * spacing});
            lines.push_back(std::move(p));
        }
        return lines;
    }

    // 路径所属区域频带（按 y 坐标划分，每区域 y 基址相隔 1000）
    static int BandOf(const PolygonD& p, int regionCount)
    {
        if (p.empty())
            return -1;
        int band = static_cast<int>(p.front().y / 1000.0);
        return (band >= 0 && band < regionCount) ? band : -1;
    }

    // 生成逆时针矩形轮廓（多边形模式的区域轮廓）
    static PolygonD MakeRect(double x0, double y0, double w, double h)
    {
        PolygonD p;
        p.push_back(Point2D{x0, y0});
        p.push_back(Point2D{x0 + w, y0});
        p.push_back(Point2D{x0 + w, y0 + h});
        p.push_back(Point2D{x0, y0 + h});
        return p;
    }

    // 校验输出路径按区域连续分块（区域不交错），返回块数
    static int CountContiguousBlocks(const PolygonsD& paths, int regionCount)
    {
        int blocks = 0;
        int prevBand = -1;
        for (const auto& p : paths)
        {
            int band = BandOf(p, regionCount);
            if (band < 0)
                return -1;
            if (band != prevBand)
            {
                ++blocks;
                prevBand = band;
            }
        }
        return blocks;
    }
};

}  // namespace

BOOST_AUTO_TEST_SUITE(PathOptimizerTests)

BOOST_AUTO_TEST_CASE(optimizer_cluster_order)
{
    // 区域1/2 聚簇（x≈0 与 x≈12），区域3 在远处（x≈1000）；乱序添加 1,3,2；
    // 各区域 y 频带相隔 1000，便于按 BandOf 识别输出路径归属
    RegionPathOptimizer opt;
    opt.addRegion(1, RegionHelper::MakeLines(0.0, 0.0, 10.0, 3, 2.0));
    opt.addRegion(3, RegionHelper::MakeLines(1000.0, 2000.0, 10.0, 3, 2.0));
    opt.addRegion(2, RegionHelper::MakeLines(12.0, 1000.0, 10.0, 3, 2.0));

    auto order = opt.optimizeOrder();
    BOOST_REQUIRE_EQUAL(order.size(), 3u);
    std::vector<int> sorted = order;
    std::sort(sorted.begin(), sorted.end());
    const std::vector<int> expectedIds{1, 2, 3};
    BOOST_CHECK(sorted == expectedIds);

    // 聚簇区域 1、2 在环游中应相邻（含首尾相邻）
    auto pos = [&](int id)
    {
        return static_cast<int>(std::find(order.begin(), order.end(), id) - order.begin());
    };
    int d = std::abs(pos(1) - pos(2));
    BOOST_CHECK(d == 1 || d == static_cast<int>(order.size()) - 1);

    // 输出完整填充路径：总数不变，区域连续分块（3 块）
    auto paths = opt.buildPaths();
    BOOST_CHECK_EQUAL(paths.size(), 9u);
    BOOST_CHECK_EQUAL(RegionHelper::CountContiguousBlocks(paths, 3), 3);
}

BOOST_AUTO_TEST_CASE(optimizer_single_region_and_path_reversal)
{
    RegionPathOptimizer opt;
    // 路径A：(0,0)->(10,0)；路径B：(20,5)->(10,5)（尾点离 A 终点更近，应被反转）
    PolygonsD paths;
    paths.push_back(PolygonD{Point2D{0.0, 0.0}, Point2D{10.0, 0.0}});
    paths.push_back(PolygonD{Point2D{20.0, 5.0}, Point2D{10.0, 5.0}});
    opt.addRegion(7, paths);

    auto order = opt.optimizeOrder();
    BOOST_REQUIRE_EQUAL(order.size(), 1u);
    BOOST_CHECK_EQUAL(order[0], 7);

    auto out = opt.buildPaths();
    BOOST_REQUIRE_EQUAL(out.size(), 2u);
    // 第一条为 A（正向），第二条为 B 反转后：首点 (10,5)
    BOOST_CHECK_CLOSE(out[0].front().x, 0.0, 1e-9);
    BOOST_CHECK_CLOSE(out[1].front().x, 10.0, 1e-9);
    BOOST_CHECK_CLOSE(out[1].front().y, 5.0, 1e-9);
    BOOST_CHECK_CLOSE(out[1].back().x, 20.0, 1e-9);
}

BOOST_AUTO_TEST_CASE(optimizer_manual_route)
{
    RegionPathOptimizer opt;
    opt.addRegion(1, RegionHelper::MakeLines(0.0, 0.0, 10.0, 2, 2.0));
    opt.addRegion(2, RegionHelper::MakeLines(500.0, 0.0, 10.0, 2, 2.0));
    // 手动覆盖区域间代价不影响接口契约：仍能求解出包含全部区域的顺序与完整路径
    opt.addRoute(1, 2, 42.0);

    auto order = opt.optimizeOrder();
    BOOST_REQUIRE_EQUAL(order.size(), 2u);
    auto out = opt.buildPaths();
    BOOST_CHECK_EQUAL(out.size(), 4u);
}

BOOST_AUTO_TEST_CASE(lua_script_embedded_optimize)
{
    // 三个相距很远的区域，按乱序 2,0,1 传入；每区域 y 频带相隔 1000，可精确识别归属
    std::vector<PolygonsD> regions;
    regions.push_back(RegionHelper::MakeLines(0.0, 0.0, 10.0, 3, 2.0));
    regions.push_back(RegionHelper::MakeLines(0.0, 1000.0, 10.0, 3, 2.0));
    regions.push_back(RegionHelper::MakeLines(0.0, 2000.0, 10.0, 3, 2.0));
    std::vector<PolygonsD> input{regions[1], regions[2], regions[0]};

    std::filesystem::path script_path = std::filesystem::path(__FILE__).parent_path() / "optimize_paths.lua";
    auto out = LuaOptimizeRegionPaths(input, script_path.string(), "optimize_paths");
    BOOST_REQUIRE_EQUAL(out.size(), 9u);
    // 路径总数不变，且按区域连续分块（3 块，无交错）
    BOOST_CHECK_EQUAL(RegionHelper::CountContiguousBlocks(out, 3), 3);

    // 内联脚本方案：显式使用 PathOptimize 优化器对象
    const char* luaSrc = R"(
function optimize_paths(regions)
    local opt = PathOptimize.new()
    for i, paths in ipairs(regions) do
        opt:addRegion(i, paths)
    end
    opt:optimizeOrder()
    return opt:buildPaths()
end
)";
    auto out2 = LuaOptimizeRegionPathsString(input, luaSrc, "optimize_paths");
    BOOST_REQUIRE_EQUAL(out2.size(), 9u);
    BOOST_CHECK_EQUAL(RegionHelper::CountContiguousBlocks(out2, 3), 3);
}

BOOST_AUTO_TEST_CASE(polygon_mode_cluster_order)
{
    // 多边形模式（填充前）：区域1/2 聚簇（x≈0 与 x≈12），区域3 在远处（x≈1000）；乱序添加 1,3,2；
    // 各区域 y 频带相隔 1000，便于按 BandOf 识别输出归属
    RegionPathOptimizer opt;
    opt.addPolygonRegion(1, {RegionHelper::MakeRect(0.0, 0.0, 10.0, 10.0)});
    opt.addPolygonRegion(3, {RegionHelper::MakeRect(1000.0, 2000.0, 10.0, 10.0)});
    opt.addPolygonRegion(2, {RegionHelper::MakeRect(12.0, 1000.0, 10.0, 10.0)});

    auto order = opt.optimizeOrder();
    BOOST_REQUIRE_EQUAL(order.size(), 3u);
    std::vector<int> sorted = order;
    std::sort(sorted.begin(), sorted.end());
    const std::vector<int> expectedIds{1, 2, 3};
    BOOST_CHECK(sorted == expectedIds);

    // 聚簇区域 1、2 在环游中应相邻（含首尾相邻）
    auto pos = [&](int id)
    {
        return static_cast<int>(std::find(order.begin(), order.end(), id) - order.begin());
    };
    int d = std::abs(pos(1) - pos(2));
    BOOST_CHECK(d == 1 || d == static_cast<int>(order.size()) - 1);

    // 输出优化顺序的多边形集合：总数不变，区域连续分块（3 块）
    auto polygons = opt.buildPolygons();
    BOOST_CHECK_EQUAL(polygons.size(), 3u);
    BOOST_CHECK_EQUAL(RegionHelper::CountContiguousBlocks(polygons, 3), 3);
}

BOOST_AUTO_TEST_CASE(polygon_mode_entry_rotation)
{
    // 区域内多边形编排：起点旋转至最近顶点（不反转，保持环绕方向）
    RegionPathOptimizer opt;
    PolygonsD polygons;
    polygons.push_back(RegionHelper::MakeRect(0.0, 0.0, 10.0, 10.0));
    // B 顶点顺序故意从远端开始：(22,0),(22,10),(12,10),(12,0)，离 A 起点 (0,0) 最近的是 (12,0)
    PolygonD b;
    b.push_back(Point2D{22.0, 0.0});
    b.push_back(Point2D{22.0, 10.0});
    b.push_back(Point2D{12.0, 10.0});
    b.push_back(Point2D{12.0, 0.0});
    polygons.push_back(std::move(b));
    opt.addPolygonRegion(5, polygons);

    auto order = opt.optimizeOrder();
    BOOST_REQUIRE_EQUAL(order.size(), 1u);

    auto out = opt.buildPolygons();
    BOOST_REQUIRE_EQUAL(out.size(), 2u);
    // A 保持原样（首点即当前位置）
    BOOST_CHECK_CLOSE(out[0].front().x, 0.0, 1e-9);
    BOOST_CHECK_CLOSE(out[0].front().y, 0.0, 1e-9);
    // B 旋转起点至最近顶点 (12,0)，且环绕方向不变（旋转后次点为 (22,0)）
    BOOST_CHECK_CLOSE(out[1].front().x, 12.0, 1e-9);
    BOOST_CHECK_CLOSE(out[1].front().y, 0.0, 1e-9);
    BOOST_REQUIRE_EQUAL(out[1].size(), 4u);
    BOOST_CHECK_CLOSE(out[1][1].x, 22.0, 1e-9);
    BOOST_CHECK_CLOSE(out[1][1].y, 0.0, 1e-9);

    // 模式不可混用：已添加多边形区域后再添加填充路径区域应抛异常（反向同理）
    BOOST_CHECK_THROW(opt.addRegion(9, polygons), std::exception);
    RegionPathOptimizer opt2;
    opt2.addRegion(1, polygons);
    BOOST_CHECK_THROW(opt2.addPolygonRegion(2, polygons), std::exception);
}

BOOST_AUTO_TEST_CASE(lua_polygon_mode_embedded_optimize)
{
    // 三个相距很远的多边形区域，按乱序 2,0,1 传入；每区域 y 频带相隔 1000，可精确识别归属
    std::vector<PolygonsD> regions;
    regions.push_back({RegionHelper::MakeRect(0.0, 0.0, 10.0, 10.0)});
    regions.push_back({RegionHelper::MakeRect(0.0, 1000.0, 10.0, 10.0)});
    regions.push_back({RegionHelper::MakeRect(0.0, 2000.0, 10.0, 10.0)});
    std::vector<PolygonsD> input{regions[1], regions[2], regions[0]};

    std::filesystem::path script_path = std::filesystem::path(__FILE__).parent_path() / "optimize_paths.lua";
    auto out = LuaOptimizeRegionPolygons(input, script_path.string(), "optimize_polygons");
    BOOST_REQUIRE_EQUAL(out.size(), 3u);
    // 多边形总数不变，且按区域连续分块（3 块，无交错）
    BOOST_CHECK_EQUAL(RegionHelper::CountContiguousBlocks(out, 3), 3);

    // 内联脚本方案：显式使用 PathOptimize 优化器对象的多边形模式接口
    const char* luaSrc = R"(
function optimize_polygons(regions)
    local opt = PathOptimize.new()
    for i, polys in ipairs(regions) do
        opt:addPolygons(i, polys)
    end
    opt:optimizeOrder()
    return opt:buildPolygons()
end
)";
    auto out2 = LuaOptimizeRegionPolygonsString(input, luaSrc, "optimize_polygons");
    BOOST_REQUIRE_EQUAL(out2.size(), 3u);
    BOOST_CHECK_EQUAL(RegionHelper::CountContiguousBlocks(out2, 3), 3);
}

BOOST_AUTO_TEST_CASE(fill_stage_lua_exposes_path_optimize)
{
    // Fill 阶段入口 LuaCustomFillByFile 的 Lua 环境应已注册 PathOptimize（路径优化接入对应流程）
    auto script_path = std::filesystem::temp_directory_path() / "hsba_fill_stage_path_optimize.lua";
    {
        std::ofstream ofs(script_path);
        ofs << "function generate_fill(poly)\n"
               "    assert(PathOptimize ~= nil and PathOptimize.optimizeRegions ~= nil "
               "and PathOptimize.optimizePolygons ~= nil)\n"
               "    return {}\n"
               "end\n";
    }

    PolygonD sq;
    sq.emplace_back(Point2D{0.0, 0.0});
    sq.emplace_back(Point2D{10000.0, 0.0});
    sq.emplace_back(Point2D{10000.0, 10000.0});
    sq.emplace_back(Point2D{0.0, 10000.0});
    Polygons poly{Integerization(sq)};

    // 脚本内 assert 失败会抛异常；正常返回则说明 PathOptimize 可用
    auto res = LuaCustomFillByFile(poly, script_path.string(), "generate_fill", 0.5);
    BOOST_CHECK(res.empty());
    std::filesystem::remove(script_path);
}

BOOST_AUTO_TEST_SUITE_END()
