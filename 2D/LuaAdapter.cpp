#include "LuaAdapter.hpp"

#include <format>

#include "base/error.hpp"
#include "2Dhull.hpp"

namespace HsBa::Slicer
{
	namespace
	{
		void l_booleanError(lua_State* L, const char* fname, const char* msg)
		{
			luaL_error(L, std::format("Error in Lua function '{}': {}", fname, msg).c_str());
		}


		int l_booleanOperation(lua_State* L)
		{
			if (lua_gettop(L) != 3 || !lua_istable(L, 1) || !lua_istable(L, 2) || !lua_isstring(L, 3))
				l_booleanError(L, "booleanOperation", "Expected two polygon tables and a string operation name");
			PolygonsD left = LuaTableToPolygonsD(L, 1);
			PolygonsD right = LuaTableToPolygonsD(L, 2);
			std::string operation = lua_tostring(L, 3);
			PolygonsD result;
			if (operation == "union")
				result = Union(left, right);
			else if (operation == "intersection")
				result = Intersection(left, right);
			else if (operation == "difference")
				result = Difference(left, right);
			else if (operation == "xor")
				result = Xor(left, right);
			else
				l_booleanError(L, "booleanOperation", std::format("Unknown operation '{}'", operation).c_str());
			PushPolygonsDToLua(L, result);
			return 1; // return the result table
		}

		int l_union(lua_State* L)
		{
			if (lua_gettop(L) != 2 || !lua_istable(L, 1) || !lua_istable(L, 2))
				l_booleanError(L, "union", "Expected two polygon tables");
			PolygonsD left = LuaTableToPolygonsD(L, 1);
			PolygonsD right = LuaTableToPolygonsD(L, 2);
			PolygonsD result = Union(left, right);
			PushPolygonsDToLua(L, result);
			return 1; // return the result table
		}

		int l_intersection(lua_State* L)
		{
			if (lua_gettop(L) != 2 || !lua_istable(L, 1) || !lua_istable(L, 2))
				l_booleanError(L, "intersection", "Expected two polygon tables");
			PolygonsD left = LuaTableToPolygonsD(L, 1);
			PolygonsD right = LuaTableToPolygonsD(L, 2);
			PolygonsD result = Intersection(left, right);
			PushPolygonsDToLua(L, result);
			return 1; // return the result table
		}

		int l_difference(lua_State* L)
		{
			if (lua_gettop(L) != 2 || !lua_istable(L, 1) || !lua_istable(L, 2))
				l_booleanError(L, "difference", "Expected two polygon tables");
			PolygonsD left = LuaTableToPolygonsD(L, 1);
			PolygonsD right = LuaTableToPolygonsD(L, 2);
			PolygonsD result = Difference(left, right);
			PushPolygonsDToLua(L, result);
			return 1; // return the result table
		}

		int l_xor(lua_State* L)
		{
			if (lua_gettop(L) != 2 || !lua_istable(L, 1) || !lua_istable(L, 2))
				l_booleanError(L, "xor", "Expected two polygon tables");
			PolygonsD left = LuaTableToPolygonsD(L, 1);
			PolygonsD right = LuaTableToPolygonsD(L, 2);
			PolygonsD result = Xor(left, right);
			PushPolygonsDToLua(L, result);
			return 1; // return the result table
		}

		int l_offsetOperation(lua_State* L)
		{
			if (lua_gettop(L) != 3 || !lua_istable(L, 1) || !lua_isnumber(L, 2))
				l_booleanError(L, "offsetOperation", "Expected a polygon table and a number offset");
			Polygons polys = LuaTableToPolygons(L, 1);
			double delta = lua_tonumber(L, 2);
			auto resultI = Offset(polys, delta);
			PushPolygonsToLua(L, resultI);
			return 1; // return the result table
		}

		int l_convexHullOperation(lua_State* L)
		{
			if (lua_gettop(L) != 1 || !lua_istable(L, 1))
				l_booleanError(L, "convexHullOperation", "Expected a polygon table");
			PolygonsD polys = LuaTableToPolygonsD(L, 1);
			PolygonsD result;
			for (const auto& p : polys)
			{
				result.push_back(ConvexHull(p));
			}
			PushPolygonsDToLua(L, result);
			return 1; // return the result table
		}

		int l_concaveHullOperation(lua_State* L)
		{
			if (lua_gettop(L) != 2 || !lua_istable(L, 1) || !lua_isnumber(L, 2))
				l_booleanError(L, "concaveHullOperation", "Expected a polygon table and an integer number of additional points");
			PolygonsD polys = LuaTableToPolygonsD(L, 1);
			int numAdditionalPoints = static_cast<int>(lua_tointeger(L, 2));
			PolygonsD result;
			for (const auto& p : polys)
			{
				result.push_back(ConcaveHullSimulation(p, numAdditionalPoints));
			}
			PushPolygonsDToLua(L, result);
			return 1; // return the result table
		}

		const luaL_Reg booleanLib[] = {
			{"booleanOperation", l_booleanOperation},
			{"union", l_union},
			{"intersection", l_intersection},
			{"difference", l_difference},
			{"xor", l_xor},
			{"offsetOperation", l_offsetOperation},
			{"convexHullOperation", l_convexHullOperation},
			{"concaveHullOperation", l_concaveHullOperation},
			{NULL, NULL}
		};

	} // namespace

	void PushPolygonDToLua(lua_State* L, const PolygonD& poly)
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

	void PushPolygonsDToLua(lua_State* L, const PolygonsD& poly)
	{
		lua_createtable(L, static_cast<int>(poly.size()), 0);
		int idx = 1;
		for (const auto& pt : poly)
		{
			PushPolygonDToLua(L, pt);
			lua_rawseti(L, -2, idx++);
		}
	}

	void PushPolygonToLua(lua_State* L, const Polygon& poly)
	{
		lua_newtable(L);
		int idx = 1;
		for (const auto& p : poly)
		{
			lua_newtable(L); // point
			lua_pushnumber(L, static_cast<double>(p.x) / integerization); lua_setfield(L, -2, "x");
			lua_pushnumber(L, static_cast<double>(p.y) / integerization); lua_setfield(L, -2, "y");
			lua_rawseti(L, -2, idx++);
		}
	}

	void PushPolygonsToLua(lua_State* L, const Polygons& poly)
	{
		lua_createtable(L, static_cast<int>(poly.size()), 0);
		int idx = 1;
		for (const auto& pt : poly)
		{
			PushPolygonToLua(L, pt);
			lua_rawseti(L, -2, idx++);
		}
	}


	// Helper: convert Lua table at given index to PolygonD
	PolygonD LuaTableToPolygonD(lua_State* L, int index)
	{
		PolygonD poly;
		if (!lua_istable(L, index)) return poly;
		size_t len = lua_rawlen(L, index);
		for (size_t i = 1; i <= len; ++i)
		{
			lua_rawgeti(L, index, static_cast<int>(i)); // point table
			if (lua_istable(L, -1))
			{
				lua_getfield(L, -1, "x");
				lua_getfield(L, -2, "y");
				if (lua_isnumber(L, -2) && lua_isnumber(L, -1))
				{
					double x = lua_tonumber(L, -2);
					double y = lua_tonumber(L, -1);
					poly.emplace_back(Point2D{ x, y });
				}
				lua_pop(L, 2); // pop x,y
			}
			lua_pop(L, 1); // pop point table
		}
		return poly;
	}

	// Helper: convert Lua table at given index to PolygonsD
	PolygonsD LuaTableToPolygonsD(lua_State* L, int index)
	{
		PolygonsD polys;
		if (!lua_istable(L, index)) return polys;
		size_t len = lua_rawlen(L, index);
		for (size_t i = 1; i <= len; ++i)
		{
			lua_rawgeti(L, index, static_cast<int>(i)); // polygon table
			PolygonD poly = LuaTableToPolygonD(L, -1);
			if (!poly.empty()) polys.emplace_back(std::move(poly));
			lua_pop(L, 1); // pop polygon table
		}
		return polys;
	}


	Polygon LuaTableToPolygon(lua_State* L, int index)
	{
		Polygon poly;
		if (!lua_istable(L, index)) return poly;
		size_t len = lua_rawlen(L, index);
		for (size_t i = 1; i <= len; ++i)
		{
			lua_rawgeti(L, index, static_cast<int>(i)); // point table
			if (lua_istable(L, -1))
			{
				lua_getfield(L, -1, "x");
				lua_getfield(L, -2, "y");
				if (lua_isnumber(L, -2) && lua_isnumber(L, -1))
				{
					double x = lua_tonumber(L, -2);
					double y = lua_tonumber(L, -1);
					poly.emplace_back(Point2{ static_cast<int64_t>(std::llround(x * integerization)),
											  static_cast<int64_t>(std::llround(y * integerization)) });
				}
				lua_pop(L, 2); // pop x,y
			}
			lua_pop(L, 1); // pop point table
		}
		return poly;
	}

	Polygons LuaTableToPolygons(lua_State* L, int index)
	{
		Polygons polys;
		if (!lua_istable(L, index)) return polys;
		size_t len = lua_rawlen(L, index);
		for (size_t i = 1; i <= len; ++i)
		{
			lua_rawgeti(L, index, static_cast<int>(i)); // polygon table
			Polygon poly = LuaTableToPolygon(L, -1);
			if (!poly.empty()) polys.emplace_back(std::move(poly));
			lua_pop(L, 1); // pop polygon table
		}
		return polys;
	}

	void RegisterLuaPolygonOperations(lua_State* L)
	{
		luaL_newlib(L, booleanLib);
		lua_setglobal(L, "PolygonOperations");
	}
} // namespace HsBa::Slicer