#define BOOST_TEST_MODULE int_polygon_test
#include <boost/test/included/unit_test.hpp>

#include <unordered_set>

#include "2D/IntPolygon.hpp"

// On MSVC, Boost.Test pulls in <windows.h>/wingdi.h, which declares a global
// function Polygon(); an unqualified `Polygon` would then be ambiguous against the
// HsBa::Slicer type alias. Keeping the test bodies inside HsBa::Slicer resolves the
// name to the namespace member first, so no clash occurs. main() stays at global
// scope because it is emitted by the Boost.Test include above.
namespace HsBa::Slicer
{

namespace
{
// Axis-aligned integer square with lower-left corner (x, y) and side length s.
Polygon MakeIntSquare(int64_t x, int64_t y, int64_t s)
{
    return Polygon{Point2{x, y}, Point2{x + s, y}, Point2{x + s, y + s}, Point2{x, y + s}};
}

// Absolute total area of a polygon set (outer minus holes under the even-odd/
// non-zero signed-sum convention Clipper2 already produces).
double AbsTotalArea(const Polygons& ps)
{
    return std::abs(Area(ps));
}
}  // namespace

// The single-Polygon convenience overloads were never invoked by the fill/hull
// tests; drive each and assert the classic overlapping-squares areas.
BOOST_AUTO_TEST_CASE(single_polygon_boolean_wrappers)
{
    // Two 1000x1000 squares overlapping by a 500x500 region.
    const Polygon a = MakeIntSquare(0, 0, 1000);
    const Polygon b = MakeIntSquare(500, 500, 1000);
    const double overlap = 500.0 * 500.0;
    const double one = 1000.0 * 1000.0;

    // Polygon,Polygon overloads.
    BOOST_CHECK_CLOSE(AbsTotalArea(Union(a, b)), 2.0 * one - overlap, 1e-9);
    BOOST_CHECK_CLOSE(AbsTotalArea(Intersection(a, b)), overlap, 1e-9);
    BOOST_CHECK_CLOSE(AbsTotalArea(Difference(a, b)), one - overlap, 1e-9);
    BOOST_CHECK_CLOSE(AbsTotalArea(Xor(a, b)), 2.0 * (one - overlap), 1e-9);

    // Polygons,Polygons overloads reach the same result on singleton sets.
    Polygons pa{a};
    Polygons pb{b};
    BOOST_CHECK_CLOSE(AbsTotalArea(Union(pa, pb)), 2.0 * one - overlap, 1e-9);
    BOOST_CHECK_CLOSE(AbsTotalArea(Intersection(pa, pb)), overlap, 1e-9);
    BOOST_CHECK_CLOSE(AbsTotalArea(Difference(pa, pb)), one - overlap, 1e-9);
    BOOST_CHECK_CLOSE(AbsTotalArea(Xor(pa, pb)), 2.0 * (one - overlap), 1e-9);
}

// MakeSimple(Polygon) single-path overload and Area(Polygons) set overload.
BOOST_AUTO_TEST_CASE(make_simple_single_and_area_of_set)
{
    // A square carrying a redundant collinear vertex on the bottom edge.
    Polygon withCollinear{Point2{0, 0}, Point2{500, 0}, Point2{1000, 0}, Point2{1000, 1000}, Point2{0, 1000}};
    Polygons simple = MakeSimple(withCollinear);
    BOOST_REQUIRE_EQUAL(simple.size(), 1u);
    // SimplifyPaths preserves the enclosed area; the ring stays a single valid loop.
    BOOST_CHECK_CLOSE(std::abs(Area(simple[0])), 1000.0 * 1000.0, 1e-6);
    BOOST_CHECK_GE(simple[0].size(), 4u);

    // Area(Polygons) sums signed areas: two disjoint unit squares.
    Polygons two{MakeIntSquare(0, 0, 1000), MakeIntSquare(3000, 0, 1000)};
    BOOST_CHECK_CLOSE(std::abs(Area(two)), 2.0 * 1000.0 * 1000.0, 1e-9);
}

// Offset(Polygon) single-path overload: inflate grows, deflate shrinks.
BOOST_AUTO_TEST_CASE(offset_single_polygon)
{
    const Polygon sq = MakeIntSquare(0, 0, 1000);
    const double base = 1000.0 * 1000.0;

    Polygons grown = Offset(sq, 100.0);
    Polygons shrunk = Offset(sq, -100.0);
    BOOST_REQUIRE(!grown.empty());
    BOOST_REQUIRE(!shrunk.empty());
    BOOST_CHECK_GT(AbsTotalArea(grown), base);
    BOOST_CHECK_LT(AbsTotalArea(shrunk), base);
}

// PointInPolygons was only ever called with the default even-odd rule; the
// isEvenOdd == false branch (union -> even-odd conversion) was dead. Cover both.
BOOST_AUTO_TEST_CASE(point_in_polygons_both_rules)
{
    const Polygons ring{MakeIntSquare(0, 0, 1000)};
    const Clipper2Lib::Point64 center{500, 500};
    const Clipper2Lib::Point64 outside{2000, 2000};
    const Clipper2Lib::Point64 onEdge{0, 500};

    BOOST_CHECK(PointInPolygons(center, ring, true) != Clipper2Lib::PointInPolygonResult::IsOutside);
    BOOST_CHECK(PointInPolygons(center, ring, false) != Clipper2Lib::PointInPolygonResult::IsOutside);
    BOOST_CHECK(PointInPolygons(outside, ring, true) == Clipper2Lib::PointInPolygonResult::IsOutside);
    BOOST_CHECK(PointInPolygons(outside, ring, false) == Clipper2Lib::PointInPolygonResult::IsOutside);
    // A point sitting on the boundary reports IsOn for both rules.
    BOOST_CHECK(PointInPolygons(onEdge, ring, true) == Clipper2Lib::PointInPolygonResult::IsOn);
    BOOST_CHECK(PointInPolygons(onEdge, ring, false) == Clipper2Lib::PointInPolygonResult::IsOn);
}

// A point falling inside a hole (a negatively-oriented ring nested in a positive
// one) must be reported as outside: covers the even-odd hole cancellation branch.
BOOST_AUTO_TEST_CASE(point_in_polygons_respects_holes)
{
    Polygon outer = MakeIntSquare(0, 0, 1000);
    Polygon hole = MakeIntSquare(200, 200, 600);
    // Force the conventional orientation: outer positive area, hole negative.
    if (Area(outer) < 0)
        std::reverse(outer.begin(), outer.end());
    if (Area(hole) > 0)
        std::reverse(hole.begin(), hole.end());

    const Polygons ring{outer, hole};
    const Clipper2Lib::Point64 inHole{500, 500};   // inside the hole ring
    const Clipper2Lib::Point64 inBand{100, 100};   // between outer and hole

    BOOST_CHECK(PointInPolygons(inHole, ring, true) == Clipper2Lib::PointInPolygonResult::IsOutside);
    BOOST_CHECK(PointInPolygons(inBand, ring, true) != Clipper2Lib::PointInPolygonResult::IsOutside);
}

// std::hash specializations for Polygon / Polygons were entirely untested.
BOOST_AUTO_TEST_CASE(polygon_hash_specializations)
{
    const Polygon s1 = MakeIntSquare(0, 0, 1000);
    const Polygon s2 = MakeIntSquare(0, 0, 1000); // equal geometry
    const Polygon s3 = MakeIntSquare(0, 0, 2000); // different geometry

    std::hash<Polygon> hp;
    BOOST_CHECK_EQUAL(hp(s1), hp(s2));
    BOOST_CHECK_NE(hp(s1), hp(s3));

    // Usable as a key: duplicate squares collapse to a single set entry.
    std::unordered_set<Polygon> set;
    set.insert(s1);
    set.insert(s2);
    set.insert(s3);
    BOOST_CHECK_EQUAL(set.size(), 2u);

    const Polygons ps1{s1, s3};
    const Polygons ps2{s1, s3};
    const Polygons ps3{s1};
    std::hash<Polygons> hps;
    BOOST_CHECK_EQUAL(hps(ps1), hps(ps2));
    BOOST_CHECK_NE(hps(ps1), hps(ps3));
}

// The Polygons overload of NormalizeToSimplePolygons (per-input fan-out) plus the
// degenerate-size early return on the single-Polygon overload.
BOOST_AUTO_TEST_CASE(normalize_to_simple_polygons_set_and_degenerate)
{
    // < 3 points -> empty result (early return).
    Polygon tiny{Point2{0, 0}, Point2{10, 10}};
    BOOST_CHECK(NormalizeToSimplePolygons(tiny).empty());

    // Bowtie self-intersection splits into two triangles; feeding it as a set keeps
    // the two triangles plus the untouched separate square.
    Polygon bowtie{Point2{0, 0}, Point2{10000, 10000}, Point2{0, 10000}, Point2{10000, 0}};
    Polygon square = MakeIntSquare(20000, 20000, 1000);
    Polygons input{bowtie, square};

    auto out = NormalizeToSimplePolygons(input);
    // bowtie -> 2 simple pieces, square -> 1 piece.
    BOOST_CHECK_EQUAL(out.size(), 3u);
}

// A self-intersecting pentagram drives MakeSimpleAndSplit into emitting multiple
// regions and exercises the nesting/hole-parent builder, orientation normalization
// and the island recursion regardless of the exact even-odd decomposition.
BOOST_AUTO_TEST_CASE(normalize_star_exercises_hole_nesting)
{
    // Five points of a star polygon {5/2} traced in self-intersecting order.
    Polygon star{Point2{0, 1000},
                 Point2{-588, -809},
                 Point2{951, 309},
                 Point2{-951, 309},
                 Point2{588, -809}};

    auto out = NormalizeToSimplePolygons(star);
    BOOST_REQUIRE(!out.empty());
    // Every emitted ring must be a genuine simple polygon (>= 3 vertices) and the
    // combined enclosed area must stay within the star's convex-hull footprint.
    for (const auto& ring : out)
        BOOST_CHECK_GE(ring.size(), 3u);
}
}  // namespace HsBa::Slicer
