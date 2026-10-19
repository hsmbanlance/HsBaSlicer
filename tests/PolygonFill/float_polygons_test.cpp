#define BOOST_TEST_MODULE float_polygons_test
#include <boost/test/included/unit_test.hpp>

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

#include "2D/FloatPolygons.hpp"
#include "base/error.hpp"

namespace Slicer = HsBa::Slicer;

// Constant used by the shape tests to compare against the analytic circle area.
namespace
{
constexpr double kPi = 3.14159265358979323846;

// Net (signed) area: Clipper2 orients holes opposite to their outer ring, so a
// plain signed sum over the result paths yields the true enclosed area after abs().
double NetArea(const Slicer::PolygonsD& polys)
{
    double total = 0.0;
    for (const auto& p : polys)
        total += Slicer::Area(p);
    return std::abs(total);
}

// First font we can actually open among common install locations; empty when none
// is present so glyph-based tests can skip cleanly instead of failing.
std::string FirstReadableFont()
{
    static const std::vector<std::string> candidates = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
    };
    for (const auto& c : candidates)
    {
        std::ifstream probe(c, std::ios::binary);
        if (probe.good())
            return c;
    }
    return {};
}

// A CFF/OTF font stores outlines as cubic Beziers (unlike TrueType's quadratic
// conics), so decomposing such a glyph drives OutlineBuilder::CubicTo. Returns the
// first openable CFF font, or empty when the host has none so the test can skip.
std::string FirstReadableCffFont()
{
    static const std::vector<std::string> candidates = {
        "/usr/share/fonts/opentype/urw-base35/NimbusSans-Regular.otf",
        "/usr/share/fonts/opentype/urw-base35/NimbusMonoPS-Regular.otf",
        "/usr/share/fonts/opentype/urw-base35/P052-Roman.otf",
        "/usr/share/fonts/opentype/urw-base35/C059-Roman.otf",
    };
    for (const auto& c : candidates)
    {
        std::ifstream probe(c, std::ios::binary);
        if (probe.good())
            return c;
    }
    return {};
}
}  // namespace

BOOST_AUTO_TEST_SUITE(float_polygons)

BOOST_AUTO_TEST_CASE(make_rectangle_area_and_vertices)
{
    auto rect = Slicer::MakeRectangle(1.0, 2.0, 4.0, 3.0);
    BOOST_REQUIRE_EQUAL(rect.size(), 5u);  // closed ring repeats the first vertex
    BOOST_CHECK_CLOSE(std::abs(Slicer::Area(rect)), 12.0, 1e-6);
    BOOST_CHECK_EQUAL(rect.front().x, rect.back().x);
    BOOST_CHECK_EQUAL(rect.front().y, rect.back().y);
}

BOOST_AUTO_TEST_CASE(make_circle_shape_and_area)
{
    const int segments = 128;
    auto circle = Slicer::MakeCircle(0.0, 0.0, 10.0, segments);
    BOOST_REQUIRE_EQUAL(circle.size(), static_cast<std::size_t>(segments + 1));
    // an inscribed polygon approaches pi * r^2
    BOOST_CHECK_CLOSE(std::abs(Slicer::Area(circle)), kPi * 100.0, 1.0);
}

BOOST_AUTO_TEST_CASE(make_ellipse_area_and_rotation)
{
    auto ellipse = Slicer::MakeEllipse(5.0, 5.0, 10.0, 4.0, 256, 0.0);
    BOOST_REQUIRE_EQUAL(ellipse.size(), 257u);
    BOOST_CHECK_CLOSE(std::abs(Slicer::Area(ellipse)), kPi * 10.0 * 4.0, 1.0);

    // rotating an ellipse about its centre preserves area
    auto rotated = Slicer::MakeEllipse(5.0, 5.0, 10.0, 4.0, 256, 0.7);
    BOOST_CHECK_CLOSE(std::abs(Slicer::Area(rotated)), std::abs(Slicer::Area(ellipse)), 1.0);
}

BOOST_AUTO_TEST_CASE(make_regular_polygon)
{
    // fewer than 3 sides cannot form a polygon
    BOOST_REQUIRE(Slicer::MakeRegularPolygon(0.0, 0.0, 5.0, 2).empty());

    auto square = Slicer::MakeRegularPolygon(0.0, 0.0, 10.0, 4);
    BOOST_REQUIRE_EQUAL(square.size(), 5u);
    // regular n-gon area with circumradius R: 0.5 * n * R^2 * sin(2*pi/n); n=4,R=10 -> 200
    BOOST_CHECK_CLOSE(std::abs(Slicer::Area(square)), 200.0, 1e-3);
}

BOOST_AUTO_TEST_CASE(boolean_two_polygons)
{
    const auto a = Slicer::MakeRectangle(0.0, 0.0, 10.0, 10.0);
    const auto b = Slicer::MakeRectangle(5.0, 5.0, 10.0, 10.0);

    BOOST_CHECK_CLOSE(NetArea(Slicer::Union(a, b)), 175.0, 1.0);
    BOOST_CHECK_CLOSE(NetArea(Slicer::Intersection(a, b)), 25.0, 1.0);
    BOOST_CHECK_CLOSE(NetArea(Slicer::Difference(a, b)), 75.0, 1.0);
    BOOST_CHECK_CLOSE(NetArea(Slicer::Xor(a, b)), 150.0, 1.0);
}

BOOST_AUTO_TEST_CASE(difference_creates_hole_signed_area)
{
    const auto big = Slicer::MakeRectangle(0.0, 0.0, 20.0, 20.0);
    const auto inner = Slicer::MakeRectangle(5.0, 5.0, 10.0, 10.0);
    // subtracting a fully-contained square yields a ring: net area = 400 - 100
    BOOST_CHECK_CLOSE(NetArea(Slicer::Difference(big, inner)), 300.0, 1.0);
}

BOOST_AUTO_TEST_CASE(boolean_polygon_sets)
{
    const Slicer::PolygonsD left{Slicer::MakeRectangle(0.0, 0.0, 10.0, 10.0)};
    const Slicer::PolygonsD right{Slicer::MakeRectangle(20.0, 0.0, 10.0, 10.0)};
    BOOST_CHECK_CLOSE(NetArea(Slicer::Union(left, right)), 200.0, 1.0);
    BOOST_CHECK(NetArea(Slicer::Intersection(left, right)) < 1e-6);  // disjoint
    BOOST_CHECK_CLOSE(NetArea(Slicer::Difference(left, right)), 100.0, 1.0);
    BOOST_CHECK_CLOSE(NetArea(Slicer::Xor(left, right)), 200.0, 1.0);
}

BOOST_AUTO_TEST_CASE(make_simple_removes_collinear)
{
    // a rectangle carrying an extra collinear vertex on the bottom edge
    Slicer::PolygonD poly{Slicer::Point2D{0, 0}, Slicer::Point2D{5, 0}, Slicer::Point2D{10, 0},
                          Slicer::Point2D{10, 10}, Slicer::Point2D{0, 10}};
    auto simplified = Slicer::MakeSimple(poly);
    BOOST_REQUIRE(!simplified.empty());
    // simplification preserves the enclosed area
    BOOST_CHECK_CLOSE(NetArea(simplified), 100.0, 1.0);

    auto setSimplified = Slicer::MakeSimple(Slicer::PolygonsD{poly});
    BOOST_REQUIRE(!setSimplified.empty());
}

BOOST_AUTO_TEST_CASE(area_overloads)
{
    const auto rect = Slicer::MakeRectangle(0.0, 0.0, 3.0, 4.0);
    BOOST_CHECK_CLOSE(std::abs(Slicer::Area(rect)), 12.0, 1e-6);
    BOOST_CHECK_CLOSE(std::abs(Slicer::Area(Slicer::PolygonsD{rect, rect})), 24.0, 1e-6);
}

BOOST_AUTO_TEST_CASE(integerization_roundtrip)
{
    Slicer::PolygonD poly{Slicer::Point2D{1.5, -2.25}, Slicer::Point2D{3.0, 4.0}, Slicer::Point2D{0.0, 0.0}};
    auto ints = Slicer::Integerization(poly);
    BOOST_REQUIRE_EQUAL(ints.size(), poly.size());
    BOOST_CHECK_EQUAL(ints[0].x, static_cast<std::int64_t>(1.5 * Slicer::integerization));
    auto back = Slicer::UnIntegerization(ints);
    BOOST_REQUIRE_EQUAL(back.size(), poly.size());
    BOOST_CHECK_CLOSE(back[0].x, 1.5, 1e-6);
    BOOST_CHECK_CLOSE(back[1].y, 4.0, 1e-6);
}

BOOST_AUTO_TEST_CASE(integerization_set_roundtrip)
{
    Slicer::PolygonsD polys{Slicer::PolygonD{Slicer::Point2D{1, 1}, Slicer::Point2D{2, 2}, Slicer::Point2D{3, 1}},
                            Slicer::PolygonD{Slicer::Point2D{4, 4}, Slicer::Point2D{5, 5}}};
    auto ints = Slicer::Integerization(polys);
    BOOST_REQUIRE_EQUAL(ints.size(), polys.size());
    auto back = Slicer::UnIntegerization(ints);
    BOOST_REQUIRE_EQUAL(back.size(), polys.size());
    BOOST_CHECK_CLOSE(back[0][0].x, 1.0, 1e-6);
    BOOST_CHECK_CLOSE(back[1][1].y, 5.0, 1e-6);
}

BOOST_AUTO_TEST_CASE(hash_specializations)
{
    Slicer::PolygonD p1{Slicer::Point2D{0, 0}, Slicer::Point2D{10, 0}, Slicer::Point2D{10, 10}};
    Slicer::PolygonD p2{Slicer::Point2D{0, 0}, Slicer::Point2D{10, 0}, Slicer::Point2D{10, 10}};
    Slicer::PolygonD p3{Slicer::Point2D{1, 1}, Slicer::Point2D{11, 1}, Slicer::Point2D{11, 11}};

    std::hash<Slicer::PolygonD> polyHash;
    BOOST_CHECK_EQUAL(polyHash(p1), polyHash(p2));
    BOOST_CHECK_NE(polyHash(p1), polyHash(p3));

    Slicer::PolygonsD s1{p1, p3};
    Slicer::PolygonsD s2{p1, p3};
    std::hash<Slicer::PolygonsD> setHash;
    BOOST_CHECK_EQUAL(setHash(s1), setHash(s2));
    BOOST_CHECK_NE(setHash(s1), setHash(Slicer::PolygonsD{p1}));
}

BOOST_AUTO_TEST_CASE(text_to_polygons_bad_font_throws)
{
    // a missing font file must surface as a RuntimeError from the FreeType load step
    BOOST_CHECK_THROW(Slicer::TextToPolygons("A", "no_such_font_file_zzz.ttf", 24.0), HsBa::Slicer::RuntimeError);
}

BOOST_AUTO_TEST_CASE(text_to_polygons_with_system_font)
{
    // pick any readable TTF from common locations; skip cleanly when none is present
    const std::vector<std::string> candidates = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
    };
    std::string font;
    for (const auto& c : candidates)
    {
        std::ifstream probe(c, std::ios::binary);
        if (probe.good())
        {
            font = c;
            break;
        }
    }
    if (font.empty())
    {
        BOOST_TEST("no system TTF font available; TextToPolygons skipped");
        return;
    }

    // rendering a glyph produces one or more closed contours with positive enclosed area
    const auto glyphs = Slicer::TextToPolygons("A", font, 48.0);
    BOOST_REQUIRE(!glyphs.empty());
    BOOST_CHECK(NetArea(glyphs) > 0.0);

    // empty input still opens the face but yields no contours
    BOOST_CHECK(Slicer::TextToPolygons("", font, 48.0).empty());
}

BOOST_AUTO_TEST_CASE(dump_polygon_and_polygons_write_svg)
{
    const auto dir = std::filesystem::temp_directory_path();
    const auto rect = Slicer::MakeRectangle(0.0, 0.0, 10.0, 10.0);

    auto slurp = [](const std::filesystem::path& p)
    {
        std::ifstream in(p);
        return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    };

    const auto single = dir / "hsba_dump_single.svg";
    Slicer::DumpPolygon(rect, single.string(), true);
    const std::string singlec = slurp(single);
    BOOST_CHECK(singlec.find("<svg") != std::string::npos);
    BOOST_CHECK(singlec.find("<path") != std::string::npos);
    BOOST_CHECK(singlec.find(" Z") != std::string::npos);  // close_path appends the Z terminator

    // an unclosed dump omits the Z terminator
    const auto open = dir / "hsba_dump_open.svg";
    Slicer::DumpPolygon(rect, open.string(), false);
    const std::string openc = slurp(open);
    BOOST_CHECK(openc.find("<path") != std::string::npos);
    BOOST_CHECK(openc.find(" Z") == std::string::npos);

    // a polygon set renders one <path> per contour and always emits a stroke
    const auto many = dir / "hsba_dump_many.svg";
    Slicer::DumpPolygons(Slicer::PolygonsD{rect, Slicer::MakeRectangle(20.0, 0.0, 5.0, 5.0)}, many.string(), true);
    const std::string manyc = slurp(many);
    BOOST_CHECK(manyc.find("<svg") != std::string::npos);
    BOOST_CHECK(manyc.find("stroke-width=0.5") != std::string::npos);

    // an empty polygon still produces a valid (degenerate) SVG frame without any path
    const auto empty = dir / "hsba_dump_empty.svg";
    Slicer::DumpPolygon(Slicer::PolygonD{}, empty.string(), true);
    const std::string emptyc = slurp(empty);
    BOOST_CHECK(emptyc.find("<svg") != std::string::npos);
    BOOST_CHECK(emptyc.find("<path") == std::string::npos);

    std::error_code ec;
    std::filesystem::remove(single, ec);
    std::filesystem::remove(open, ec);
    std::filesystem::remove(many, ec);
    std::filesystem::remove(empty, ec);
}

// 'A' is composed solely of straight segments, so the FreeType outline decomposer
// never calls the conic/cubic flattening callbacks. Curved glyphs (O, o, S, e) are
// quadratic Bezier curves in TrueType fonts, which drives OutlineBuilder::ConicTo.
BOOST_AUTO_TEST_CASE(text_to_polygons_curved_glyphs_exercise_conic)
{
    const std::string font = FirstReadableFont();
    if (font.empty())
    {
        BOOST_TEST("no system TTF font available; curved-glyph test skipped");
        return;
    }

    const auto glyphs = Slicer::TextToPolygons("OoSe", font, 64.0);
    BOOST_REQUIRE(!glyphs.empty());
    BOOST_CHECK(NetArea(glyphs) > 0.0);
    // CloseContour guarantees every returned ring is explicitly closed.
    for (const auto& poly : glyphs)
    {
        BOOST_REQUIRE(poly.size() >= 3);
        BOOST_CHECK_EQUAL(poly.front().x, poly.back().x);
        BOOST_CHECK_EQUAL(poly.front().y, poly.back().y);
    }
}

// Counterpart of the conic test for PostScript/CFF outlines: a curved glyph from a
// CFF font is flattened through the cubic Bezier callback (OutlineBuilder::CubicTo).
BOOST_AUTO_TEST_CASE(text_to_polygons_cff_glyphs_exercise_cubic)
{
    const std::string font = FirstReadableCffFont();
    if (font.empty())
    {
        BOOST_TEST("no system CFF/OTF font available; cubic-glyph test skipped");
        return;
    }

    const auto glyphs = Slicer::TextToPolygons("BSae", font, 64.0);
    BOOST_REQUIRE(!glyphs.empty());
    BOOST_CHECK(NetArea(glyphs) > 0.0);
    for (const auto& poly : glyphs)
    {
        BOOST_REQUIRE(poly.size() >= 3);
        BOOST_CHECK_EQUAL(poly.front().x, poly.back().x);
        BOOST_CHECK_EQUAL(poly.front().y, poly.back().y);
    }
}

// Writing to a path whose directory does not exist makes the ofstream fail to open,
// so WriteSvgFile takes its early-return instead of emitting a file (and must not throw).
BOOST_AUTO_TEST_CASE(dump_to_unwritable_path_is_a_noop)
{
    const auto rect = Slicer::MakeRectangle(0.0, 0.0, 10.0, 10.0);
    Slicer::DumpPolygon(rect, "/hsba_no_such_dir_xyz/out.svg", true);
    Slicer::DumpPolygons(Slicer::PolygonsD{rect}, "/hsba_no_such_dir_xyz/many.svg", true);
    BOOST_CHECK(!std::filesystem::exists("/hsba_no_such_dir_xyz/out.svg"));
}

BOOST_AUTO_TEST_SUITE_END()
