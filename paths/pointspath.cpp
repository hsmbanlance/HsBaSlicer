#include "pointspath.hpp"

#include <ranges>
#include <format>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>

#include <lua.hpp>
#include "utils/LuaNewObject.hpp"

#include "base/error.hpp"

namespace HsBa::Slicer
{
	namespace {
	inline const char* GcodeTypeToString(HsBa::Slicer::GcodeType t)
	{
		switch (t)
		{
			case GcodeType::G0: return "G0";
			case GcodeType::G1: return "G1";
			case GcodeType::G2: return "G2";
			case GcodeType::G3: return "G3";
			case GcodeType::G17: return "G17";
			case GcodeType::G18: return "G18";
			case GcodeType::G19: return "G19";
			case GcodeType::G20: return "G20";
			case GcodeType::G21: return "G21";
			case GcodeType::G90: return "G90";
			case GcodeType::G91: return "G91";
			default: return "G";
		}
	}
	}

	PointsPath::PointsPath(GCodeUnits units , OutPoints3 p) :
		units_{units},startPoint_{p},points_{}
	{ }

	void PointsPath::push_back(const GPoint& point)
	{
		points_.emplace_back(point);
	}


	std::string PointsPath::ToString() const
	{
		std::ostringstream ss;
		ss << std::fixed << std::setprecision(4);
		// Units header
		if (units_ == GCodeUnits::mm)
			ss << "G21 ; units mm\n";
		else
			ss << "G20 ; units inch\n";
		// Start absolute positioning
		ss << "G90\n";
		// start point move
		ss << "G0 X" << startPoint_.x << " Y" << startPoint_.y << " Z" << startPoint_.z << "\n";

		for (const auto& pt : points_)
		{
			if(pt.type == GcodeType::G0 || pt.type == GcodeType::G1)
			{
				auto code = GcodeTypeToString(pt.type);
				ss << code;
				// For linear moves include XYZ, feed and extrusion
				ss << " ";
				ss << "X" << pt.p1.x << " Y" << pt.p1.y << " Z" << pt.p1.z;
				if (pt.velocity > 0.0f)
					ss << " F" << pt.velocity;
				ss << " E" << std::setprecision(6) << pt.extrusion << std::setprecision(4);
				ss << "\n";
			}
			else if (pt.type == GcodeType::G2 || pt.type == GcodeType::G3)
			{
				auto code = GcodeTypeToString(pt.type);
				ss << code;
				// For arc moves include XYZ, IJK, feed and extrusion
				ss << " ";
				ss << "X" << pt.p1.x << " Y" << pt.p1.y << " Z" << pt.p1.z;
				ss << " I" << pt.center.x << " J" << pt.center.y << " K" << pt.center.z;
				if (pt.velocity > 0.0f)
					ss << " F" << pt.velocity;
				ss << " E" << std::setprecision(6) << pt.extrusion << std::setprecision(4);
				ss << "\n";
			}
			else
			{
				continue;
			}
		}

		return ss.str();
	}

	std::string PointsPath::ToString(std::string_view script) const
	{
		std::string result;
		auto L = MakeUniqueLuaState();
		if (!L) throw RuntimeError("Lua init failed");
		luaL_openlibs(L.get());

		// push points table
		lua_newtable(L.get()); // points table at -1
		int idx = 1;
		for (const auto& pt : points_)
		{
			lua_newtable(L.get()); // point table
			// type
			lua_pushstring(L.get(), GcodeTypeToString(pt.type));
			lua_setfield(L.get(), -2, "type");
			// p1
			lua_newtable(L.get());
			lua_pushnumber(L.get(), pt.p1.x);
			lua_setfield(L.get(), -2, "x");
			lua_pushnumber(L.get(), pt.p1.y);
			lua_setfield(L.get(), -2, "y");
			lua_pushnumber(L.get(), pt.p1.z);
			lua_setfield(L.get(), -2, "z");
			lua_setfield(L.get(), -2, "p1");
			// center
			lua_newtable(L.get());
			lua_pushnumber(L.get(), pt.center.x);
			lua_setfield(L.get(), -2, "x");
			lua_pushnumber(L.get(), pt.center.y);
			lua_setfield(L.get(), -2, "y");
			lua_pushnumber(L.get(), pt.center.z);
			lua_setfield(L.get(), -2, "z");
			lua_setfield(L.get(), -2, "center");
			// velocity and extrusion
			lua_pushnumber(L.get(), pt.velocity);
			lua_setfield(L.get(), -2, "velocity");
			lua_pushnumber(L.get(), pt.extrusion);
			lua_setfield(L.get(), -2, "extrusion");

			// set into points[idx]
			lua_rawseti(L.get(), -2, idx);
			++idx;
		}
		lua_setglobal(L.get(), "points");

		// push startPoint
		lua_newtable(L.get());
		lua_pushnumber(L.get(), startPoint_.x);
		lua_setfield(L.get(), -2, "x");
		lua_pushnumber(L.get(), startPoint_.y);
		lua_setfield(L.get(), -2, "y");
		lua_pushnumber(L.get(), startPoint_.z);
		lua_setfield(L.get(), -2, "z");
		lua_setglobal(L.get(), "startPoint");

		// units
		lua_pushstring(L.get(), units_ == GCodeUnits::mm ? "mm" : "inch");
		lua_setglobal(L.get(), "units");

		// load and run script
		int loadStatus = luaL_loadbuffer(L.get(), script.data(), script.size(), "PointsPathScript");
		if (loadStatus != LUA_OK)
		{
			result = std::string("Lua load error: ") + lua_tostring(L.get(), -1);
			return result;
		}

		int callStatus = lua_pcall(L.get(), 0, LUA_MULTRET, 0);
		if (callStatus != LUA_OK)
		{
			result = std::string("Lua runtime error: ") + lua_tostring(L.get(), -1);
			return result;
		}

		// If the chunk returned a string, take the top of stack
		int nret = lua_gettop(L.get());
		if (nret > 0 && lua_isstring(L.get(), -1))
		{
			size_t len = 0;
			const char* s = lua_tolstring(L.get(), -1, &len);
			result.assign(s, len);
			return result;
		}

		// otherwise try global 'result'
		lua_getglobal(L.get(), "result");
		if (lua_isstring(L.get(), -1))
		{
			size_t len = 0;
			const char* s = lua_tolstring(L.get(), -1, &len);
			result.assign(s, len);
		}
		else
		{
			result = std::string();
		}

		return result;
	}

	void PointsPath::Save(const std::filesystem::path& p) const
	{
		auto txt = ToString();
		std::ofstream ofs(p, std::ios::binary);
		ofs << txt;
	}

	void PointsPath::Save(const std::filesystem::path& p, std::string_view script) const
	{
		auto txt = ToString(script);
		std::ofstream ofs(p, std::ios::binary);
		ofs << txt;
	}

	std::string PointsPath::ToString(std::string_view script, std::string_view funcName) const
	{
		auto L = MakeUniqueLuaState();
		if (!L) throw RuntimeError("Lua init failed");
		luaL_openlibs(L.get());
		// push points as global
		lua_newtable(L.get());
		int idx = 1;
		for (const auto& pt : points_)
		{
			lua_newtable(L.get()); // point table
			// type
			lua_pushstring(L.get(), GcodeTypeToString(pt.type));
			lua_setfield(L.get(), -2, "type");
			// p1
			lua_newtable(L.get());
			lua_pushnumber(L.get(), pt.p1.x);
			lua_setfield(L.get(), -2, "x");
			lua_pushnumber(L.get(), pt.p1.y);
			lua_setfield(L.get(), -2, "y");
			lua_pushnumber(L.get(), pt.p1.z);
			lua_setfield(L.get(), -2, "z");
			lua_setfield(L.get(), -2, "p1");
			// center
			lua_newtable(L.get());
			lua_pushnumber(L.get(), pt.center.x);
			lua_setfield(L.get(), -2, "x");
			lua_pushnumber(L.get(), pt.center.y);
			lua_setfield(L.get(), -2, "y");
			lua_pushnumber(L.get(), pt.center.z);
			lua_setfield(L.get(), -2, "z");
			lua_setfield(L.get(), -2, "center");
			// velocity and extrusion
			lua_pushnumber(L.get(), pt.velocity);
			lua_setfield(L.get(), -2, "velocity");
			lua_pushnumber(L.get(), pt.extrusion);
			lua_setfield(L.get(), -2, "extrusion");

			// set into points[idx]
			lua_rawseti(L.get(), -2, idx);
			++idx;
		}
		lua_setglobal(L.get(), "points");
		// push startPoint
		lua_newtable(L.get());
		lua_pushnumber(L.get(), startPoint_.x);
		lua_setfield(L.get(), -2, "x");
		lua_pushnumber(L.get(), startPoint_.y);
		lua_setfield(L.get(), -2, "y");
		lua_pushnumber(L.get(), startPoint_.z);
		lua_setfield(L.get(), -2, "z");
		lua_setglobal(L.get(), "startPoint");
		// units
		lua_pushstring(L.get(), units_ == GCodeUnits::mm ? "mm" : "inch");
		lua_setglobal(L.get(), "units");
		// call function
		lua_getglobal(L.get(), funcName.data());
		if (!lua_isfunction(L.get(), -1))
		{
			throw RuntimeError("Lua function '" + std::string(funcName) + "' not found");
		}
		int callStatus = lua_pcall(L.get(), 0, LUA_MULTRET, 0);
		if (callStatus != LUA_OK)
		{
			std::string err = lua_tostring(L.get(), -1);
			throw RuntimeError(std::format("-- Lua runtime error: {}" , err));
		}
		std::string result;
		// If the function returned a string, take the top of stack
		int nret = lua_gettop(L.get());
		if (nret > 0 && lua_isstring(L.get(), -1))
		{
			size_t len = 0;
			const char* s = lua_tolstring(L.get(), -1, &len);
			result.assign(s, len);
			return result;
		}
		// otherwise try global 'result'
		lua_getglobal(L.get(), "result");
		if (lua_isstring(L.get(), -1))
		{
			size_t len = 0;
			const char* s = lua_tolstring(L.get(), -1, &len);
			result.assign(s, len);
		}
		else
		{
			result = std::string();
		}
		return result;
	}

	void PointsPath::Save(const std::filesystem::path& path, std::string_view script, std::string_view funcName) const
	{
		auto txt = ToString(script, funcName);
		std::ofstream ofs(path, std::ios::binary);
		ofs << txt;
	}

	std::string PointsPath::ToString(const std::filesystem::path& script_file, const std::string_view funcName) const
	{
		std::ifstream ifs(script_file, std::ios::binary);
		if (!ifs)
			throw RuntimeError("Failed to open Lua script file: " + script_file.string());
		std::stringstream buffer;
		buffer << ifs.rdbuf();
		auto script = buffer.str();
		return ToString(std::string_view{script}, funcName);
	}

	void PointsPath::Save(const std::filesystem::path& path, const std::filesystem::path& script_file, std::string_view funcName) const
	{
		auto txt = ToString(script_file, funcName);
		std::ofstream ofs(path, std::ios::binary);
		ofs << txt;
	}

} // namespace HsBa::Slicer
