#include "PolygonFill.hpp"

#include <cmath>
#include <lua.hpp>
#include <sstream>
#include <numbers>

#include "base/error.hpp"

namespace HsBa::Slicer
{
	namespace
	{
		Polygon ClosePath(const Polygon& p)
		{
			if (p.empty() || p.front() == p.back()) return p;
			Polygon res = p;
			res.push_back(res.front());
			return res;
		}

		// Helper: convert Polygon to Lua table (array of {x=.., y=..}) pushing it on stack
		void PushPolygonToLua(lua_State* L, const PolygonD& poly)
		{
			lua_newtable(L);
			int idx = 1;
			for (const auto& p : poly)
			{
				lua_newtable(L); // point
				lua_pushnumber(L, p.x); lua_setfield(L, -2, "x");
				lua_pushnumber(L, p.y); lua_setfield(L, -2, "y");
				lua_rawseti(L, -2, idx++);
			}
		}
	}

	Polygons OffsetFill(const Polygon& poly, double spacing, Clipper2Lib::JoinType join_type)
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
			auto offset_path = offs;
			for (auto& p : offset_path) {
				if (!p.empty()) {
					p = ClosePath(p);
				}
			}
			for (const auto& p : offset_path) res.emplace_back(p);
			++step;
			if (step > 10000) break;
		}
		return res;
	}

	// Generate independent straight line segments (each Path has exactly 2 points)
	Polygons LineFill(const Polygon& poly, double spacing, double angle_deg, double /*lineThickness*/)
	{
		Polygons res;
		if (spacing <= 0) return res;

		PolygonD polyD = UnIntegerization(poly);

		double minx = 1e300, miny = 1e300, maxx = -1e300, maxy = -1e300;
		for (auto& pt : polyD)
		{
			minx = std::min(minx, pt.x);
			miny = std::min(miny, pt.y);
			maxx = std::max(maxx, pt.x);
			maxy = std::max(maxy, pt.y);
		}
		if (minx > maxx) return res;

		double ang = angle_deg * std::numbers::pi_v<double> / 180.0;
		double ux = std::cos(ang), uy = std::sin(ang);
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

			// Use a thin rectangle to compute intersections, but then extract segment endpoints
			double half = 1.0; // small thickness to find clipped pieces
			double pvx = -uy, pvy = ux;
			double rx = pvx * half, ry = pvy * half;

			PolygonD rect;
			rect.emplace_back(Point2D{ p1x + rx, p1y + ry });
			rect.emplace_back(Point2D{ p2x + rx, p2y + ry });
			rect.emplace_back(Point2D{ p2x - rx, p2y - ry });
			rect.emplace_back(Point2D{ p1x - rx, p1y - ry });

			Polygon rectI = Integerization(rect);
			Polygons clipped = Intersection(poly, rectI);

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
				Polygon seg;
				seg.emplace_back(Point2{ (int64_t)std::llround(a.x * integerization), (int64_t)std::llround(a.y * integerization) });
				seg.emplace_back(Point2{ (int64_t)std::llround(b.x * integerization), (int64_t)std::llround(b.y * integerization) });
				res.emplace_back(seg);
			}
		}

		return res;
	}

	// Generate a connected zigzag path by connecting centers of line segments across scanlines,
	// then extrude the resulting polyline into thin polygons.
	Polygons SimpleZigzagFill(const Polygon& poly, double spacing, double angle_deg, double lineThickness)
	{
		Polygons res;
		if (spacing <= 0) return res;

		PolygonD polyD = UnIntegerization(poly);

		double minx = 1e300, miny = 1e300, maxx = -1e300, maxy = -1e300;
		for (auto& pt : polyD)
		{
			minx = std::min(minx, pt.x);
			miny = std::min(miny, pt.y);
			maxx = std::max(maxx, pt.x);
			maxy = std::max(maxy, pt.y);
		}
		if (minx > maxx) return res;

		double ang = angle_deg * std::numbers::pi_v<double> / 180.0;
		double ux = std::cos(ang), uy = std::sin(ang);
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

		// collect per-row segments as pairs of endpoints
		std::vector<std::vector<std::pair<Point2D, Point2D>>> rows;
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
			Polygons clipped = Intersection(poly, rectI);

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

		// collect pieces here
		std::vector<Polygon> allPieces;

		// For each segment, extrude to rectangle and intersect with polygon (to ensure inside)
		// We'll clamp segment endpoints so that the resulting center-line lies inside `poly`.
		auto point_inside = [&](const Point2D& pt)->bool {
			// convert to integer point and test against integer polygon `poly`
			Clipper2Lib::Point64 p64{ (int64_t)std::llround(pt.x * integerization), (int64_t)std::llround(pt.y * integerization) };
			auto res = Clipper2Lib::PointInPolygon(p64, poly);
			return res != Clipper2Lib::PointInPolygonResult::IsOutside;
			};

		auto find_first_inside = [&](const Point2D& from, const Point2D& to)->double {
			double lo = 0.0, hi = 1.0;
			// if already inside at from, return 0
			if (point_inside(from)) return 0.0;
			if (!point_inside(to)) return 1.0; // no interior point
			for (int iter = 0; iter < 40; ++iter)
			{
				double mid = (lo + hi) * 0.5;
				Point2D p{ from.x + (to.x - from.x) * mid, from.y + (to.y - from.y) * mid };
				if (point_inside(p)) hi = mid; else lo = mid;
			}
			return hi;
			};

		auto find_last_inside = [&](const Point2D& from, const Point2D& to)->double {
			// returns t in [0,1] as largest t such that point(from + t*(to-from)) is inside
			double lo = 0.0, hi = 1.0;
			if (point_inside(to)) return 1.0;
			if (!point_inside(from)) return 0.0;
			for (int iter = 0; iter < 40; ++iter)
			{
				double mid = (lo + hi) * 0.5;
				Point2D p{ from.x + (to.x - from.x) * mid, from.y + (to.y - from.y) * mid };
				if (point_inside(p)) lo = mid; else hi = mid;
			}
			return lo;
			};

		auto extrude_and_add = [&](const Point2D& a_in, const Point2D& b_in) {
			Point2D a = a_in;
			Point2D b = b_in;
			double dx = b.x - a.x, dy = b.y - a.y;
			double len = std::hypot(dx, dy);
			if (len <= 1e-12) return;

			// clamp the segment to the portion that lies inside the polygon
			double t0 = find_first_inside(a, b);
			double t1 = find_last_inside(a, b);
			if (t1 <= t0) return; // no interior portion
			Point2D aa{ a.x + (b.x - a.x) * t0, a.y + (b.y - a.y) * t0 };
			Point2D bb{ a.x + (b.x - a.x) * t1, a.y + (b.y - a.y) * t1 };
			double ndx = bb.x - aa.x, ndy = bb.y - aa.y;
			double nlen = std::hypot(ndx, ndy);
			if (nlen <= 1e-12) return;

			double half = lineThickness * 0.5;
			double nx = -ndy / nlen * half, ny = ndx / nlen * half;
			PolygonD rect;
			rect.emplace_back(Point2D{ aa.x + nx, aa.y + ny });
			rect.emplace_back(Point2D{ bb.x + nx, bb.y + ny });
			rect.emplace_back(Point2D{ bb.x - nx, bb.y - ny });
			rect.emplace_back(Point2D{ aa.x - nx, aa.y - ny });
			Polygons ip = Integerization(PolygonsD{ rect });
			for (auto& p : ip)
			{
				Polygons clipped = Intersection(poly, p);
				for (auto& c : clipped) allPieces.push_back(c);
			}
			};
		// add segments and connectors in zigzag order
		std::pair<Point2D, Point2D> prev_end; bool has_prev = false;
		for (size_t r = 0; r < rows.size(); ++r)
		{
			auto& segs = rows[r];
			if (segs.empty()) continue;
			if ((r & 1) == 0)
			{
				for (size_t i = 0; i < segs.size(); ++i)
				{
					auto [a, b] = segs[i];
					// add this segment
					extrude_and_add(a, b);
					// connector from prev_end to this segment's start (shrink to avoid exiting polygon)
					if (has_prev)
					{
						extrude_and_add(prev_end.second, a);
					}
					prev_end = { a,b }; has_prev = true;
				}
			}
			else
			{
				for (size_t ii = 0; ii < segs.size(); ++ii)
				{
					size_t i = segs.size() - 1 - ii;
					auto [a, b] = segs[i];
					// add this segment (but direction reversed to maintain path)
					extrude_and_add(b, a);
					if (has_prev)
					{
						extrude_and_add(prev_end.second, b);
					}
					prev_end = { b,a }; has_prev = true;
				}
			}
		}

		// return collected pieces (do not union/merge; caller expects paths)
		for (auto& p : allPieces) res.push_back(p);

		return res;
	}

	Polygons ZigzagFill(const Polygon& poly, double spacing, double angle_deg,
		double lineThickness)
	{
		Polygons res;
		if (spacing <= 0) return res;

		// Build rows same as SimpleZigzagFill
		PolygonD polyD = UnIntegerization(poly);

		double minx = 1e300, miny = 1e300, maxx = -1e300, maxy = -1e300;
		for (auto& pt : polyD)
		{
			minx = std::min(minx, pt.x);
			miny = std::min(miny, pt.y);
			maxx = std::max(maxx, pt.x);
			maxy = std::max(maxy, pt.y);
		}
		if (minx > maxx) return res;

		double ang = angle_deg * std::numbers::pi_v<double> / 180.0;
		double ux = std::cos(ang), uy = std::sin(ang);
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

		// collect per-row segments as pairs of endpoints
		std::vector<std::vector<std::pair<Point2D, Point2D>>> rows;
		for (double t = minProj - spacing; t <= maxProj + spacing; t += spacing)
		{
			double cx = (minx + maxx) * 0.5 + vx * t;
			double cy = (miny + maxy) * 0.5 + vy * t;
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
			Polygons clipped = Intersection(poly, rectI);

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

		// flatten segments into indexable list and build union-find to group into islands
		struct SegInfo { size_t row; size_t idx; Point2D a, b; double s_min, s_max; };
		std::vector<SegInfo> segList;
		for (size_t r = 0; r < rows.size(); ++r)
		{
			auto& segs = rows[r];
			for (size_t i = 0; i < segs.size(); ++i)
			{
				auto [a, b] = segs[i];
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

		// helper: get index of segment (r,i) in segList
		std::vector<std::vector<size_t>> segIndex(rows.size());
		size_t idx = 0; for (size_t r = 0; r < rows.size(); ++r) { segIndex[r].reserve(rows[r].size()); for (size_t i = 0; i < rows[r].size(); ++i) segIndex[r].push_back(idx++); }

		// connect segments between adjacent rows if their s intervals overlap
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

		// map root -> component id
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

		// helper to test point inside polygon
		auto point_inside = [&](const Point2D& pt)->bool {
			Clipper2Lib::Point64 p64{ (int64_t)std::llround(pt.x * integerization), (int64_t)std::llround(pt.y * integerization) };
			auto r = Clipper2Lib::PointInPolygon(p64, poly);
			return r != Clipper2Lib::PointInPolygonResult::IsOutside;
			};

		// eps shrink amount (float units)
		double eps = (integerization / 100.0) / integerization; // 0.01

		// extrude a centerline segment (shorten both ends by eps) and add intersection pieces
		auto extrude_shrink = [&](const Point2D& a_in, const Point2D& b_in) {
			Point2D a = a_in; Point2D b = b_in;
			double dx = b.x - a.x, dy = b.y - a.y;
			double len = std::hypot(dx, dy);
			if (len <= 1e-12) return;
			double sx = dx / len, sy = dy / len;
			a.x += sx * eps; a.y += sy * eps;
			b.x -= sx * eps; b.y -= sy * eps;
			dx = b.x - a.x; dy = b.y - a.y; len = std::hypot(dx, dy);
			if (len <= 1e-12) return;
			double half = lineThickness * 0.5;
			double nx = -dy / len * half, ny = dx / len * half;
			PolygonD rect;
			rect.emplace_back(Point2D{ a.x + nx, a.y + ny });
			rect.emplace_back(Point2D{ b.x + nx, b.y + ny });
			rect.emplace_back(Point2D{ b.x - nx, b.y - ny });
			rect.emplace_back(Point2D{ a.x - nx, a.y - ny });
			Polygons ip = Integerization(PolygonsD{ rect });
			for (auto& p : ip)
			{
				Polygons clipped = Intersection(poly, p);
				for (auto& c : clipped) res.push_back(c);
			}
			};
		auto build_bridge = [&](Point2D ca, Point2D cb)->std::vector<Point2D>
			{
				std::vector<Point2D> path;
				if (point_inside(ca) && point_inside(cb)) {
					path = { ca,cb };          // 完全内部
					return path;
				}
				// 1. 找 p1,p2
				const int steps = 40;
				Point2D p1, p2;
				bool ok1 = false, ok2 = false;
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
				PolygonD outer = polyD;
				if (Clipper2Lib::Area(Integerization(outer)) < 0) std::reverse(outer.begin(), outer.end());
				auto distOnRing = [&](size_t i, size_t j)->double {
					double d = 0;
					for (size_t k = i; k != j; k = (k + 1) % outer.size()) {
						size_t kk = (k + 1) % outer.size();
						d += std::hypot(outer[kk].x - outer[k].x, outer[kk].y - outer[k].y);
					}
					return d;
					};
				size_t i1 = 0, i2 = 0;
				double best1 = 1e300, best2 = 1e300;
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

		// iterate rows building global zigzag order and adding segments/connectors
		std::pair<Point2D, Point2D> prev_end; bool has_prev = false;
		int prev_comp = -1;
		for (size_t r = 0; r < rows.size(); ++r)
		{
			auto& segs = rows[r];
			if (segs.empty()) continue;
			if ((r & 1) == 0)
			{
				for (size_t i = 0; i < segs.size(); ++i)
				{
					auto [a, b] = segs[i];
					size_t idxSeg = segIndex[r][i];
					int cid = compId[idxSeg];
					// add this segment (shrink both ends)
					extrude_shrink(a, b);
					// connector from prev_end to this segment's start
					if (has_prev)
					{
						// always shrink connector ends
						Point2D ca = prev_end.second;
						Point2D cb = a;
						// if connector crosses between different components, ensure midpoint inside
						if (cid != prev_comp)
						{
							auto bridge = build_bridge(ca, cb);
							for (size_t i = 1; i < bridge.size(); i++)
								extrude_shrink(bridge[i - 1], bridge[i]);

						}
						else
						{
							extrude_shrink(ca, cb);
						}
					}
					prev_end = { a,b }; has_prev = true; prev_comp = cid;
				}
			}
			else
			{
				for (size_t ii = 0; ii < segs.size(); ++ii)
				{
					size_t i = segs.size() - 1 - ii;
					auto [a, b] = segs[i];
					size_t idxSeg = segIndex[r][i];
					int cid = compId[idxSeg];
					extrude_shrink(b, a);
					if (has_prev)
					{
						Point2D ca = prev_end.second;
						Point2D cb = b;
						if (cid != prev_comp)
						{
							Point2D mid{ (ca.x + cb.x) * 0.5, (ca.y + cb.y) * 0.5 };
							if (point_inside(mid)) extrude_shrink(ca, cb);
						}
						else
						{
							extrude_shrink(ca, cb);
						}
					}
					prev_end = { b,a }; has_prev = true; prev_comp = cid;
				}
			}
		}

		return res;
	}

	Polygons CompositeOffsetFill(const Polygon& poly, double spacing,
		double offsetStep, int outwardCount, int inwardCount, FillMode mode,
		double angle_deg, double lineThickness, Clipper2Lib::JoinType join_type)
	{
		Polygons res;
		auto fillOne = [&](const Polygon& p)->Polygons {
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
				Polygons f = fillOne(op);
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
				Polygons f = fillOne(op);
				for (auto& q : f) res.push_back(q);
			}
		}

		return res;
	}

	Polygons HybridFill(const Polygon& poly, double spacing,
		double offsetStep, int outwardCount, int inwardCount,
		FillMode mode, double angle_deg,
		double lineThickness,
		Clipper2Lib::JoinType join_type)
	{
		Polygons res;

		/* ---------- 1. 外扩 ---------- */
		for (int i = 0; i < outwardCount; ++i)
		{
			auto offs = Offset(poly, offsetStep * (i + 1), join_type,
				Clipper2Lib::EndType::Polygon);
			for (const auto& p : offs)
				res.emplace_back(ClosePath(p));
		}

		const auto minAreaInt64 = static_cast<int64_t>(std::pow(lineThickness, 2));
		int       done = 0;
		for (int i = 1; i <= inwardCount - 1; ++i)
		{
			auto offs = Offset(poly, -offsetStep * i, join_type,
				Clipper2Lib::EndType::Polygon);
			if (offs.empty()) break;                     // 彻底消失
			const auto& front = offs.front();
			if (front.size() < 3 ||
				std::abs(Clipper2Lib::Area(front)) < minAreaInt64)
				break;                                   // 退化
			for (const auto& p : offs)
				res.emplace_back(ClosePath(p));
			done = i;
		}

		double finalDelta = -offsetStep * (done + 1);
		auto   finalOffs = Offset(poly, finalDelta, join_type,
			Clipper2Lib::EndType::Polygon);

		auto fillOne = [&](const Polygon& p) -> Polygons {
			switch (mode)
			{
			case FillMode::Line:          return LineFill(p, spacing, angle_deg, lineThickness);
			case FillMode::SimpleZigzag:  return SimpleZigzagFill(p, spacing, angle_deg, lineThickness);
			default:                      return ZigzagFill(p, spacing, angle_deg, lineThickness);
			}
			};

		for (const auto& island : finalOffs)
		{
			if (island.size() < 3 ||
				std::abs(Clipper2Lib::Area(island)) < minAreaInt64)
				continue;
			auto f = fillOne(island);
			res.insert(res.end(), f.begin(), f.end());
		}
		return res;
	}

	// LuaCustomFill: call Lua script function to generate table of polylines/polygons
	Polygons LuaCustomFill(const Polygon& poly, const std::string& scriptPath, const std::string& functionName,
		double lineThickness)
	{
		Polygons res;
		// Convert integer polygon to float polygon for Lua
		PolygonD polyD = UnIntegerization(poly);

		lua_State* L = luaL_newstate();
		if (!L)
			throw RuntimeError("Failed to create Lua state");
		luaL_openlibs(L);

		// load script
		if (luaL_loadfile(L, scriptPath.c_str()) || lua_pcall(L, 0, 0, 0))
		{
			lua_close(L);
			throw RuntimeError("Failed to load Lua script: " + std::string(lua_tostring(L, -1)));
		}

		// get function
		lua_getglobal(L, functionName.c_str());
		if (!lua_isfunction(L, -1)) {
			lua_close(L);
			throw RuntimeError("Lua function not found: " + functionName);
		}

		// push polygon argument
		PushPolygonToLua(L, polyD);

		// call function with 1 arg, 1 result
		if (lua_pcall(L, 1, 1, 0) != LUA_OK)
		{
			lua_close(L);
			throw RuntimeError("Error calling Lua function: " + std::string(lua_tostring(L, -1)));
		}

		// expect table of polylines / polygons
		if (!lua_istable(L, -1))
		{
			lua_close(L);
			throw RuntimeError("Lua function did not return a table");
		}

		Polygons allPieces;

		// iterate array
		lua_pushnil(L);
		while (lua_next(L, -2))
		{
			// key at -2, value at -1
			if (lua_istable(L, -1))
			{
				PolygonD outpoly;
				int n = (int)lua_rawlen(L, -1);
				for (int i = 1; i <= n; ++i)
				{
					lua_rawgeti(L, -1, i);
					if (lua_istable(L, -1))
					{
						lua_getfield(L, -1, "x");
						lua_getfield(L, -2, "y");
						double x = lua_isnumber(L, -2) ? lua_tonumber(L, -2) : 0.0;
						double y = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 0.0;
						outpoly.emplace_back(Point2D{ x, y });
						lua_pop(L, 2);
					}
					lua_pop(L, 1);
				}
				if (!outpoly.empty())
				{
					// convert to integer polygon and add
					Polygon ip = Integerization(outpoly);
					allPieces.push_back(ip);
				}
			}
			lua_pop(L, 1); // pop value, keep key for next
		}

		// return collected pieces (do not union/merge; caller expects paths)
		for (auto& p : allPieces) res.push_back(p);

		lua_close(L);
		return res;
	}

} // namespace HsBa::Slicer
