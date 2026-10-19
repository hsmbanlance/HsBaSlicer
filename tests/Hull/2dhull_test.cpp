#define BOOST_TEST_MODULE hull_2d_test
#include <boost/test/included/unit_test.hpp>

#include <algorithm>

#include "2D/2Dhull.hpp"

// On MSVC, Boost.Test pulls in <windows.h>/wingdi.h, which declares a global
// function Polygon(); an unqualified `Polygon` would then be ambiguous against the
// HsBa::Slicer type alias. Keeping the test bodies inside HsBa::Slicer resolves the
// name to the namespace member first, so no clash occurs. main() stays at global
// scope because it is emitted by the Boost.Test include above.
namespace HsBa::Slicer
{
namespace
{
// Absolute shoelace area of a closed ring.
double RingArea(const Polygon& r)
{
    double a = 0.0;
    for (size_t i = 0, n = r.size(); i < n; ++i)
    {
        const Point2& p = r[i];
        const Point2& q = r[(i + 1) % n];
        a += static_cast<double>(p.x) * q.y - static_cast<double>(q.x) * p.y;
    }
    return std::abs(a) / 2.0;
}

// Strictly convex iff every consecutive edge cross product is non-zero and shares a
// single sign. Orientation (CW vs CCW) is deliberately not assumed; only convexity is.
bool IsStrictlyConvex(const Polygon& r)
{
    const size_t n = r.size();
    if (n < 3)
        return true;
    int sign = 0;
    for (size_t i = 0; i < n; ++i)
    {
        const Point2& o = r[i];
        const Point2& a = r[(i + 1) % n];
        const Point2& b = r[(i + 2) % n];
        const double cross = static_cast<double>(a.x - o.x) * (b.y - a.y) - static_cast<double>(a.y - o.y) * (b.x - a.x);
        if (cross == 0.0)
            return false;
        const int cur = cross > 0.0 ? 1 : -1;
        if (sign == 0)
            sign = cur;
        else if (cur != sign)
            return false;
    }
    return true;
}
}  // namespace

BOOST_AUTO_TEST_SUITE(hull_2d_test)

BOOST_AUTO_TEST_CASE(convex_hull_int_removes_interior_point)
{
    // square plus one interior point; Graham scan must drop the interior vertex
    Polygon poly{Point2{0, 0}, Point2{10, 0}, Point2{10, 10}, Point2{5, 5}, Point2{0, 10}};
    auto hull = ConvexHull(poly);

    BOOST_TEST(hull.size() == 4);
    for (const auto& p : hull)
    {
        const bool from_input = std::find(poly.begin(), poly.end(), p) != poly.end();
        BOOST_TEST(from_input);
    }
    // the interior point must not survive
    const bool interior_kept = std::find(hull.begin(), hull.end(), Point2{5, 5}) != hull.end();
    BOOST_TEST(!interior_kept);
}

BOOST_AUTO_TEST_CASE(convex_hull_int_small_polygon_passthrough)
{
    Polygon tri{Point2{0, 0}, Point2{4, 0}, Point2{0, 3}};
    auto hull = ConvexHull(tri);
    BOOST_TEST(hull.size() == 3);

    Polygon two{Point2{1, 1}, Point2{2, 2}};
    BOOST_TEST(ConvexHull(two).size() == 2);
}

BOOST_AUTO_TEST_CASE(concave_hull_int_inserts_additional_points)
{
    Polygon poly{Point2{0, 0}, Point2{10, 0}, Point2{10, 10}, Point2{5, 5}, Point2{0, 10}};
    const int additional = 2;
    auto hull = ConvexHull(poly);
    auto concave = ConcaveHullSimulation(poly, additional);

    // each hull edge contributes its start vertex plus 'additional' interpolants,
    // so the densified ring holds hull.size() * (additional + 1) points
    BOOST_TEST(concave.size() == hull.size() * (additional + 1));
    const bool first_is_hull_vertex = concave.front() == hull.front();
    BOOST_TEST(first_is_hull_vertex);
}

BOOST_AUTO_TEST_CASE(convex_hull_int_multiple_polygons)
{
    Polygons polys;
    polys.emplace_back(Polygon{Point2{0, 0}, Point2{1, 0}, Point2{0, 1}});
    polys.emplace_back(Polygon{Point2{10, 10}, Point2{11, 10}, Point2{10, 11}});

    auto hull = ConvexHull(polys);
    // 6 input points, the two inner ones (1,0)/(0,1) style candidates may drop
    BOOST_TEST(hull.size() >= 4);
    BOOST_TEST(hull.size() <= 6);

    auto concave = ConcaveHullSimulation(polys, 1);
    BOOST_TEST(concave.size() == hull.size() * 2);
}

BOOST_AUTO_TEST_CASE(convex_hull_double_removes_interior_point)
{
    PolygonD poly{Point2D{0.0, 0.0}, Point2D{10.0, 0.0}, Point2D{10.0, 10.0}, Point2D{5.5, 5.5}, Point2D{0.0, 10.0}};
    auto hull = ConvexHull(poly);

    BOOST_TEST(hull.size() == 4);
    for (const auto& p : hull)
    {
        const bool from_input = std::find(poly.begin(), poly.end(), p) != poly.end();
        BOOST_TEST(from_input);
    }
    const bool interior_kept = std::find(hull.begin(), hull.end(), Point2D{5.5, 5.5}) != hull.end();
    BOOST_TEST(!interior_kept);
}

BOOST_AUTO_TEST_CASE(convex_hull_double_small_polygon_passthrough)
{
    PolygonD tri{Point2D{0.0, 0.0}, Point2D{4.0, 0.0}, Point2D{0.0, 3.0}};
    BOOST_TEST(ConvexHull(tri).size() == 3);
}

BOOST_AUTO_TEST_CASE(concave_hull_double_inserts_additional_points)
{
    PolygonD poly{Point2D{0.0, 0.0}, Point2D{10.0, 0.0}, Point2D{10.0, 10.0}, Point2D{5.5, 5.5}, Point2D{0.0, 10.0}};
    auto hull = ConvexHull(poly);
    auto concave = ConcaveHullSimulation(poly, 3);
    BOOST_TEST(concave.size() == hull.size() * 4);
}

BOOST_AUTO_TEST_CASE(convex_hull_double_multiple_polygons)
{
    PolygonsD polys;
    polys.emplace_back(PolygonD{Point2D{0.0, 0.0}, Point2D{2.0, 0.0}, Point2D{0.0, 2.0}});
    polys.emplace_back(PolygonD{Point2D{9.0, 9.0}, Point2D{12.0, 9.0}, Point2D{9.0, 12.0}});

    auto hull = ConvexHull(polys);
    BOOST_TEST(hull.size() >= 4);
    BOOST_TEST(hull.size() <= 6);

    auto concave = ConcaveHullSimulation(polys, 1);
    BOOST_TEST(concave.size() == hull.size() * 2);
}

// First vertex is deliberately not the lowest point, forcing the pivot search to
// update minIndex and swap (the branch the existing lowest-first inputs never hit).
BOOST_AUTO_TEST_CASE(convex_hull_int_pivot_is_not_first_vertex)
{
    Polygon poly{Point2{10, 10}, Point2{0, 0}, Point2{10, 0}, Point2{0, 10}, Point2{5, 5}};
    auto hull = ConvexHull(poly);

    BOOST_TEST(hull.size() == 4);
    BOOST_TEST(RingArea(hull) == 100.0);
    // The rectangle interior point must be dropped.
    const bool interior_kept = std::find(hull.begin(), hull.end(), Point2{5, 5}) != hull.end();
    BOOST_TEST(!interior_kept);

    PolygonD polyd{Point2D{10.0, 10.0}, Point2D{0.0, 0.0}, Point2D{10.0, 0.0}, Point2D{0.0, 10.0},
                   Point2D{5.5, 5.5}};
    BOOST_TEST(ConvexHull(polyd).size() == 4);
}

// A scattered rectangle (unordered, interior fillers) must reduce to exactly the four
// corners with the correct area and a consistently-oriented strictly convex ring;
// this actively checks the polar-angle sort + scan ordering, not just the vertex count.
BOOST_AUTO_TEST_CASE(convex_hull_int_scattered_is_convex_with_known_area)
{
    Polygon poly{Point2{5, 0}, Point2{2, 1}, Point2{0, 3}, Point2{5, 3}, Point2{1, 2}, Point2{4, 1},
                 Point2{3, 2}, Point2{2, 2}, Point2{0, 0}};
    auto hull = ConvexHull(poly);

    BOOST_TEST(hull.size() == 4);
    BOOST_TEST(RingArea(hull) == 15.0);
    BOOST_TEST(IsStrictlyConvex(hull));
    // Every hull vertex must be an extreme corner of the input rectangle.
    for (const auto& p : hull)
    {
        const bool from_input = std::find(poly.begin(), poly.end(), p) != poly.end();
        BOOST_TEST(from_input);
    }
}

// Empty input drives the size==0 early-return of both ConcaveHullSimulation overloads
// (ConvexHull of an empty polygon is empty, so the interpolation loop is skipped).
BOOST_AUTO_TEST_CASE(concave_hull_empty_polygon_returns_empty)
{
    Polygon empty;
    BOOST_TEST(ConcaveHullSimulation(empty, 3).empty());

    PolygonD emptyd;
    BOOST_TEST(ConcaveHullSimulation(emptyd, 3).empty());
}

BOOST_AUTO_TEST_SUITE_END()
}  // namespace HsBa::Slicer
