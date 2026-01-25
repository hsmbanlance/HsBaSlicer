#include "PolygonFill.hpp"

#include <cmath>
#include <lua.hpp>
#include <sstream>
#include <numbers>
#include <cstring>

#include "base/error.hpp"
#include "LuaAdapter.hpp"
#include "utils/LuaNewObject.hpp"


namespace HsBa::Slicer
{
	namespace
	{
		constexpr double DEG_TO_RAD_FACTOR = 180.0;
		constexpr int MAX_FILL_ITERATIONS = 10000;
		constexpr int MAX_BINARY_SEARCH_ITERATIONS = 40;
		constexpr int LARGE_ROW_OFFSET = 1000000;
		constexpr double ANGLE_EPSILON = 12.0;
		constexpr double INTEGERIZATION_PRECISION = 100.0;

		Polygon ClosePath(const Polygon& p)
		{
			if (p.empty() || p.front() == p.back()) return p;
			Polygon res = p;
			res.push_back(res.front());
			return res;
		}

		std::vector<std::vector<std::pair<Point2D, Point2D>>> LineFilling(const Polygons& poly, double spacing, double angle_deg, double lineThickness, double& ux, double& uy, PolygonsD& polyD)
		{
			std::vector<std::vector<std::pair<Point2D, Point2D>>> rows;
			polyD = UnIntegerization(poly);

			double minx = 1e300, miny = 1e300, maxx = -1e300, maxy = -1e300;
			for (auto& ps : polyD)
			{
				for (const auto& pt : ps)
				{
					minx = std::min(minx, pt.x);
					miny = std::min(miny, pt.y);
					maxx = std::max(maxx, pt.x);
					maxy = std::max(maxy, pt.y);
				}
			}
			if (minx > maxx) return rows;

			double ang = angle_deg * std::numbers::pi_v<double> / DEG_TO_RAD_FACTOR;
			ux = std::cos(ang), uy = std::sin(ang);
			double vx = -uy, vy = ux;

			double cornersX[4] = { minx, maxx, maxx, minx };
			double cornersY[4] = { miny, miny, maxy, maxy };
			double minProj = 1e300, maxProj = -1e300;
			for (int i = 0; i != 4; ++i)
			{
				double proj = cornersX[i] * vx + cornersY[i] * vy;
				minProj = std::min(minProj, proj);
				maxProj = std::max(maxProj, proj);
			}

			double length = std::hypot(maxx - minx, maxy - miny) * 2.0;
			for (double t = minProj - spacing; t <= maxProj + spacing; t += spacing)
			{
				double cx = vx * t;
				double cy = vy * t;
				double hx = ux * (length * 0.5);
				double hy = uy * (length * 0.5);
				double p1x = cx - hx, p1y = cy - hy;
				double p2x = cx + hx, p2y = cy + hy;
				double half = lineThickness * 0.5;
				double pvx = -uy, pvy = ux;
				double rx = pvx * half, ry = pvy * half;

				PolygonD rect;
				rect.emplace_back(Point2D{ p1x + rx, p1y + ry });
				rect.emplace_back(Point2D{ p2x + rx, p2y + ry });
				rect.emplace_back(Point2D{ p2x - rx, p2y - ry });
				rect.emplace_back(Point2D{ p1x - rx, p1y - ry });

				Polygon rectI = Integerization(rect);
				Polygons clipped = Intersection(poly, Polygons{ rectI });

				std::vector<std::pair<Point2D, Point2D>> segs;
				for (auto& c : clipped)
				{
					PolygonD pc = UnIntegerization(c);
					if (pc.empty()) continue;
					double s_min = 1e300, s_max = -1e300;
					double p_sum = 0; int cnt = 0;
					for (auto& v : pc)
					{
						double s = v.x * ux + v.y * uy;
						double p = v.x * vx + v.y * vy;
						s_min = std::min(s_min, s);
						s_max = std::max(s_max, s);
						p_sum += p; ++cnt;
					}
					if (cnt == 0) continue;
					double p_avg = p_sum / cnt;
					Point2D a{ ux * s_min + vx * p_avg, uy * s_min + vy * p_avg };
					Point2D b{ ux * s_max + vx * p_avg, uy * s_max + vy * p_avg };
					// ensure ordering a->b along u
					if ((a.x * ux + a.y * uy) > (b.x * ux + b.y * uy)) std::swap(a, b);
					segs.emplace_back(a, b);
				}

				// sort segments by their s_min
				std::sort(segs.begin(), segs.end(), [&](const auto& A, const auto& B) {
					double sa = A.first.x * ux + A.first.y * uy;
					double sb = B.first.x * ux + B.first.y * uy;
					return sa < sb;
					});

				rows.push_back(std::move(segs));
			}
			return rows;
		}

		Polygons OffsetOnly(const Polygons poly, double delta, size_t inner,
			size_t outer, Clipper2Lib::JoinType join_type, std::pair<Polygons, Polygons>& out_inner_outer)
		{
			Polygons res;
			if (delta == 0 || (poly.empty() && inner == 0 && outer == 0)) 
				return res;
			int step = 0;
			bool inner_done = (inner == 0);
			bool outer_done = (outer == 0);
			while (true)
			{
				double cur_delta = delta + step * delta;
				if (inner_done && outer_done) 
					break;
				Polygons offs_inner, offs_outer;
				if(!inner_done)
					offs_inner = Offset(poly, -cur_delta, join_type, Clipper2Lib::EndType::Polygon);
				if(!outer_done)
					offs_outer = Offset(poly, cur_delta, join_type, Clipper2Lib::EndType::Polygon);
				if (offs_inner.empty() && offs_outer.empty()) 
					break;
				if (step < inner)
				{
					for (const auto& p : offs_inner) res.emplace_back(p);
				}
				else
				{
					out_inner_outer.first = offs_inner;
					inner_done = true;
				}
				if (step < outer)
				{
					for (const auto& p : offs_outer) res.emplace_back(p);
				}
				else
				{
					out_inner_outer.second = offs_outer;
					outer_done = true;
				}
				++step;
			}
			return res;
		}

		// registered function: generate_fill(poly_table)
		int l_report(lua_State* L,const char* fname, const char* msg)
		{
			std::ostringstream oss;
			oss << "Error in Lua function '" << fname << "': " << msg;
			return luaL_error(L, "%s", oss.str().c_str());
		}

		int l_offsetFill(lua_State* L)
		{
			PolygonsD polyD = LuaTableToPolygonsD(L, 1);
			Polygons poly = Integerization(polyD);
			double spacing = lua_tonumber(L, 2);
			Clipper2Lib::JoinType join_type = Clipper2Lib::JoinType::Square;
			if (lua_getfield(L, 3, "join_type") == LUA_TSTRING)
			{
				const char* jt_str = lua_tostring(L, -1);
				if (strcmp(jt_str, "Square") == 0)
					join_type = Clipper2Lib::JoinType::Square;
				else if (strcmp(jt_str, "Bevel") == 0)
					join_type = Clipper2Lib::JoinType::Bevel;
				else if (strcmp(jt_str, "Round") == 0)
					join_type = Clipper2Lib::JoinType::Round;
				else if (strcmp(jt_str, "Miter") == 0)
					join_type = Clipper2Lib::JoinType::Miter;
			}
			Polygons res = OffsetFill(poly, spacing, join_type);
			PolygonsD resD;
			for (const auto& p : res)
			{
				resD.emplace_back(UnIntegerization(p));
			}
			PushPolygonsDToLua(L, resD);
			return 1;
		}

		int l_lineFill(lua_State* L)
		{
			PolygonsD polyD = LuaTableToPolygonsD(L, 1);
			Polygons poly = Integerization(polyD);
			double spacing = lua_tonumber(L, 2);
			double angle_deg = lua_tonumber(L, 3);
			double lineThickness = lua_tonumber(L, 4);
			Polygons res = LineFill(poly, spacing, angle_deg, lineThickness);
			PolygonsD resD;
			for (const auto& p : res)
			{
				resD.emplace_back(UnIntegerization(p));
			}
			PushPolygonsDToLua(L, resD);
			return 1;
		}

		int l_simpleZigzagFill(lua_State* L)
		{
			PolygonsD polyD = LuaTableToPolygonsD(L, 1);
			Polygons poly = Integerization(polyD);
			double spacing = lua_tonumber(L, 2);
			double angle_deg = lua_tonumber(L, 3);
			double lineThickness = lua_tonumber(L, 4);
			Polygons res = SimpleZigzagFill(poly, spacing, angle_deg, lineThickness);
			PolygonsD resD;
			for (const auto& p : res)
			{
				resD.emplace_back(UnIntegerization(p));
			}
			PushPolygonsDToLua(L, resD);
			return 1;
		}

		int l_zigzagFill(lua_State* L)
		{
			PolygonsD polyD = LuaTableToPolygonsD(L, 1);
			Polygons poly = Integerization(polyD);
			double spacing = lua_tonumber(L, 2);
			double angle_deg = lua_tonumber(L, 3);
			double lineThickness = lua_tonumber(L, 4);
			Polygons res = ZigzagFill(poly, spacing, angle_deg, lineThickness);
			PolygonsD resD;
			for (const auto& p : res)
			{
				resD.emplace_back(UnIntegerization(p));
			}
			PushPolygonsDToLua(L, resD);
			return 1;
		}

		int l_compositeOffsetFill(lua_State* L)
		{
			PolygonsD polyD = LuaTableToPolygonsD(L, 1);
			Polygons poly = Integerization(polyD);
			double spacing = lua_tonumber(L, 2);
			double offsetStep = lua_tonumber(L, 3);
			int outwardCount = static_cast<int>(lua_tointeger(L, 4));
			int inwardCount = static_cast<int>(lua_tointeger(L, 5));
			FillMode mode = FillMode::Line;
			const char* mode_str = lua_tostring(L, 6);
			if (strcmp(mode_str, "Line") == 0)
				mode = FillMode::Line;
			else if (strcmp(mode_str, "SimpleZigzag") == 0)
				mode = FillMode::SimpleZigzag;
			else if (strcmp(mode_str, "Zigzag") == 0)
				mode = FillMode::Zigzag;
			double angle_deg = lua_tonumber(L, 7);
			double lineThickness = lua_tonumber(L, 8);
			Clipper2Lib::JoinType join_type = Clipper2Lib::JoinType::Square;
			if (lua_getfield(L, 9, "join_type") == LUA_TSTRING)
			{
				const char* jt_str = lua_tostring(L, -1);
				if (strcmp(jt_str, "Square") == 0)
					join_type = Clipper2Lib::JoinType::Square;
				else if (strcmp(jt_str, "Bevel") == 0)
					join_type = Clipper2Lib::JoinType::Bevel;
				else if (strcmp(jt_str, "Round") == 0)
					join_type = Clipper2Lib::JoinType::Round;
				else if (strcmp(jt_str, "Miter") == 0)
					join_type = Clipper2Lib::JoinType::Miter;
			}
			Polygons res;
			res = CompositeOffsetFill(poly, spacing, offsetStep, outwardCount,
				inwardCount, mode, angle_deg, lineThickness, join_type);
			PolygonsD resD;
			for (const auto& p : res)
			{
				resD.emplace_back(UnIntegerization(p));
			}
			PushPolygonsDToLua(L, resD);
			return 1;
		}

		int l_hybridFill(lua_State* L)
		{
			PolygonsD polyD = LuaTableToPolygonsD(L, 1);
			Polygons poly = Integerization(polyD);
			double spacing = lua_tonumber(L, 2);
			double offsetStep = lua_tonumber(L, 3);
			int outwardCount = static_cast<int>(lua_tointeger(L, 4));
			int inwardCount = static_cast<int>(lua_tointeger(L, 5));
			FillMode mode = FillMode::Line;
			const char* mode_str = lua_tostring(L, 6);
			if (strcmp(mode_str, "Line") == 0)
				mode = FillMode::Line;
			else if (strcmp(mode_str, "SimpleZigzag") == 0)
				mode = FillMode::SimpleZigzag;
			else if (strcmp(mode_str, "Zigzag") == 0)
				mode = FillMode::Zigzag;
			double angle_deg = lua_tonumber(L, 7);
			double lineThickness = lua_tonumber(L, 8);
			Clipper2Lib::JoinType join_type = Clipper2Lib::JoinType::Square;
			if (lua_getfield(L, 9, "join_type") == LUA_TSTRING)
			{
				const char* jt_str = lua_tostring(L, -1);
				if (strcmp(jt_str, "Square") == 0)
					join_type = Clipper2Lib::JoinType::Square;
				else if (strcmp(jt_str, "Bevel") == 0)
					join_type = Clipper2Lib::JoinType::Bevel;
				else if (strcmp(jt_str, "Round") == 0)
					join_type = Clipper2Lib::JoinType::Round;
				else if (strcmp(jt_str, "Miter") == 0)
					join_type = Clipper2Lib::JoinType::Miter;
			}
			Polygons res;
			res = HybridFill(poly, spacing, offsetStep, outwardCount,
				inwardCount, mode, angle_deg, lineThickness, join_type);
			PolygonsD resD;
			for (const auto& p : res)
			{
				resD.emplace_back(UnIntegerization(p));
			}
			PushPolygonsDToLua(L, resD);
			return 1;
		}

		int l_offsetOnly(lua_State* L)
		{
			PolygonsD polyD = LuaTableToPolygonsD(L, 1);
			Polygons poly = Integerization(polyD);
			double delta = lua_tonumber(L, 2);
			size_t inner = static_cast<size_t>(lua_tointeger(L, 3));
			size_t outer = static_cast<size_t>(lua_tointeger(L, 4));
			Clipper2Lib::JoinType join_type = Clipper2Lib::JoinType::Square;
			if (lua_getfield(L, 5, "join_type") == LUA_TSTRING)
			{
				const char* jt_str = lua_tostring(L, -1);
				if (strcmp(jt_str, "Square") == 0)
					join_type = Clipper2Lib::JoinType::Square;
				else if (strcmp(jt_str, "Bevel") == 0)
					join_type = Clipper2Lib::JoinType::Bevel;
				else if (strcmp(jt_str, "Round") == 0)
					join_type = Clipper2Lib::JoinType::Round;
				else if (strcmp(jt_str, "Miter") == 0)
					join_type = Clipper2Lib::JoinType::Miter;
			}
			std::pair<Polygons, Polygons> out_inner_outer;
			Polygons res = OffsetOnly(poly, delta, inner, outer, join_type, out_inner_outer);
			PolygonsD resD;
			for (const auto& p : res)
			{
				resD.emplace_back(UnIntegerization(p));
			}
			PushPolygonsDToLua(L, resD);
			// push inner
			PolygonsD innerD;
			for (const auto& p : out_inner_outer.first)
			{
				innerD.emplace_back(UnIntegerization(p));
			}
			PushPolygonsDToLua(L, innerD);
			// push outer
			PolygonsD outerD;
			for (const auto& p : out_inner_outer.second)
			{
				outerD.emplace_back(UnIntegerization(p));
			}
			PushPolygonsDToLua(L, outerD);
			return 3;
		}
		
		const luaL_Reg polygonFillLib[] = {
			{"offsetFill", l_offsetFill},
			{"lineFill", l_lineFill},
			{"simpleZigzagFill", l_simpleZigzagFill},
			{"zigzagFill", l_zigzagFill},
			{"compositeOffsetFill", l_compositeOffsetFill},
			{"hybridFill", l_hybridFill},
			{"offsetOnly", l_offsetOnly},
			{NULL, nullptr}
		};

		void RegisterLuaPolygonFillFunctions(lua_State* L)
		{
			luaL_newlib(L, polygonFillLib);
			lua_setglobal(L, "PolygonFill");
		}
	} // namespace

	Polygons OffsetFill(const Polygons& poly, double spacing, Clipper2Lib::JoinType join_type)
	{
		Polygons res;
		if (spacing <= 0) return res;

		int step = 1;
		while (true)
		{
			double delta = -spacing * step;
			Polygons offs = Offset(poly, delta, join_type, Clipper2Lib::EndType::Polygon);
			if (offs.empty()) break;
			// Add first point to close 
			Polygons offset_path = offs;
			for (auto& p : offset_path) {
				if (!p.empty()) {
					p = ClosePath(p);
				}
			}
			for (const auto& p : offset_path) res.emplace_back(p);
			++step;
			if (step > MAX_FILL_ITERATIONS) break;
		}
		return res;
	}

	// Generate independent straight line segments (each Path has exactly 2 points)
	Polygons LineFill(const Polygons& poly, double spacing, double angle_deg, double /*lineThickness*/)
	{
		double ux, uy;
		PolygonsD polyD;
		auto rows = LineFilling(poly, spacing, angle_deg, 1.0, ux, uy, polyD);
		(void)ux; (void)uy;
		Polygons res;
		for (const auto& r : rows)
		{
			for (const auto& [p0, p1] : r)
			{
				Polygon line;
				line.emplace_back(p0);
				line.emplace_back(p1);
				res.emplace_back(std::move(line));
			}
		}
		return res;
	}

	// Generate a connected zigzag path by connecting centers of line segments across scanlines,
	// then return that path as a single integer polyline (no extrusion performed here).
	Polygons SimpleZigzagFill(const Polygons& poly, double spacing, double angle_deg, double lineThickness)
	{
		Polygons res;
		if (spacing <= 0) return res;

		double ux, uy;
		PolygonsD polyD;
		auto rows = LineFilling(poly, spacing, angle_deg, lineThickness, ux, uy, polyD);

		// helpers
		auto point_inside = [&](const Point2D& pt)->bool {
			Clipper2Lib::Point64 p64{ (int64_t)std::llround(pt.x * integerization), (int64_t)std::llround(pt.y * integerization) };
			auto r = PointInPolygons(p64, poly);
			return r != Clipper2Lib::PointInPolygonResult::IsOutside;
		};

		auto find_first_inside = [&](const Point2D& from, const Point2D& to)->double {
			double lo = 0.0, hi = 1.0;
			if (point_inside(from)) return 0.0;
			if (!point_inside(to)) return 1.0;
			for (int iter = 0; iter < MAX_BINARY_SEARCH_ITERATIONS; ++iter)
			{
				double mid = (lo + hi) * 0.5;
				Point2D p{ from.x + (to.x - from.x) * mid, from.y + (to.y - from.y) * mid };
				if (point_inside(p)) hi = mid; else lo = mid;
			}
			return hi;
		};

		auto find_last_inside = [&](const Point2D& from, const Point2D& to)->double {
			double lo = 0.0, hi = 1.0;
			if (point_inside(to)) return 1.0;
			if (!point_inside(from)) return 0.0;
			for (int iter = 0; iter < MAX_BINARY_SEARCH_ITERATIONS; ++iter)
			{
				double mid = (lo + hi) * 0.5;
				Point2D p{ from.x + (to.x - from.x) * mid, from.y + (to.y - from.y) * mid };
				if (point_inside(p)) lo = mid; else hi = mid;
			}
			return lo;
		};

		auto dump_segment = [&](const Point2D& a, const Point2D& b) {
    		Polygon ln;
    		ln.emplace_back(Point2{(int64_t)std::llround(a.x * integerization),
                          (int64_t)std::llround(a.y * integerization)});
    		ln.emplace_back(Point2{(int64_t)std::llround(b.x * integerization),
                          (int64_t)std::llround(b.y * integerization)});
    		res.emplace_back(std::move(ln));
		};

		// Build polylines, only allowing connectors between the same row or adjacent rows.
		std::vector<std::vector<Point2D>> polylines;
		int current_row = -LARGE_ROW_OFFSET;
		for (size_t r = 0; r < rows.size(); ++r)
		{
			auto &segs = rows[r];
			if (segs.empty()) continue;
			if ((r & 1) == 0)
			{
				for (size_t i = 0; i < segs.size(); ++i)
				{
					const auto& [a,b] = segs[i];
					double dx = b.x - a.x, dy = b.y - a.y;
					double len = std::hypot(dx, dy);
					if (len <= 1e-12) 
						continue;
					double eps = (integerization / INTEGERIZATION_PRECISION) / integerization;
					double sx = dx / len;
					double sy = dy / len;
					Point2D a2{ a.x + sx * eps, a.y + sy * eps };
					Point2D b2{ b.x - sx * eps, b.y - sy * eps };
					double t0 = find_first_inside(a2, b2);
					double t1 = find_last_inside(a2, b2);
					if (t1 <= t0)
					{
						dump_segment(a2, b2);
						continue;
					}
					Point2D aa{ a2.x + (b2.x - a2.x) * t0, a2.y + (b2.y - a2.y) * t0 };
					Point2D bb{ a2.x + (b2.x - a2.x) * t1, a2.y + (b2.y - a2.y) * t1 };

					if (polylines.empty()) 
					{
						polylines.emplace_back();
						polylines.back().push_back(aa);
						polylines.back().push_back(bb);
						current_row = (int)r; 
					}
					else {
						// allow connector only if same row or adjacent row
						if ((int)r == current_row || (int)r == current_row + 1) {
							// append connector by pushing aa then bb
							// avoid duplicate points
							auto &cur = polylines.back();
							if (std::hypot(cur.back().x - aa.x, cur.back().y - aa.y) > 1e-9) 
								cur.push_back(aa);
							cur.push_back(bb);
							current_row = (int)r;
						}
						else {
							// start a new polyline (do not connect across non-adjacent rows)
							polylines.emplace_back();
							polylines.back().push_back(aa);
							polylines.back().push_back(bb);
							current_row = (int)r;
						}
					}
				}
			}
			else
			{
				for (size_t ii = 0; ii < segs.size(); ++ii)
				{
					size_t i = segs.size() - 1 - ii;
					const auto& [a,b] = segs[i];
					double dx = b.x - a.x, dy = b.y - a.y;
					double len = std::hypot(dx, dy);
					if (len <= 1e-12) continue;
					double eps = (integerization / INTEGERIZATION_PRECISION) / integerization;
					double sx = dx / len;
					double sy = dy / len;
					Point2D a2{ a.x + sx * eps, a.y + sy * eps };
					Point2D b2{ b.x - sx * eps, b.y - sy * eps };
					double t0 = find_first_inside(a2, b2);
					double t1 = find_last_inside(a2, b2);
					if (t1 <= t0)
					{
						dump_segment(a2, b2);
						continue;
					}
					Point2D aa{ a2.x + (b2.x - a2.x) * t0, a2.y + (b2.y - a2.y) * t0 };
					Point2D bb{ a2.x + (b2.x - a2.x) * t1, a2.y + (b2.y - a2.y) * t1 };

					if (polylines.empty()) {
						polylines.emplace_back();
						polylines.back().push_back(aa);
						polylines.back().push_back(bb);
						current_row = (int)r; 
					}
					else {
						if ((int)r == current_row || (int)r == current_row + 1) {
							auto &cur = polylines.back();
							if (std::hypot(cur.back().x - aa.x, cur.back().y - aa.y) > 1e-9) cur.push_back(aa);
							cur.push_back(bb);
							current_row = (int)r;
						}
						else {
							polylines.emplace_back();
							polylines.back().push_back(aa);
							polylines.back().push_back(bb);
							current_row = (int)r;
						}
					}
				}
			}
		}

		// convert polylines to integer Polygons
		for (auto &pl : polylines) {
			if (pl.empty()) continue;
			Polygon out; out.reserve(pl.size());
			for (auto &p : pl) out.emplace_back(Point2{ (int64_t)std::llround(p.x * integerization), (int64_t)std::llround(p.y * integerization) });
			res.emplace_back(std::move(out));
		}
		return res;
	}

	Polygons ZigzagFill(const Polygons& poly, double spacing, double angle_deg,
		double lineThickness)
	{
		Polygons res;
		if (spacing <= 0) return res;

		double ux = 0, uy = 0;
		PolygonsD polyD;
		auto rows = LineFilling(poly, spacing, angle_deg, lineThickness, ux, uy, polyD);

		// sort each row by s_min
		for (auto& segs : rows)
		{
			std::sort(segs.begin(), segs.end(), [&](const auto& A, const auto& B) {
				double sa = A.first.x * ux + A.first.y * uy;
				double sb = B.first.x * ux + B.first.y * uy;
				return sa < sb;
			});
		}

		// flatten segments and compute connectivity (islands)
		struct SegInfo { size_t row; size_t idx; Point2D a, b; double s_min, s_max; };
		std::vector<SegInfo> segList;
		for (size_t r = 0; r < rows.size(); ++r)
		{
			for (size_t i = 0; i < rows[r].size(); ++i)
			{
				const auto& [a, b] = rows[r][i];
				double smin = a.x * ux + a.y * uy;
				double smax = b.x * ux + b.y * uy;
				segList.push_back(SegInfo{ r, i, a, b, smin, smax });
			}
		}
		size_t N = segList.size();
		if (N == 0) return res;

		std::vector<size_t> parent(N);
		for (size_t i = 0; i < N; ++i) parent[i] = i;
		std::function<size_t(size_t)> findp = [&](size_t x)->size_t { return parent[x] == x ? x : parent[x] = findp(parent[x]); };
		auto unite = [&](size_t a, size_t b) { size_t pa = findp(a), pb = findp(b); if (pa != pb) parent[pa] = pb; };

		// index mapping
		std::vector<std::vector<size_t>> segIndex(rows.size());
		size_t idx = 0;
		for (size_t r = 0; r < rows.size(); ++r)
		{
			segIndex[r].reserve(rows[r].size());
			for (size_t i = 0; i < rows[r].size(); ++i) segIndex[r].push_back(idx++);
		}

		// connect adjacent rows by interval overlap
		for (size_t r = 0; r + 1 < rows.size(); ++r)
		{
			for (size_t i = 0; i < rows[r].size(); ++i)
			{
				size_t id1 = segIndex[r][i];
				for (size_t j = 0; j < rows[r + 1].size(); ++j)
				{
					size_t id2 = segIndex[r + 1][j];
					double lo = std::max(segList[id1].s_min, segList[id2].s_min);
					double hi = std::min(segList[id1].s_max, segList[id2].s_max);
					if (lo <= hi) unite(id1, id2);
				}
			}
		}

		// components
		std::unordered_map<size_t, int> compMap;
		std::vector<int> compId(N, -1);
		int compCnt = 0;
		for (size_t i = 0; i < N; ++i)
		{
			size_t r = findp(i);
			auto it = compMap.find(r);
			if (it == compMap.end()) { compMap[r] = compCnt; compId[i] = compCnt; compCnt++; }
			else compId[i] = it->second;
		}

		// point inside helper and clamping helpers
		auto point_inside = [&](const Point2D& pt)->bool {
			Clipper2Lib::Point64 p64{ (int64_t)std::llround(pt.x * integerization), (int64_t)std::llround(pt.y * integerization) };
			auto r = PointInPolygons(p64, poly);
			return r != Clipper2Lib::PointInPolygonResult::IsOutside;
		};

		auto find_first_inside = [&](const Point2D& from, const Point2D& to)->double {
			double lo = 0.0, hi = 1.0;
			if (point_inside(from)) return 0.0;
			if (!point_inside(to)) return 1.0;
			for (int iter = 0; iter < MAX_BINARY_SEARCH_ITERATIONS; ++iter)
			{
				double mid = (lo + hi) * 0.5;
				Point2D p{ from.x + (to.x - from.x) * mid, from.y + (to.y - from.y) * mid };
				if (point_inside(p)) hi = mid; else lo = mid;
			}
			return hi;
		};

		auto find_last_inside = [&](const Point2D& from, const Point2D& to)->double {
			double lo = 0.0, hi = 1.0;
			if (point_inside(to)) return 1.0;
			if (!point_inside(from)) return 0.0;
			for (int iter = 0; iter < MAX_BINARY_SEARCH_ITERATIONS; ++iter)
			{
				double mid = (lo + hi) * 0.5;
				Point2D p{ from.x + (to.x - from.x) * mid, from.y + (to.y - from.y) * mid };
				if (point_inside(p)) lo = mid; else hi = mid;
			}
			return lo;
		};

		// re-use build_bridge logic from previous implementation (adapted)
		auto build_bridge = [&](Point2D ca, Point2D cb)->std::vector<Point2D>
		{
			std::vector<Point2D> path;
			if (point_inside(ca) && point_inside(cb)) {
				path = { ca,cb };
				return path;
			}
			const int steps = MAX_BINARY_SEARCH_ITERATIONS;
			Point2D p1, p2; bool ok1 = false, ok2 = false;
			for (int i = 0; i <= steps; i++) {
				double t = double(i) / steps;
				Point2D p{ ca.x * (1 - t) + cb.x * t, ca.y * (1 - t) + cb.y * t };
				if (!ok1 && point_inside(p)) { p1 = p; ok1 = true; }
			}
			for (int i = steps; i >= 0; i--) {
				double t = double(i) / steps;
				Point2D p{ ca.x * (1 - t) + cb.x * t, ca.y * (1 - t) + cb.y * t };
				if (!ok2 && point_inside(p)) { p2 = p; ok2 = true; }
			}
			if (!ok1 || !ok2) return {};
			PolygonD outer = polyD[0];
			if (Clipper2Lib::Area(Integerization(outer)) < 0) std::reverse(outer.begin(), outer.end());
			auto distOnRing = [&](size_t i, size_t j)->double {
				double d = 0;
				for (size_t k = i; k != j; k = (k + 1) % outer.size()) {
					size_t kk = (k + 1) % outer.size();
					d += std::hypot(outer[kk].x - outer[k].x, outer[kk].y - outer[k].y);
				}
				return d;
			};
			size_t i1 = 0, i2 = 0; double best1 = 1e300, best2 = 1e300;
			for (size_t i = 0; i < outer.size(); i++) {
				double d1 = std::hypot(outer[i].x - p1.x, outer[i].y - p1.y);
				double d2 = std::hypot(outer[i].x - p2.x, outer[i].y - p2.y);
				if (d1 < best1) { best1 = d1; i1 = i; }
				if (d2 < best2) { best2 = d2; i2 = i; }
			}
			double dCW = distOnRing(i1, i2);
			double dCCW = distOnRing(i2, i1);
			std::vector<Point2D> arc;
			if (dCW < dCCW) {
				for (size_t k = i1;; k = (k + 1) % outer.size()) { arc.push_back(outer[k]); if (k == i2)break; }
			}
			else {
				for (size_t k = i1;; k = (k + outer.size() - 1) % outer.size()) { arc.push_back(outer[k]); if (k == i2)break; }
			}
			std::vector<Point2D> samp;
			samp.push_back(p1);
			double acc = 0, step = std::min(0.5 * integerization, lineThickness * 2.0);
			for (size_t i = 1; i < arc.size(); i++) {
				acc += std::hypot(arc[i].x - arc[i - 1].x, arc[i].y - arc[i - 1].y);
				if (acc >= step) { acc = 0; samp.push_back(arc[i]); }
			}
			samp.push_back(p2);
			path.push_back(ca);
			for (auto& p : samp) path.push_back(p);
			path.push_back(cb);
			return path;
		};

		auto dump_segment = [&](const Point2D& a, const Point2D& b) {
    		Polygon ln;
    		ln.emplace_back(Point2{(int64_t)std::llround(a.x * integerization),
                          (int64_t)std::llround(a.y * integerization)});
    		ln.emplace_back(Point2{(int64_t)std::llround(b.x * integerization),
                          (int64_t)std::llround(b.y * integerization)});
    		res.emplace_back(std::move(ln));
		};

		// Build polylines only connecting same row or adjacent rows to avoid loops
		std::vector<std::vector<Point2D>> polylines;
		// collect extra straight segments when connector/bridge fails
		std::vector<Polygon> extraLines;
		int prev_row = -LARGE_ROW_OFFSET;
		bool have_prev = false;
		int prev_comp_local = -1;
		double eps = (integerization / INTEGERIZATION_PRECISION) / integerization;

		auto push_seg_to_current = [&](std::vector<Point2D>& pl, const Point2D& aa, const Point2D& bb) {
			if (pl.empty()) { pl.push_back(aa); pl.push_back(bb); return; }
			if (std::hypot(pl.back().x - aa.x, pl.back().y - aa.y) > 1e-9) pl.push_back(aa);
			pl.push_back(bb);
		};

		auto clamp_segment = [&](const Point2D& a, const Point2D& b, Point2D& aa, Point2D& bb)->bool {
			double dx = b.x - a.x, dy = b.y - a.y;
			double len = std::hypot(dx, dy);
			if (len <= 1e-12) return false;
			double sx = dx / len, sy = dy / len;
			Point2D a2{ a.x + sx * eps, a.y + sy * eps };
			Point2D b2{ b.x - sx * eps, b.y - sy * eps };
			double t0 = find_first_inside(a2, b2);
			double t1 = find_last_inside(a2, b2);
			if (t1 <= t0) return false;
			aa = Point2D{ a2.x + (b2.x - a2.x) * t0, a2.y + (b2.y - a2.y) * t0 };
			bb = Point2D{ a2.x + (b2.x - a2.x) * t1, a2.y + (b2.y - a2.y) * t1 };
			return true;
		};

		for (size_t r = 0; r < rows.size(); ++r)
		{
			auto& segs = rows[r];
			if (segs.empty()) continue;
			if ((r & 1) == 0)
			{
				for (size_t i = 0; i < segs.size(); ++i)
				{
					const auto& [a, b] = segs[i];
					size_t idxSeg = segIndex[r][i];
					int cid = compId[idxSeg];
					Point2D aa, bb;
					if (!clamp_segment(a, b, aa, bb))
					{
						dump_segment(aa, bb);
						continue;
					}

					if (polylines.empty()) 
					{ 
						polylines.emplace_back();
						push_seg_to_current(polylines.back(), aa, bb);
						prev_row = (int)r;
						have_prev = true; prev_comp_local = cid;
						continue; 
					}

					// allow connect only if same row or adjacent row
					if ((int)r == prev_row || (int)r == prev_row + 1)
					{
						// if different component and adjacent row, attempt bridge; otherwise simple connect
						if (cid != prev_comp_local && (int)r == prev_row + 1)
						{
							auto bridge = build_bridge(polylines.back().back(), aa);
							if (!bridge.empty())
							{
								// append bridge (skip duplicate start)
								for (size_t k = 1; k < bridge.size(); ++k) polylines.back().push_back(bridge[k]);
								// then append segment
								push_seg_to_current(polylines.back(), aa, bb);
							}
							else
							{
								// fallback: only connect if midpoint inside
								Point2D mid{ (polylines.back().back().x + aa.x) * 0.5, (polylines.back().back().y + aa.y) * 0.5 };
								if (point_inside(mid)) 
								{
									push_seg_to_current(polylines.back(), aa, bb); 
								}
								else 
								{ 
									polylines.emplace_back(); 
									push_seg_to_current(polylines.back(), aa, bb); 
									Polygon line; line.emplace_back(Point2{ (int64_t)std::llround(aa.x * integerization), (int64_t)std::llround(aa.y * integerization) }); line.emplace_back(Point2{ (int64_t)std::llround(bb.x * integerization), (int64_t)std::llround(bb.y * integerization) }); extraLines.push_back(std::move(line));
								}
							}
						}
						else
						{
							push_seg_to_current(polylines.back(), aa, bb);
						}
						prev_row = (int)r; prev_comp_local = cid; have_prev = true;
					}
					else
					{
						// start new polyline if rows are not adjacent
						polylines.emplace_back();
						push_seg_to_current(polylines.back(), aa, bb);
						prev_row = (int)r; prev_comp_local = cid; have_prev = true;
					}
				}
			}
			else
			{
				for (size_t ii = 0; ii < segs.size(); ++ii)
				{
					size_t i = segs.size() - 1 - ii;
					const auto& [a, b] = segs[i];
					size_t idxSeg = segIndex[r][i];
					int cid = compId[idxSeg];
					Point2D aa, bb;
					if (!clamp_segment(b, a, aa, bb))
					{
						dump_segment(aa, bb);
						continue;
					}

					if (polylines.empty()) 
					{ 
						polylines.emplace_back(); 
						push_seg_to_current(polylines.back(), aa, bb); 
						prev_row = (int)r;
						have_prev = true; prev_comp_local = cid; continue; }

					if ((int)r == prev_row || (int)r == prev_row + 1)
					{
						if (cid != prev_comp_local && (int)r == prev_row + 1)
						{
							auto bridge = build_bridge(polylines.back().back(), aa);
							if (!bridge.empty()) 
							{
								for (size_t k = 1; k < bridge.size(); ++k)
									polylines.back().push_back(bridge[k]); 
								push_seg_to_current(polylines.back(), aa, bb); 
							}
								else 
								{ 
									Point2D mid{ (polylines.back().back().x + aa.x) * 0.5, (polylines.back().back().y + aa.y) * 0.5 };
									if (point_inside(mid)) 
									{
										push_seg_to_current(polylines.back(), aa, bb);
									} else
									{ 
										polylines.emplace_back();
										push_seg_to_current(polylines.back(), aa, bb); 
										Polygon line; line.emplace_back(Point2{ (int64_t)std::llround(aa.x * integerization), (int64_t)std::llround(aa.y * integerization) }); line.emplace_back(Point2{ (int64_t)std::llround(bb.x * integerization), (int64_t)std::llround(bb.y * integerization) }); extraLines.push_back(std::move(line));
									} 
								}
						}
						else 
						{
							push_seg_to_current(polylines.back(), aa, bb); 
						}
						prev_row = (int)r; prev_comp_local = cid; have_prev = true;
					}
					else
					{
						push_seg_to_current(polylines.back(), aa, bb);
						prev_row = (int)r;
						prev_comp_local = cid;
						have_prev = true;
					}
				}
			}
		}

		// convert polylines to integer polygons
		for (auto &pl : polylines)
		{
			if (pl.empty()) continue;
			Polygon out; out.reserve(pl.size());
			for (auto &p : pl) out.emplace_back(Point2{ (int64_t)std::llround(p.x * integerization), (int64_t)std::llround(p.y * integerization) });
			res.emplace_back(out);
		}

		// append any extra straight segments that failed to connect
		for (auto &ln : extraLines) res.push_back(ln);
		return res;
	}

	Polygons CompositeOffsetFill(const Polygons& poly, double spacing,
		double offsetStep, int outwardCount, int inwardCount, FillMode mode,
		double angle_deg, double lineThickness, Clipper2Lib::JoinType join_type)
	{
		Polygons res;
		auto fillOne = [&](const Polygons& p)->Polygons 
			{
			switch (mode)
			{
			case FillMode::Line:
				return LineFill(p, spacing, angle_deg, lineThickness);
			case FillMode::SimpleZigzag:
				return SimpleZigzagFill(p, spacing, angle_deg, lineThickness);
			case FillMode::Zigzag:
				return ZigzagFill(p, spacing, angle_deg, lineThickness);
			default:
				return ZigzagFill(p, spacing, angle_deg, lineThickness);
			}
			};

		// base polygon
		Polygons base = fillOne(poly);
		for (auto& b : base) res.push_back(b);

		// outward offsets
		for (int i = 1; i <= outwardCount; ++i)
		{
			double delta = offsetStep * i;
			Polygons offs = Offset(poly, delta, join_type, Clipper2Lib::EndType::Polygon);
			for (auto& op : offs)
			{
				Polygons f = fillOne(Polygons{op});
				for (auto& q : f) res.push_back(q);
			}
		}

		// inward offsets
		for (int i = 1; i <= inwardCount; ++i)
		{
			double delta = -offsetStep * i;
			Polygons offs = Offset(poly, delta, join_type, Clipper2Lib::EndType::Polygon);
			for (auto& op : offs)
			{
				Polygons f = fillOne(Polygons{ op });
				for (auto& q : f) res.push_back(q);
			}
		}

		return res;
	}

	Polygons HybridFill(const Polygons& poly, double spacing,
		double offsetStep, int outwardCount, int inwardCount,
		FillMode mode, double angle_deg,
		double lineThickness,
		Clipper2Lib::JoinType join_type)
	{
		Polygons res;

		for (int i = 0; i < outwardCount; ++i)
		{
			auto offs = Offset(poly, offsetStep * (i + 1), join_type,
				Clipper2Lib::EndType::Polygon);
			for (const auto& p : offs)
				res.emplace_back(ClosePath(p));
		}

		const auto minAreaInt64 = static_cast<int64_t>(std::pow(lineThickness, 2));
		int done = 0;
		for (int i = 1; i <= inwardCount - 1; ++i)
		{
			auto offs = Offset(poly, -offsetStep * i, join_type,
				Clipper2Lib::EndType::Polygon);
			if (offs.empty()) break;                     
			const auto& front = offs.front();
			if (front.size() < 3 ||
				std::abs(Clipper2Lib::Area(front)) < minAreaInt64)
				break;                                  
			for (const auto& p : offs)
				res.emplace_back(ClosePath(p));
			done = i;
		}

		double finalDelta = -offsetStep * (done + 1);
		auto   finalOffs = Offset(poly, finalDelta, join_type,
			Clipper2Lib::EndType::Polygon);

		auto fillOne = [&](const Polygons& p) -> Polygons {
			switch (mode)
			{
			case FillMode::Line:          
				return LineFill(p, spacing, angle_deg, lineThickness);
			case FillMode::SimpleZigzag:  
				return SimpleZigzagFill(p, spacing, angle_deg, lineThickness);
			default:                      
				return ZigzagFill(p, spacing, angle_deg, lineThickness);
			}
			};

		for (const auto& island : finalOffs)
		{
			if (island.size() < 3 ||
				Clipper2Lib::Area(island) < minAreaInt64)
				continue;
			auto f = fillOne(Polygons{ island });
			res.insert(res.end(), f.begin(), f.end());
		}
		return res;
	}

	// LuaCustomFill: call Lua script function to generate table of polylines/polygons
	Polygons LuaCustomFill(const Polygons& poly, const std::string& scriptPath, const std::string& functionName,
		double lineThickness, const std::function<void(lua_State*)>& lua_reg)
	{
		Polygons res;
		// Convert integer polygon to float polygon for Lua
		auto polyD = UnIntegerization(poly);

		auto L = MakeUniqueLuaState();
		if (!L)
			throw RuntimeError("Failed to create Lua state");
		luaL_openlibs(L.get());

		// load register functions
		RegisterLuaPolygonOperations(L.get());
		RegisterLuaPolygonFillFunctions(L.get());
		if (lua_reg) lua_reg(L.get());

		// load script
		if (luaL_loadfile(L.get(), scriptPath.c_str()) || lua_pcall(L.get(), 0, 0, 0))
		{
			throw RuntimeError("Failed to load Lua script: " + std::string(lua_tostring(L.get(), -1)));
		}

		// get function
		lua_getglobal(L.get(), functionName.c_str());
		if (!lua_isfunction(L.get(), -1)) 
		{
			throw RuntimeError("Lua function not found: " + functionName);
		}

		// push polygon argument
		PushPolygonsDToLua(L.get(), polyD);

		// call function with 1 arg, 1 result
		if (lua_pcall(L.get(), 1, 1, 0) != LUA_OK)
		{
			lua_close(L.get());
			throw RuntimeError("Error calling Lua function: " + std::string(lua_tostring(L.get(), -1)));
		}

		// expect table of polylines / polygons
		if (!lua_istable(L.get(), -1))
		{
			throw RuntimeError("Lua function did not return a table");
		}

		Polygons allPieces;

		// iterate array
		lua_pushnil(L.get());
		while (lua_next(L.get(), -2))
		{
			// key at -2, value at -1
			if (lua_istable(L.get(), -1))
			{
				PolygonD outpoly;
				int n = (int)lua_rawlen(L.get(), -1);
				for (int i = 1; i <= n; ++i)
				{
					lua_rawgeti(L.get(), -1, i);
					if (lua_istable(L.get(), -1))
					{
						lua_getfield(L.get(), -1, "x");
						lua_getfield(L.get(), -2, "y");
						double x = lua_isnumber(L.get(), -2) ? lua_tonumber(L.get(), -2) : 0.0;
						double y = lua_isnumber(L.get(), -1) ? lua_tonumber(L.get(), -1) : 0.0;
						outpoly.emplace_back(Point2D{ x, y });
						lua_pop(L.get(), 2);
					}
					lua_pop(L.get(), 1);
				}
				if (!outpoly.empty())
				{
					// convert to integer polygon and add
					Polygon ip = Integerization(outpoly);
					allPieces.push_back(ip);
				}
			}
			lua_pop(L.get(), 1); // pop value, keep key for next
		}

		// return collected pieces (do not union/merge; caller expects paths)
		for (auto& p : allPieces) res.push_back(p);
		return res;
	}

	Polygons LuaCustomFillString(const Polygons& poly,
		const std::string& luaScript,
		const std::string& functionName,
		double lineThickness, const std::function<void(lua_State*)>& lua_reg)
	{
		auto polyD = UnIntegerization(poly);

		auto L = MakeUniqueLuaState();
		if (!L) 
			throw RuntimeError("Failed to create Lua state");
		luaL_openlibs(L.get());

		RegisterLuaPolygonOperations(L.get());
		RegisterLuaPolygonFillFunctions(L.get());
		if (lua_reg) lua_reg(L.get());

		if (luaL_loadstring(L.get(), luaScript.c_str()) != LUA_OK) 
		{
			std::string err = lua_tostring(L.get(), -1);
			throw RuntimeError("Failed to load Lua string: " + err);
		}
		if (lua_pcall(L.get(), 0, 0, 0) != LUA_OK) 
		{          
			std::string err = lua_tostring(L.get(), -1);
			throw RuntimeError("Exec Lua string failed: " + err);
		}

		lua_getglobal(L.get(), functionName.c_str());
		if (!lua_isfunction(L.get(), -1)) 
		{
			throw RuntimeError("Lua function not found: " + functionName);
		}

		PushPolygonsDToLua(L.get(), polyD);
		if (lua_pcall(L.get(), 1, 1, 0) != LUA_OK) 
		{
			std::string err = lua_tostring(L.get(), -1);
			throw RuntimeError("Error calling Lua function: " + err);
		}

		if (!lua_istable(L.get(), -1)) 
		{
			throw RuntimeError("Lua function did not return a table");
		}

		Polygons allPieces;
		lua_pushnil(L.get());
		while (lua_next(L.get(), -2)) 
		{
			if (lua_istable(L.get(), -1)) 
			{
				PolygonD outpoly;
				int n = (int)lua_rawlen(L.get(), -1);
				for (int i = 1; i <= n; ++i) {
					lua_rawgeti(L.get(), -1, i);
					if (lua_istable(L.get(), -1)) {
						lua_getfield(L.get(), -1, "x");
						lua_getfield(L.get(), -2, "y");
						double x = lua_isnumber(L.get(), -2) ? lua_tonumber(L.get(), -2) : 0.0;
						double y = lua_isnumber(L.get(), -1) ? lua_tonumber(L.get(), -1) : 0.0;
						outpoly.emplace_back(Point2D{ x, y });
						lua_pop(L.get(), 2);
					}
					lua_pop(L.get(), 1);
				}
				if (!outpoly.empty()) {
					Polygon ip = Integerization(outpoly);
					allPieces.push_back(ip);
				}
			}
			lua_pop(L.get(), 1);
		}
		return allPieces;
	}

} // namespace HsBa::Slicer
