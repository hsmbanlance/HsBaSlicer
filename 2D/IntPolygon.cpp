#include "IntPolygon.hpp"

#include <algorithm>
#include <boost/container_hash/hash.hpp>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string_view>

#include <clipper2/clipper.engine.h>

namespace HsBa::Slicer
{
	Polygons MakeSimple(const Polygon& p, double epsilon)
	{
		return Clipper2Lib::SimplifyPaths(Polygons{ p }, epsilon);
	}
	Polygons MakeSimple(const Polygons& ps, double epsilon)
	{
		return Clipper2Lib::SimplifyPaths(ps, epsilon);
	}

	Polygons Union(const Polygon& left, const Polygon& right,
		Clipper2Lib::FillRule fill_rule)
	{
		return Clipper2Lib::Union(Polygons{ left }, Polygons{ right }, fill_rule);
	}
	Polygons Intersection(const Polygon& left, const Polygon& right, Clipper2Lib::FillRule fill_rule)
	{
		return Clipper2Lib::Intersect(Polygons{ left }, Polygons{ right }, fill_rule);
	}
	Polygons Difference(const Polygon& left, const Polygon& right, Clipper2Lib::FillRule fill_rule)
	{
		return Clipper2Lib::Difference(Polygons{ left }, Polygons{ right }, fill_rule);
	}
	Polygons Xor(const Polygon& left, const Polygon& right, Clipper2Lib::FillRule fill_rule)
	{
		return Clipper2Lib::Xor(Polygons{ left }, Polygons{ right }, fill_rule);
	}

	Polygons Union(const Polygons& left, const Polygons& right, Clipper2Lib::FillRule fill_rule)
	{
		return Clipper2Lib::Union(left, right, fill_rule);
	}
	Polygons Intersection(const Polygons& left, const Polygons& right, Clipper2Lib::FillRule fill_rule)
	{
		return Clipper2Lib::Intersect(left, right, fill_rule);
	}
	Polygons Difference(const Polygons& left, const Polygons& right, Clipper2Lib::FillRule fill_rule)
	{
		return Clipper2Lib::Difference(left, right, fill_rule);
	}
	Polygons Xor(const Polygons& left, const Polygons& right, Clipper2Lib::FillRule fill_rule)
	{
		return Clipper2Lib::Xor(left, right, fill_rule);
	}

	Polygons Offset(const Polygon& p, double delta, Clipper2Lib::JoinType join_type, Clipper2Lib::EndType end_type)
	{
		Polygons res;
		Clipper2Lib::ClipperOffset offset;
		offset.AddPath(p, join_type, end_type);
		offset.Execute(delta, res);
		return res;
	}
	Polygons Offset(const Polygons& ps, double delta, Clipper2Lib::JoinType join_type, Clipper2Lib::EndType end_type)
	{
		Polygons res;
		Clipper2Lib::ClipperOffset offset;
		offset.AddPaths(ps, join_type, end_type);
		offset.Execute(delta, res);
		return res;
	}

	Clipper2Lib::PointInPolygonResult PointInPolygons(const Clipper2Lib::Point64& point, const Polygons& polys, bool isEvenOdd)
	{
		const auto even_odd_inside = [](const Clipper2Lib::Point64& pt, const Polygons& ps) -> Clipper2Lib::PointInPolygonResult {
			for (const auto& pl : ps)
			{
				auto r = Clipper2Lib::PointInPolygon(pt, pl);
				switch (r)
				{
				case Clipper2Lib::PointInPolygonResult::IsOn:
					return Clipper2Lib::PointInPolygonResult::IsOn;
				case Clipper2Lib::PointInPolygonResult::IsInside:
					if (Area(pl) < 0)
					{
						return Clipper2Lib::PointInPolygonResult::IsOutside;
					}
					break;
				case Clipper2Lib::PointInPolygonResult::IsOutside:
					if (Area(pl) > 0)
					{
						return Clipper2Lib::PointInPolygonResult::IsOutside;
					}
					break;
				}
			}
			return Clipper2Lib::PointInPolygonResult::IsInside;
			};
		if (!isEvenOdd)
		{
			// polygon isn't even-odd, make it is even-odd
			Polygons odd_polys;
			Clipper2Lib::Clipper64 clipper;
			clipper.AddSubject(polys);
			clipper.Execute(Clipper2Lib::ClipType::Union, Clipper2Lib::FillRule::EvenOdd, odd_polys);
			return even_odd_inside(point, odd_polys);
		}
		else
		{
			return even_odd_inside(point, polys);
		}
	}

	double Area(const Polygon& p)
	{
		return Clipper2Lib::Area(p);
	}
	double Area(const Polygons& ps)
	{
		return Clipper2Lib::Area(ps);
	}

#ifdef HSBA_POLYGON_DUMP

	namespace
	{
		template<typename PointT>
		std::string MakeSvgPathData(const std::vector<PointT>& poly, bool close_path, double scale)
		{
			std::ostringstream out;
			out << std::fixed << std::setprecision(6);
			if (poly.empty())
				return {};
			out << "M " << (static_cast<double>(poly.front().x) / scale) << ' ' << (static_cast<double>(poly.front().y) / scale);
			for (size_t i = 1; i < poly.size(); ++i)
			{
				out << " L " << (static_cast<double>(poly[i].x) / scale) << ' ' << (static_cast<double>(poly[i].y) / scale);
			}
			if (close_path)
				out << " Z";
			return out.str();
		}

		template<typename PointT>
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

		template<typename PointT>
		void WriteSvgFile(std::string_view filename, const std::vector<std::vector<PointT>>& polys, bool close_path, double scale, std::string_view type_comment)
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
			file << "<!-- " << type_comment << " / integerization=" << integerization << " / scale=1/" << integerization << " -->\n";
			file << "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"" << min_x << " " << min_y << " " << width << " " << height << "\" preserveAspectRatio=\"xMinYMin meet\">\n";
			DumpSvgPaths(polys, file, close_path, scale);
			file << "</svg>\n";
		}
	}

	void DumpPolygon(const Polygon& p, std::string_view filename, bool close_path)
	{
		WriteSvgFile(filename, Polygons{ p }, close_path, integerization, "DumpPolygon(Polygon) scaled by 1/integerization");
	}

	void DumpPolygons(const Polygons& ps, std::string_view filename, bool close_path)
	{
		WriteSvgFile(filename, ps, close_path, integerization, "DumpPolygons(Polygons) scaled by 1/integerization");
	}
#endif

	namespace
	{

		void ExtractPolygonsFromPolyTree(const Clipper2Lib::PolyTree64& node,
			std::vector<Polygons>& out)
		{
			if (node.Polygon().empty())
			{
				for (const auto& child : node)
					ExtractPolygonsFromPolyTree(*child, out);
				return;
			}

			Polygons current;
			current.emplace_back(node.Polygon());

			for (const auto& hole : node)
				current.emplace_back(hole->Polygon());

			out.emplace_back(std::move(current));

			for (const auto& hole : node)
				for (const auto& island : *hole)
					ExtractPolygonsFromPolyTree(*island, out);
		}

		std::vector<Polygons> PolyTreeSplit(const Clipper2Lib::PolyTree64& tree)
		{
			std::vector<Polygons> result;
			ExtractPolygonsFromPolyTree(tree, result);
			return result;
		}
	}

	std::vector<Polygons> MakeSimpleAndSplit(const Polygon& p, double epsilon)
	{
		Clipper2Lib::PolyTree64 polyTree;
		Clipper2Lib::Clipper64 clipper;
		clipper.AddSubject(Polygons{ p });
		clipper.Execute(Clipper2Lib::ClipType::Union, Clipper2Lib::FillRule::EvenOdd, polyTree);
		std::vector<Polygons> result;
		auto res = PolyTreeSplit(polyTree);
		for (auto& ps : res)
		{
			ps = MakeSimple(ps, epsilon);
		}
		return res;
	}
}// namespace HsBa::Slicer

std::size_t std::hash<HsBa::Slicer::Polygon>::operator()(const HsBa::Slicer::Polygon& p) const
{
	size_t seed = 0;
	for (const auto& point : p)
	{
		boost::hash_combine(seed, point.x);
		boost::hash_combine(seed, point.y);
	}
	return seed;
}

std::size_t std::hash<HsBa::Slicer::Polygons>::operator()(const HsBa::Slicer::Polygons& p) const
{
	size_t seed = 0;
	for (const auto& poly : p)
	{
		std::hash<HsBa::Slicer::Polygon> hasher;
		boost::hash_combine(seed, hasher(poly));
	}
	return seed;
}