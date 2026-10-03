#include "spiral_path.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace HsBa::Slicer
{
namespace
{
// Twice the signed area of a closed loop (shoelace). The 0.5 factor is omitted
// because only relative magnitude and sign are used. Positive => CCW.
double SignedArea2(const PolygonD& p)
{
    double a = 0.0;
    const size_t n = p.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++)
    {
        a += p[j].x * p[i].y - p[i].x * p[j].y;
    }
    return a;
}

bool NearEqualXY(const Point2D& a, const Point2D& b)
{
    return std::abs(a.x - b.x) < 1e-9 && std::abs(a.y - b.y) < 1e-9;
}

// Return an open CCW loop with the duplicate closing vertex removed; empty if
// it has fewer than 3 distinct vertices.
PolygonD NormalizeOuterLoop(const PolygonD& in)
{
    PolygonD p = in;
    while (p.size() > 1 && NearEqualXY(p.front(), p.back()))
    {
        p.pop_back();
    }
    if (p.size() < 3)
    {
        return {};
    }
    if (SignedArea2(p) < 0.0)
    {
        std::reverse(p.begin(), p.end());
    }
    return p;
}

// Index of the vertex closest (XY) to the given target point.
size_t NearestToXY(const PolygonD& loop, double x, double y)
{
    size_t best = 0;
    double bestD = std::numeric_limits<double>::max();
    for (size_t i = 0; i < loop.size(); ++i)
    {
        double dx = loop[i].x - x;
        double dy = loop[i].y - y;
        double d = dx * dx + dy * dy;
        if (d < bestD)
        {
            bestD = d;
            best = i;
        }
    }
    return best;
}

// Deterministic seam for the first layer: lowest Y, then lowest X.
size_t CanonicalSeam(const PolygonD& loop)
{
    size_t best = 0;
    for (size_t i = 1; i < loop.size(); ++i)
    {
        if (loop[i].y < loop[best].y ||
            (std::abs(loop[i].y - loop[best].y) < 1e-12 && loop[i].x < loop[best].x))
        {
            best = i;
        }
    }
    return best;
}
}  // namespace

std::vector<SpiralPoint> SpiralizeOuterWall(const std::vector<PolygonsD>& layer_sections,
                                            const std::vector<double>& layer_zs)
{
    std::vector<SpiralPoint> path;
    const size_t nl = layer_sections.size();
    if (nl == 0 || layer_zs.size() != nl)
    {
        return path;
    }

    // Pick the outermost closed loop per layer (largest |signed area|); layers
    // without a valid closed contour are skipped.
    struct LayerLoop
    {
        PolygonD loop;
        double z = 0.0;
    };
    std::vector<LayerLoop> loops;
    loops.reserve(nl);
    for (size_t i = 0; i < nl; ++i)
    {
        const PolygonD* best = nullptr;
        double bestArea = 0.0;
        for (const auto& poly : layer_sections[i])
        {
            double area = std::abs(SignedArea2(poly));
            if (area > bestArea)
            {
                bestArea = area;
                best = &poly;
            }
        }
        if (!best || bestArea <= 0.0)
        {
            continue;
        }
        PolygonD loop = NormalizeOuterLoop(*best);
        if (loop.size() < 3)
        {
            continue;
        }
        loops.push_back(LayerLoop{ std::move(loop), layer_zs[i] });
    }
    if (loops.empty())
    {
        return path;
    }

    bool have_seam = false;
    double seam_x = 0.0;
    double seam_y = 0.0;
    for (size_t li = 0; li < loops.size(); ++li)
    {
        const PolygonD& loop = loops[li].loop;
        const size_t m = loop.size();
        const size_t start = have_seam ? NearestToXY(loop, seam_x, seam_y) : CanonicalSeam(loop);

        const double z_start = loops[li].z;
        // Ramp up to the next layer's height over one full revolution; the top
        // layer has nothing above to connect to, so it is traversed flat.
        const double z_end = (li + 1 < loops.size()) ? loops[li + 1].z : z_start;

        // Emit m+1 points so the loop closes back onto its seam at z_end, which
        // is exactly the next loop's start (same seam XY, same height) => continuous.
        for (size_t k = 0; k <= m; ++k)
        {
            const Point2D& pt = loop[(start + k) % m];
            const double frac = static_cast<double>(k) / static_cast<double>(m);
            path.push_back(SpiralPoint{ pt.x, pt.y, z_start + (z_end - z_start) * frac });
        }

        seam_x = loop[start].x;
        seam_y = loop[start].y;
        have_seam = true;
    }
    return path;
}

}  // namespace HsBa::Slicer
