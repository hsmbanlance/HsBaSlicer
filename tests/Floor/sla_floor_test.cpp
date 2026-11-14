/** @file sla_floor_test.cpp
 * @brief SLA floor/raft geometry contract: millimetre configuration, border band and hull footprint.
 *
 * The raft is the one place where a millimetre length is handed to Offset(), which works on
 * integer-coordinate paths. A raw millimetre delta is a million times too small, so the whole
 * suite below pins real millimetre distances on the produced polygons instead of only checking
 * that something came back.
 */
#define BOOST_TEST_MODULE sla_floor_test
#include <boost/test/included/unit_test.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

#include "2D/FloatPolygons.hpp"
#include "LibHsBaSlicer/Floor/sla_floor.hpp"

namespace Slicer = HsBa::Slicer;

namespace
{
struct Box
{
    double min_x = 0.0;
    double min_y = 0.0;
    double max_x = 0.0;
    double max_y = 0.0;
};

// World-unit bounding box of a polygon set, converted the same way every consumer reads it.
Box BoundsOf(const Slicer::PolygonsD& polys)
{
    Box box{1e18, 1e18, -1e18, -1e18};
    for (const auto& path : polys)
    {
        for (const auto& pt : path)
        {
            box.min_x = std::min(box.min_x, pt.x);
            box.min_y = std::min(box.min_y, pt.y);
            box.max_x = std::max(box.max_x, pt.x);
            box.max_y = std::max(box.max_y, pt.y);
        }
    }
    return box;
}

Box BoundsOf(const Slicer::Polygons& polys)
{
    return BoundsOf(Slicer::UnIntegerization(polys));
}

bool SameBox(const Box& a, double min_x, double min_y, double max_x, double max_y)
{
    // The coordinate grid is 1/integerization mm, so half a grid step is well outside any
    // modelling error and still far below anything a unit mix-up could produce.
    constexpr double kEps = 1.0e-5;
    return std::abs(a.min_x - min_x) < kEps && std::abs(a.min_y - min_y) < kEps &&
           std::abs(a.max_x - max_x) < kEps && std::abs(a.max_y - max_y) < kEps;
}

bool HasBoxWithMargin(const Slicer::Polygons& polys, double margin)
{
    for (const auto& ring : polys)
    {
        if (SameBox(BoundsOf(Slicer::Polygons{ring}), -margin, -margin, 10.0 + margin, 10.0 + margin))
        {
            return true;
        }
    }
    return false;
}

// Total vertex count over every ring, so a fill that collapsed into a single line is visible.
std::size_t PointCount(const Slicer::Polygons& polys)
{
    std::size_t count = 0;
    for (const auto& ring : polys)
    {
        count += ring.size();
    }
    return count;
}

// A closed footprint square in integer coordinates, the type the floor API takes.
Slicer::Polygons FootprintSquare(double x, double y, double size)
{
    return Slicer::Polygons{Slicer::Integerization(Slicer::MakeRectangle(x, y, size, size))};
}

// Shoelace area of one ring, so the assertions do not depend on ring orientation.
double RingArea(const Slicer::PolygonD& ring)
{
    double area = 0.0;
    const std::size_t n = ring.size();
    for (std::size_t i = 0, j = n - 1; i < n; j = i++)
    {
        area += ring[j].x * ring[i].y - ring[i].x * ring[j].y;
    }
    return std::abs(area) * 0.5;
}
}  // namespace

BOOST_AUTO_TEST_SUITE(sla_floor)

// raft_offset and border_width are millimetres: a 10x10 footprint with the default
// raft_offset=2 and border_width=1 has to reach past the part by millimetres. Before the unit
// fix the offsets were 1e-6 mm and the raft collapsed onto the part itself.
// The loops are the centrelines of the border bands, so the outermost one sits half a band
// inside the raft outline: footprint + raft_offset + border_width - border_width/2.
BOOST_AUTO_TEST_CASE(raft_offset_expands_the_footprint_in_millimetres)
{
    const Slicer::Polygons footprint = FootprintSquare(0.0, 0.0, 10.0);
    Slicer::SlaFloorConfig config;  // raft_offset 2.0, border_width 1.0, border_count 2
    BOOST_CHECK_CLOSE(config.raft_offset, 2.0, 1e-9);
    BOOST_CHECK_CLOSE(config.border_width, 1.0, 1e-9);

    const Box raft = BoundsOf(Slicer::GenerateFloorRaft(footprint, config));
    BOOST_CHECK_MESSAGE(SameBox(raft, -2.5, -2.5, 12.5, 12.5),
                        "raft box " << raft.min_x << "," << raft.min_y << "," << raft.max_x << "," << raft.max_y);

    // Growing the raft must be a real expansion, not an artefact of the fill lines: the
    // footprint itself stays where the model is.
    BOOST_CHECK_MESSAGE(SameBox(BoundsOf(footprint), 0.0, 0.0, 10.0, 10.0), "footprint moved");
}

// The raft outline has to follow raft_offset one millimetre per millimetre; a parameter that is
// multiplied by a constant factor of 1e-6 responds to nothing at all.
BOOST_AUTO_TEST_CASE(raft_offset_parameter_changes_the_raft_size)
{
    const Slicer::Polygons footprint = FootprintSquare(0.0, 0.0, 10.0);
    Slicer::SlaFloorConfig config;
    // "small" is a reserved MIDL keyword that the Windows SDK headers define away, so these
    // boxes get names the platform cannot rewrite.
    const Box reduced = BoundsOf(Slicer::GenerateFloorRaft(footprint, config));
    config.raft_offset = 5.0;  // three millimetres more margin on every side

    const Box grown = BoundsOf(Slicer::GenerateFloorRaft(footprint, config));
    BOOST_CHECK_CLOSE(grown.min_x - reduced.min_x, -3.0, 1e-3);
    BOOST_CHECK_CLOSE(grown.min_y - reduced.min_y, -3.0, 1e-3);
    BOOST_CHECK_CLOSE(grown.max_x - reduced.max_x, 3.0, 1e-3);
    BOOST_CHECK_CLOSE(grown.max_y - reduced.max_y, 3.0, 1e-3);
}

// The border is the band between the raft outline and the interior: one outer boundary plus one
// hole, both at the configured millimetre distances. The inward offset used to carry the wrong
// sign, which made the difference empty.
BOOST_AUTO_TEST_CASE(border_is_the_band_around_the_interior)
{
    const Slicer::Polygons footprint = FootprintSquare(0.0, 0.0, 10.0);
    Slicer::SlaFloorConfig config;  // border_width 1.0, border_count 2 -> a 2mm band

    const Slicer::Polygons border = Slicer::GenerateFloorBorder(footprint, config);
    BOOST_REQUIRE_EQUAL(border.size(), 2u);

    const Box outer = BoundsOf(Slicer::Polygons{border[0]});
    const Box inner = BoundsOf(Slicer::Polygons{border[1]});
    const bool outer_is_raft_edge = SameBox(outer, -3.0, -3.0, 13.0, 13.0);
    const bool inner_is_fill_edge = SameBox(inner, -1.0, -1.0, 11.0, 11.0);
    BOOST_CHECK_MESSAGE(outer_is_raft_edge, "border outer edge " << outer.min_x << ".." << outer.max_x);
    BOOST_CHECK_MESSAGE(inner_is_fill_edge, "border inner edge " << inner.min_x << ".." << inner.max_x);
}

// border_count asks for that many loops inside the raft margin, each one border_width apart.
// A single Offset call cannot yield them, so a run with border_count=2 used to emit one.
BOOST_AUTO_TEST_CASE(raft_carries_one_border_loop_per_border_count)
{
    const Slicer::Polygons footprint = FootprintSquare(0.0, 0.0, 10.0);
    Slicer::SlaFloorConfig config;  // border_count 2

    const Slicer::Polygons raft = Slicer::GenerateFloorRaft(footprint, config);
    BOOST_REQUIRE(!raft.empty());

    // Loops sit 0.5mm inside each border band: 2.5mm and 1.5mm from the raft outline, i.e.
    // at +0.5 and -0.5 relative to the footprint plus raft_offset.
    BOOST_CHECK_MESSAGE(HasBoxWithMargin(raft, 2.5), "no border loop at the first band");
    BOOST_CHECK_MESSAGE(HasBoxWithMargin(raft, 1.5), "no border loop at the second band");
}

// The interior fill stays inside the last border loop and still spans the part. The fill region
// is the 12x12 square [-1,11] (see border_is_the_band_around_the_interior); ZigzagFill keeps its
// strokes a little inside that region - 0.01mm along a line, ~0.1mm between the outermost rows -
// and lays them on a 0.5mm grid. Every one of those margins is in millimetres: with the offsets
// handed to Offset() in coordinate units the region collapses onto the 10x10 footprint.
BOOST_AUTO_TEST_CASE(floor_fill_stays_inside_the_raft)
{
    const Slicer::Polygons footprint = FootprintSquare(0.0, 0.0, 10.0);
    Slicer::SlaFloorConfig config;  // fill_spacing 0.5mm

    const Slicer::Polygons fill = Slicer::GenerateFloorFill(footprint, config);
    BOOST_REQUIRE(!fill.empty());
    const Box box = BoundsOf(fill);
    BOOST_CHECK_MESSAGE(SameBox(box, -0.99, -0.90005, 10.99, 10.90005),
                        "fill box " << box.min_x << "," << box.min_y << "," << box.max_x << "," << box.max_y);

    // A 0.5mm grid across a 12mm region is two dozen rows. Handing the spacing over in
    // coordinate units would leave a single line.
    BOOST_CHECK_GE(PointCount(fill), 40u);
}

// The convex hull is the one option that has to merge separate bodies into a single raft.
BOOST_AUTO_TEST_CASE(convex_hull_merges_separate_bodies)
{
    Slicer::Polygons bodies = FootprintSquare(0.0, 0.0, 10.0);
    const Slicer::Polygons second = FootprintSquare(30.0, 0.0, 10.0);
    bodies.insert(bodies.end(), second.begin(), second.end());
    BOOST_REQUIRE_EQUAL(bodies.size(), 2u);

    Slicer::SlaFloorConfig merged;
    merged.use_convex_hull = true;
    const Slicer::Polygons contact = Slicer::GenerateFloorContact(bodies, merged);
    BOOST_REQUIRE_EQUAL(contact.size(), 1u);
    BOOST_CHECK_MESSAGE(SameBox(BoundsOf(contact), 0.0, 0.0, 40.0, 10.0), "hull does not span both bodies");
    BOOST_CHECK_CLOSE(RingArea(Slicer::UnIntegerization(Slicer::Polygons{contact[0]})[0]), 400.0, 1e-4);

    Slicer::SlaFloorConfig separate;
    separate.use_convex_hull = false;
    BOOST_CHECK_EQUAL(Slicer::GenerateFloorContact(bodies, separate).size(), 2u);
}

BOOST_AUTO_TEST_SUITE_END()
