#include "FloatPolygons.hpp"

#include <algorithm>
#include <boost/container_hash/hash.hpp>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string_view>

namespace HsBa::Slicer
{
	PolygonsD MakeSimple(const PolygonD& p, double epsilon)
	{
		return Clipper2Lib::SimplifyPaths(PolygonsD{ p }, epsilon);
	}
	PolygonsD MakeSimple(const PolygonsD& ps, double epsilon)
	{
		return Clipper2Lib::SimplifyPaths(ps, epsilon);
	}

	PolygonsD Union(const PolygonD& left, const PolygonD& right,
		Clipper2Lib::FillRule fill_rule)
	{
		return Clipper2Lib::Union(PolygonsD{ left }, PolygonsD{ right }, fill_rule);
	}
	PolygonsD Intersection(const PolygonD& left, const PolygonD& right, Clipper2Lib::FillRule fill_rule)
	{
		return Clipper2Lib::Intersect(PolygonsD{ left }, PolygonsD{ right }, fill_rule);
	}
	PolygonsD Difference(const PolygonD& left, const PolygonD& right, Clipper2Lib::FillRule fill_rule)
	{
		return Clipper2Lib::Difference(PolygonsD{ left }, PolygonsD{ right }, fill_rule);
	}
	PolygonsD Xor(const PolygonD& left, const PolygonD& right, Clipper2Lib::FillRule fill_rule)
	{
		return Clipper2Lib::Xor(PolygonsD{ left }, PolygonsD{ right }, fill_rule);
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
			file << "<!-- " << type_comment << " -->\n";
			file << "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"" << min_x << " " << min_y << " " << width << " " << height << "\" preserveAspectRatio=\"xMinYMin meet\">\n";
			DumpSvgPaths(polys, file, close_path, scale);
			file << "</svg>\n";
		}
	}

	void DumpPolygon(const PolygonD& p, std::string_view filename, bool close_path)
	{
		WriteSvgFile(filename, PolygonsD{ p }, close_path, 1.0, "DumpPolygon(PolygonD) without integerization scaling");
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
			res.emplace_back(Point2{ p.x * integerization,p.y * integerization });
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
			res.emplace_back(Point2D{ p.x / integerization,p.y / integerization });
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


}// namespace HsBa::Slicer

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