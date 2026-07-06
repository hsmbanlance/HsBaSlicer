#include "FloatPolygons.hpp"

#include <algorithm>
#include <boost/container_hash/hash.hpp>
#include <fstream>
#include <iomanip>
#include <numbers>
#include <sstream>
#include <string_view>

#include "base/error.hpp"

#ifdef HSBA_HAVE_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#endif  // HSBA_HAVE_FREETYPE

namespace HsBa::Slicer
{
PolygonsD MakeSimple(const PolygonD& p, double epsilon)
{
    return Clipper2Lib::SimplifyPaths(PolygonsD{p}, epsilon);
}
PolygonsD MakeSimple(const PolygonsD& ps, double epsilon)
{
    return Clipper2Lib::SimplifyPaths(ps, epsilon);
}

PolygonsD Union(const PolygonD& left, const PolygonD& right, Clipper2Lib::FillRule fill_rule)
{
    return Clipper2Lib::Union(PolygonsD{left}, PolygonsD{right}, fill_rule);
}
PolygonsD Intersection(const PolygonD& left, const PolygonD& right, Clipper2Lib::FillRule fill_rule)
{
    return Clipper2Lib::Intersect(PolygonsD{left}, PolygonsD{right}, fill_rule);
}
PolygonsD Difference(const PolygonD& left, const PolygonD& right, Clipper2Lib::FillRule fill_rule)
{
    return Clipper2Lib::Difference(PolygonsD{left}, PolygonsD{right}, fill_rule);
}
PolygonsD Xor(const PolygonD& left, const PolygonD& right, Clipper2Lib::FillRule fill_rule)
{
    return Clipper2Lib::Xor(PolygonsD{left}, PolygonsD{right}, fill_rule);
}

PolygonsD Union(const PolygonsD& left, const PolygonsD& right, Clipper2Lib::FillRule fill_rule)
{
    return Clipper2Lib::Union(left, right, fill_rule);
}
PolygonsD Intersection(const PolygonsD& left, const PolygonsD& right, Clipper2Lib::FillRule fill_rule)
{
    return Clipper2Lib::Intersect(left, right, fill_rule);
}
PolygonsD Difference(const PolygonsD& left, const PolygonsD& right, Clipper2Lib::FillRule fill_rule)
{
    return Clipper2Lib::Difference(left, right, fill_rule);
}
PolygonsD Xor(const PolygonsD& left, const PolygonsD& right, Clipper2Lib::FillRule fill_rule)
{
    return Clipper2Lib::Xor(left, right, fill_rule);
}

double Area(const PolygonD& p)
{
    return Clipper2Lib::Area(p);
}
double Area(const PolygonsD& ps)
{
    return Clipper2Lib::Area(ps);
}

#ifdef HSBA_POLYGON_DUMP

namespace
{
template <typename PointT>
std::string MakeSvgPathData(const std::vector<PointT>& poly, bool close_path, double scale)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(6);
    if (poly.empty())
        return {};
    out << "M " << (static_cast<double>(poly.front().x) / scale) << ' '
        << (static_cast<double>(poly.front().y) / scale);
    for (size_t i = 1; i < poly.size(); ++i)
    {
        out << " L " << (static_cast<double>(poly[i].x) / scale) << ' ' << (static_cast<double>(poly[i].y) / scale);
    }
    if (close_path)
        out << " Z";
    return out.str();
}

template <typename PointT>
void DumpSvgPaths(const std::vector<std::vector<PointT>>& polys, std::ofstream& file, bool close_path, double scale)
{
    for (const auto& poly : polys)
    {
        auto data = MakeSvgPathData(poly, close_path, scale);
        if (data.empty())
            continue;
        file << "  <path d=\"" << data << "\" fill=\"none\" stroke=\"#000\" stroke-width=0.5";
        file << " fill-rule=\"evenodd\" />\n";
    }
}

template <typename PointT>
void WriteSvgFile(std::string_view filename, const std::vector<std::vector<PointT>>& polys, bool close_path,
                  double scale, std::string_view type_comment)
{
    std::ofstream file(std::string(filename), std::ios::binary);
    if (!file)
        return;

    double min_x = 0;
    double min_y = 0;
    double max_x = 0;
    double max_y = 0;
    bool first = true;
    for (const auto& poly : polys)
    {
        for (const auto& point : poly)
        {
            double x = static_cast<double>(point.x) / scale;
            double y = static_cast<double>(point.y) / scale;
            if (first)
            {
                min_x = max_x = x;
                min_y = max_y = y;
                first = false;
            }
            else
            {
                min_x = std::min(min_x, x);
                min_y = std::min(min_y, y);
                max_x = std::max(max_x, x);
                max_y = std::max(max_y, y);
            }
        }
    }
    if (first)
    {
        min_x = min_y = 0.0;
        max_x = max_y = 1.0;
    }
    double width = std::max(1e-6, max_x - min_x);
    double height = std::max(1e-6, max_y - min_y);

    file << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    file << "<!-- HsBaSlicer polygon dump -->\n";
    file << "<!-- " << type_comment << " -->\n";
    file << "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"" << min_x << " " << min_y << " " << width << " "
         << height << "\" preserveAspectRatio=\"xMinYMin meet\">\n";
    DumpSvgPaths(polys, file, close_path, scale);
    file << "</svg>\n";
}
}  // namespace

void DumpPolygon(const PolygonD& p, std::string_view filename, bool close_path)
{
    WriteSvgFile(filename, PolygonsD{p}, close_path, 1.0, "DumpPolygon(PolygonD) without integerization scaling");
}

void DumpPolygons(const PolygonsD& ps, std::string_view filename, bool close_path)
{
    WriteSvgFile(filename, ps, close_path, 1.0, "DumpPolygons(PolygonsD) without integerization scaling");
}

#endif

Polygon Integerization(const PolygonD& poly)
{
    Polygon res;
    for (const auto& p : poly)
    {
        res.emplace_back(Point2{p.x * integerization, p.y * integerization});
    }
    return res;
}
Polygons Integerization(const PolygonsD& polys)
{
    Polygons res;
    for (const auto& poly : polys)
    {
        res.emplace_back(Integerization(poly));
    }
    return res;
}

PolygonD UnIntegerization(const Polygon& poly)
{
    PolygonD res;
    for (const auto& p : poly)
    {
        res.emplace_back(Point2D{p.x / integerization, p.y / integerization});
    }
    return res;
}

PolygonsD UnIntegerization(const Polygons& polys)
{
    PolygonsD res;
    for (const auto& poly : polys)
    {
        res.emplace_back(UnIntegerization(poly));
    }
    return res;
}
namespace
{
struct OutlineBuilder
{
    PolygonsD polygons;
    PolygonD current;
    int curveSegments = 8;

    static Point2D ToPointD(const FT_Vector& v)
    {
        return Point2D{static_cast<double>(v.x) / 64.0, static_cast<double>(v.y) / 64.0};
    }

    void CloseContour()
    {
        if (!current.empty())
        {
            if (current.front().x != current.back().x || current.front().y != current.back().y)
                current.push_back(current.front());
            polygons.push_back(std::move(current));
            current.clear();
        }
    }

    static int MoveTo(const FT_Vector* to, void* user)
    {
        auto* self = static_cast<OutlineBuilder*>(user);
        self->CloseContour();
        self->current.emplace_back(ToPointD(*to));
        return 0;
    }

    static int LineTo(const FT_Vector* to, void* user)
    {
        auto* self = static_cast<OutlineBuilder*>(user);
        self->current.emplace_back(ToPointD(*to));
        return 0;
    }

    static int ConicTo(const FT_Vector* control, const FT_Vector* to, void* user)
    {
        auto* self = static_cast<OutlineBuilder*>(user);
        if (self->current.empty())
            return 0;
        Point2D p0 = self->current.back();
        Point2D p1 = ToPointD(*control);
        Point2D p2 = ToPointD(*to);
        for (int step = 1; step <= self->curveSegments; ++step)
        {
            double t = static_cast<double>(step) / self->curveSegments;
            double u = 1.0 - t;
            double x = u * u * p0.x + 2 * u * t * p1.x + t * t * p2.x;
            double y = u * u * p0.y + 2 * u * t * p1.y + t * t * p2.y;
            self->current.emplace_back(Point2D{x, y});
        }
        return 0;
    }

    static int CubicTo(const FT_Vector* control1, const FT_Vector* control2, const FT_Vector* to, void* user)
    {
        auto* self = static_cast<OutlineBuilder*>(user);
        if (self->current.empty())
            return 0;
        Point2D p0 = self->current.back();
        Point2D p1 = ToPointD(*control1);
        Point2D p2 = ToPointD(*control2);
        Point2D p3 = ToPointD(*to);
        for (int step = 1; step <= self->curveSegments; ++step)
        {
            double t = static_cast<double>(step) / self->curveSegments;
            double u = 1.0 - t;
            double x = u * u * u * p0.x + 3 * u * u * t * p1.x + 3 * u * t * t * p2.x + t * t * t * p3.x;
            double y = u * u * u * p0.y + 3 * u * u * t * p1.y + 3 * u * t * t * p2.y + t * t * t * p3.y;
            self->current.emplace_back(Point2D{x, y});
        }
        return 0;
    }
};


PolygonD MakeEllipsePath(double cx, double cy, double rx, double ry, int segments, double rotation)
{
    PolygonD poly;
    poly.reserve(segments + 1);
    double angle_step = 2.0 * std::numbers::pi / segments;
    for (int i = 0; i < segments; ++i)
    {
        double angle = i * angle_step;
        double px = rx * std::cos(angle);
        double py = ry * std::sin(angle);
        double rxp = std::cos(rotation) * px - std::sin(rotation) * py;
        double ryp = std::sin(rotation) * px + std::cos(rotation) * py;
        poly.emplace_back(Point2D{cx + rxp, cy + ryp});
    }
    if (!poly.empty())
        poly.push_back(poly.front());
    return poly;
}
}  // namespace

PolygonD MakeRectangle(double x, double y, double width, double height)
{
    return PolygonD{Point2D{x, y}, Point2D{x + width, y}, Point2D{x + width, y + height}, Point2D{x, y + height},
                    Point2D{x, y}};
}

PolygonD MakeCircle(double cx, double cy, double radius, int segments)
{
    return MakeEllipsePath(cx, cy, radius, radius, std::max(3, segments), 0.0);
}

PolygonD MakeEllipse(double cx, double cy, double rx, double ry, int segments, double rotation)
{
    return MakeEllipsePath(cx, cy, rx, ry, std::max(3, segments), rotation);
}

PolygonD MakeRegularPolygon(double cx, double cy, double radius, int sides, double rotation)
{
    PolygonD poly;
    if (sides < 3)
        return poly;
    double angle_step = 2.0 * std::numbers::pi / sides;
    poly.reserve(sides + 1);
    for (int i = 0; i < sides; ++i)
    {
        double angle = rotation + i * angle_step;
        poly.emplace_back(Point2D{cx + radius * std::cos(angle), cy + radius * std::sin(angle)});
    }
    poly.push_back(poly.front());
    return poly;
}

PolygonsD TextToPolygons(const std::string& utf8_text, const std::string& font_file, double font_size, double x,
                         double y, int curve_segments)
{

#ifndef HSBA_HAVE_FREETYPE
    throw NotSupportedError("Need FreeType support");
#else
    FT_Library library;
    if (FT_Init_FreeType(&library) != 0)
        throw RuntimeError("Failed to initialize FreeType library");

    FT_Face face;
    if (FT_New_Face(library, font_file.c_str(), 0, &face) != 0)
    {
        FT_Done_FreeType(library);
        throw RuntimeError("Failed to load font file: " + font_file);
    }

    FT_Set_Char_Size(face, 0, static_cast<FT_F26Dot6>(font_size * 64.0), 0, 0);

    double penX = x;
    double penY = y;
    PolygonsD result;
    OutlineBuilder builder;
    builder.curveSegments = std::max(1, curve_segments);

    FT_Outline_Funcs funcs;
    funcs.move_to = OutlineBuilder::MoveTo;
    funcs.line_to = OutlineBuilder::LineTo;
    funcs.conic_to = OutlineBuilder::ConicTo;
    funcs.cubic_to = OutlineBuilder::CubicTo;
    funcs.shift = 0;
    funcs.delta = 0;

    for (unsigned char c : utf8_text)
    {
        FT_UInt glyph_index = FT_Get_Char_Index(face, c);
        if (FT_Load_Glyph(face, glyph_index, FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING) != 0)
        {
            continue;
        }

        if (face->glyph->format == FT_GLYPH_FORMAT_OUTLINE)
        {
            FT_Outline* outline = &face->glyph->outline;
            FT_Outline_Translate(outline, static_cast<FT_Pos>(penX * 64.0), static_cast<FT_Pos>(penY * 64.0));
            builder.current.clear();
            builder.polygons.clear();
            FT_Outline_Decompose(outline, &funcs, &builder);
            builder.CloseContour();
            for (auto& poly : builder.polygons)
                result.push_back(std::move(poly));
        }

        penX += static_cast<double>(face->glyph->advance.x) / 64.0;
    }

    FT_Done_Face(face);
    FT_Done_FreeType(library);
    return result;
#endif  // HSBA_HAVE_FREETYPE
}

}  // namespace HsBa::Slicer

std::size_t std::hash<HsBa::Slicer::PolygonD>::operator()(const HsBa::Slicer::PolygonD& p) const
{
    size_t seed = 0;
    for (const auto& point : p)
    {
        boost::hash_combine(seed, point.x);
        boost::hash_combine(seed, point.y);
    }
    return seed;
}

std::size_t std::hash<HsBa::Slicer::PolygonsD>::operator()(const HsBa::Slicer::PolygonsD& p) const
{
    size_t seed = 0;
    for (const auto& poly : p)
    {
        std::hash<HsBa::Slicer::PolygonD> hasher;
        boost::hash_combine(seed, hasher(poly));
    }
    return seed;
}