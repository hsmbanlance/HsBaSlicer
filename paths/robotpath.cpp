#include "robotpath.hpp"

#include <sstream>
#include <iomanip>
#include <string>
#include <fstream>
#include <format>

#include <lua.hpp>
#include "utils/LuaNewObject.hpp"

#include "base/error.hpp"

namespace HsBa::Slicer
{
namespace {
    const char* RLPointTypeToString(RLPointType t)
    {
        switch (t)
        {
        case RLPointType::MoveJ: return "MoveJ";
        case RLPointType::MoveL: return "MoveL";
        case RLPointType::MoveC: return "MoveC";
		case RLPointType::ProgramLStart: return "ProgramLStart";
		case RLPointType::ProgramStart: return "ProgramStart";
		case RLPointType::ProgramCStart: return "ProgramCStart";
		case RLPointType::ProgramL: return "ProgramL";
		case RLPointType::ProgramC: return "ProgramC";
		case RLPointType::ProgramLEnd: return "ProgramLEnd";
		case RLPointType::ProgramCEnd: return "ProgramCEnd";
        default: return "Unknown";
        }
    }
}
}

namespace HsBa::Slicer
{
	RobotPath::RobotPath(RLType robotType, OutPoints3 startPoint, std::string startProgramFunc, std::string endProgramFunc) 
		: robotType_{robotType}, points_{}, startPoint_{startPoint}, startProgramFunc_{startProgramFunc}, endProgramFunc_{endProgramFunc}
	{
	}

	void RobotPath::push_back(const RLPoint& point)
	{
		points_.emplace_back(point);
	}

	RLType RobotPath::getRobotType() const
	{
		return robotType_;
	}

	void RobotPath::Save(const std::filesystem::path& p) const
	{
		auto txt = ToString();
		std::ofstream ofs(p, std::ios::binary);
		ofs << txt;
	}

	void RobotPath::Save(const std::filesystem::path& p, std::string_view script) const
	{
		auto txt = ToString(script);
		std::ofstream ofs(p, std::ios::binary);
		ofs << txt;
	}

	std::string RobotPath::ToString() const
	{
		std::ostringstream ss;
		ss << "# RobotPath default export\n";
		switch (robotType_)
		{
		case RLType::Abb:
			ss << "! Robot: ABB\n";
			ss << GenerateAbbCode();
			break;
		case RLType::Kuka:
			ss << "# Robot: KUKA\n";
			ss << GenerateKukaCode();
			break;
		case RLType::Fanuc:
			ss << "# Robot: FANUC\n";
			ss << GenerateFanucCode();
			break;
		default:
			throw NotSupportedError("Not support robot, please use lua script");
		}
		return ss.str();
	}

	std::string RobotPath::ToString(std::string_view script) const
	{
		// When script provided: prepend header that declares script is user-provided and robot type is ignored
		const std::string header = "// Script provided by user - robot type ignored, remove this line when using real robots\n";

        auto L = MakeUniqueLuaState();
        if (!L) throw RuntimeError("Lua init failed");
        luaL_openlibs(L.get());

        // push header
        lua_pushstring(L.get(), header.c_str());
        lua_setglobal(L.get(), "header");

        // push program function names
        lua_pushstring(L.get(), startProgramFunc_.c_str());
        lua_setglobal(L.get(), "startProgramFunc");
        lua_pushstring(L.get(), endProgramFunc_.c_str());
        lua_setglobal(L.get(), "endProgramFunc");

        // push points
        lua_newtable(L.get());
        int idx = 1;
        for (const auto& pt : points_)
        {
            lua_newtable(L.get()); // point
            // end
            lua_newtable(L.get());
            lua_pushnumber(L.get(), pt.end.x); lua_setfield(L.get(), -2, "x");
            lua_pushnumber(L.get(), pt.end.y); lua_setfield(L.get(), -2, "y");
            lua_pushnumber(L.get(), pt.end.z); lua_setfield(L.get(), -2, "z");
            lua_setfield(L.get(), -2, "end");
            // middle
            lua_newtable(L.get());
            lua_pushnumber(L.get(), pt.middle.x); lua_setfield(L.get(), -2, "x");
            lua_pushnumber(L.get(), pt.middle.y); lua_setfield(L.get(), -2, "y");
            lua_pushnumber(L.get(), pt.middle.z); lua_setfield(L.get(), -2, "z");
            lua_setfield(L.get(), -2, "middle");
            // velocity and type
            lua_pushnumber(L.get(), pt.velocity); lua_setfield(L.get(), -2, "velocity");
            lua_pushstring(L.get(), RLPointTypeToString(pt.type)); lua_setfield(L.get(), -2, "type");
            // set into points
            lua_rawseti(L.get(), -2, idx);
            ++idx;
        }
        lua_setglobal(L.get(), "points");

        // push startPoint
        lua_newtable(L.get());
        lua_pushnumber(L.get(), startPoint_.x); lua_setfield(L.get(), -2, "x");
        lua_pushnumber(L.get(), startPoint_.y); lua_setfield(L.get(), -2, "y");
        lua_pushnumber(L.get(), startPoint_.z); lua_setfield(L.get(), -2, "z");
        lua_setglobal(L.get(), "startPoint");

        // execute script
        int loadStatus = luaL_loadbuffer(L.get(), script.data(), script.size(), "RobotPathScript");
        if (loadStatus != LUA_OK)
        {
            std::string err = lua_tostring(L.get(), -1);
            throw RuntimeError(std::format("-- Lua load error: {}", err));
        }

        int callStatus = lua_pcall(L.get(), 0, LUA_MULTRET, 0);
        if (callStatus != LUA_OK)
        {
            std::string err = lua_tostring(L.get(), -1);
            throw RuntimeError(std::format("-- Lua runtime error: {}" , err));
        }

        // if returned a string, take it
        int nret = lua_gettop(L.get());
        std::string body;
        if (nret > 0 && lua_isstring(L.get(), -1))
        {
            size_t len = 0;
            const char* s = lua_tolstring(L.get(), -1, &len);
            body.assign(s, len);
            return body;
        }

        lua_getglobal(L.get(), "result");
        if (lua_isstring(L.get(), -1))
        {
            size_t len = 0;
            const char* s = lua_tolstring(L.get(), -1, &len);
            body.assign(s, len);
        }
        else
        {
            body = "";
        }
		return body;
	}

	std::string RobotPath::GenerateAbbCode() const
	{

		// simple ABB-like textual representation
		std::ostringstream ss;
		ss << "! default z10 for not in program and fine for programing\n";
		ss << "! default workjob1 and tooldata1\n";
		// define modules
		ss << "MODULE mainModule\n";
		ss << "  PROC main()\n";
		// start point
		ss << "    MOVEJ " << std::fixed << std::setprecision(4)
			<< "[" << startPoint_.x << "," << startPoint_.y << "," << startPoint_.z << ",0.0,0.0,0.0]"
			<< " ,v100 ,z10 ,tooldata1\\Wobj=workjob1; !Start Point\n";
		// points
		for (size_t i = 0; i < points_.size(); ++i)
		{
			auto &pt = points_[i];
			if (pt.type == RLPointType::ProgramLStart || pt.type == RLPointType::ProgramStart ||
				pt.type == RLPointType::ProgramCStart)
			{
				ss << std::format("    ! Start of Program Segment {}\n", pt.programIndex);
				ss << "    " << startProgramFunc_ << ";\n";
			}
			else if (pt.type == RLPointType::ProgramLEnd || pt.type == RLPointType::ProgramCEnd)
			{
				ss << std::format("    ! End of Program Segment {}\n", pt.programIndex);
				ss << "    " << endProgramFunc_ << ";\n";
			}
			switch (pt.type)
			{
			case RLPointType::MoveJ:
				ss << "    MOVEJ ";
				ss << "[" << pt.end.x << "," << pt.end.y << "," << pt.end.z << ",0.0,0.0,0.0]";
				// speed bracket: [velocity,50,500,1000]
				ss << " ,[" << pt.velocity << ",50,500,1000] ,z10 ,tooldata1\\Wobj=workjob1;\n";
				break;
			case RLPointType::MoveL:
				ss << "    MOVEL ";
				ss << "[" << pt.end.x << "," << pt.end.y << "," << pt.end.z << ",0.0,0.0,0.0]";
				ss << " ,[" << pt.velocity << ",50,500,1000] ,z10 ,tooldata1\\Wobj=workjob1;\n";
				break;
			case RLPointType::ProgramLStart:
			case RLPointType::ProgramL:
			case RLPointType::ProgramLEnd:
				ss << "    MOVEL ";
				ss << "[" << pt.end.x << "," << pt.end.y << "," << pt.end.z << ",0.0,0.0,0.0]";
				ss << " ,[" << pt.velocity << ",50,500,1000] ,fine ,tooldata1\\Wobj=workjob1";
				ss << std::format("; ! Program Point {}\n", pt.programIndex);
				ss << "\n";
				break;
			case RLPointType::MoveC:
				ss << "    MOVEC ";
				ss << "[" << pt.middle.x << "," << pt.middle.y << "," << pt.middle.z << ",0.0,0.0,0.0], "
					<< "[" << pt.end.x << "," << pt.end.y << "," << pt.end.z << ",0.0,0.0,0.0]"
					<< " ,[" << pt.velocity << ",50,500,1000] ,z10 ,tooldata1\\Wobj=workjob1;\n";
				break;
			case RLPointType::ProgramCStart:
			case RLPointType::ProgramC:
			case RLPointType::ProgramCEnd:
				ss << "    MOVEC ";
				ss << "[" << pt.middle.x << "," << pt.middle.y << "," << pt.middle.z << ",0.0,0.0,0.0], "
					<< "[" << pt.end.x << "," << pt.end.y << "," << pt.end.z << ",0.0,0.0,0.0]"
					<< " ,[" << pt.velocity << ",50,500,1000] ,fine ,tooldata1\\Wobj=workjob1;";
				ss << std::format(" ! Program Point {}\n", pt.programIndex);
				ss << "\n";
				break;
			default:
				ss << "    ! Unsupported point type: " << RLPointTypeToString(pt.type) << "\n";
				continue;
			}
		}
		ss << "  ENDPROC\n";
		ss << "ENDMODULE\n";
		return ss.str();
	}

	std::string RobotPath::GenerateKukaCode() const
	{
		std::ostringstream ss;
		ss << "; KUKA simple export\n";
		ss << "DEF main()\n";
		ss << "  ; start P[0]\n";
		ss << "  P[0]:=\"Start\"\n";
		for (size_t i = 0; i < points_.size(); ++i)
		{
			auto &pt = points_[i];
			ss << "  ; " << RLPointTypeToString(pt.type) << " to (" << pt.end.x << "," << pt.end.y << "," << pt.end.z << ")\n";
			if (pt.type == RLPointType::ProgramLStart || pt.type == RLPointType::ProgramStart ||
				pt.type == RLPointType::ProgramCStart)
			{
				ss << "  ; Start of Program Segment " << pt.programIndex << "\n";
				ss << "  " << startProgramFunc_ << "\n";
			}
			else if (pt.type == RLPointType::ProgramLEnd || pt.type == RLPointType::ProgramCEnd)
			{
				ss << "  ; End of Program Segment " << pt.programIndex << "\n";
				ss << "  " << endProgramFunc_ << "\n";
			}
			// if this point is a circular move (use middle as via point)
			if (pt.type == RLPointType::MoveC || pt.type == RLPointType::ProgramCStart || pt.type == RLPointType::ProgramC || pt.type == RLPointType::ProgramCEnd)
			{
				ss << "  CIRC {X " << pt.middle.x << ", Y " << pt.middle.y << ", Z " << pt.middle.z << ", A 0, B 0, C 0} {X "
				   << pt.end.x << ", Y " << pt.end.y << ", Z " << pt.end.z << ", A 0, B 0, C 0} C_DIS ;";
			}
			else
			{
				ss << "  LIN {X " << pt.end.x << ", Y " << pt.end.y << ", Z " << pt.end.z << ", A 0, B 0, C 0} C_DIS ;";
			}
			if (pt.type == RLPointType::ProgramLStart || pt.type == RLPointType::ProgramStart || pt.type == RLPointType::ProgramCStart ||
				pt.type == RLPointType::ProgramL || pt.type == RLPointType::ProgramC ||
				pt.type == RLPointType::ProgramLEnd || pt.type == RLPointType::ProgramCEnd)
			{
				ss << "  ; Program Point " << pt.programIndex << "\n";
			}
			else
			{
				ss << "\n";
			}
		}
		ss << "END\n";
		return ss.str();
	}

	std::string RobotPath::GenerateFanucCode() const
	{
		std::ostringstream ss;
		ss << "; FANUC simple export\n";
		ss << "PR[1]=\"Start\"\n";
		for (size_t i = 0; i < points_.size(); ++i)
		{
			auto &pt = points_[i];
			ss << "  ! " << RLPointTypeToString(pt.type) << " to (" << pt.end.x << "," << pt.end.y << "," << pt.end.z << ")\n";
			if (pt.type == RLPointType::ProgramLStart || pt.type == RLPointType::ProgramStart ||
				pt.type == RLPointType::ProgramCStart)
			{
				ss << "  ! Start of Program Segment " << pt.programIndex << "\n";
				ss << "  " << startProgramFunc_ << "\n";
			}
			else if (pt.type == RLPointType::ProgramLEnd || pt.type == RLPointType::ProgramCEnd)
			{
				ss << "  ! End of Program Segment " << pt.programIndex << "\n";
				ss << "  " << endProgramFunc_ << "\n";
			}
			if (pt.type == RLPointType::MoveC || pt.type == RLPointType::ProgramCStart || pt.type == RLPointType::ProgramC || pt.type == RLPointType::ProgramCEnd)
			{
				// Fanuc ARC: use middle as via point, end as target. Controller syntax varies between models;
				// we emit a conservative ARC line; user may need to adapt for their controller.
				ss << "  ARC P_VIA P_END 100% FINE ;  ! via=(" << pt.middle.x << "," << pt.middle.y << "," << pt.middle.z << ") end=(" << pt.end.x << "," << pt.end.y << "," << pt.end.z << ")";
			}
			else
			{
				ss << "  J P[" << i << "] 100% FINE ;";
			}
			if (pt.type == RLPointType::ProgramLStart || pt.type == RLPointType::ProgramStart || pt.type == RLPointType::ProgramCStart ||
				pt.type == RLPointType::ProgramL || pt.type == RLPointType::ProgramC ||
				pt.type == RLPointType::ProgramLEnd || pt.type == RLPointType::ProgramCEnd)
			{
				ss << "  ! Program Point " << pt.programIndex << "\n";
			}
			else
			{
				ss << "\n";
			}
		}
		return ss.str();
	}

	std::string RobotPath::ToString(std::string_view script, std::string_view funcName) const
	{
		auto L = MakeUniqueLuaState();
		if (!L)
			throw RuntimeError("Lua init failed");
		luaL_openlibs(L.get());
		// push function name
		lua_pushstring(L.get(), funcName.data());
		lua_setglobal(L.get(), "funcName");
		// push points
		lua_newtable(L.get());
		int idx = 1;
		for (const auto& pt : points_)
		{
			lua_newtable(L.get()); // point
			// end
			lua_newtable(L.get());
			lua_pushnumber(L.get(), pt.end.x); lua_setfield(L.get(), -2, "x");
			lua_pushnumber(L.get(), pt.end.y); lua_setfield(L.get(), -2, "y");
			lua_pushnumber(L.get(), pt.end.z); lua_setfield(L.get(), -2, "z");
			lua_setfield(L.get(), -2, "end");
			// middle
			lua_newtable(L.get());
			lua_pushnumber(L.get(), pt.middle.x); lua_setfield(L.get(), -2, "x");
			lua_pushnumber(L.get(), pt.middle.y); lua_setfield(L.get(), -2, "y");
			lua_pushnumber(L.get(), pt.middle.z); lua_setfield(L.get(), -2, "z");
			lua_setfield(L.get(), -2, "middle");
			// velocity and type
			lua_pushnumber(L.get(), pt.velocity); lua_setfield(L.get(), -2, "velocity");
			lua_pushstring(L.get(), RLPointTypeToString(pt.type)); lua_setfield(L.get(), -2, "type");
			// set into points
			lua_rawseti(L.get(), -2, idx);
			++idx;
		}
		lua_setglobal(L.get(), "points");
		// execute script
		int loadStatus = luaL_loadbuffer(L.get(), script.data(), script.size(), "RobotPathScriptWithFunc");
		if (loadStatus != LUA_OK)
		{
			std::string err = lua_tostring(L.get(), -1);
			throw RuntimeError(std::format("-- Lua load error: {}", err));
		}
		int callStatus = lua_pcall(L.get(), 0, LUA_MULTRET, 0);
		if (callStatus != LUA_OK)
		{
			std::string err = lua_tostring(L.get(), -1);
			throw RuntimeError(std::format("-- Lua runtime error: {}", err));
		}
		// if returned a string, take it
		int nret = lua_gettop(L.get());
		std::string result;
		if (nret > 0 && lua_isstring(L.get(), -1))
		{
			size_t len = 0;
			const char* s = lua_tolstring(L.get(), -1, &len);
			result.assign(s, len);
			// L will be closed by UniqueLua deleter
			return result;
		}
		lua_getglobal(L.get(), "result");
		if (lua_isstring(L.get(), -1))
		{
			size_t len = 0;
			const char* s = lua_tolstring(L.get(), -1, &len);
			result.assign(s, len);
		}
		else
		{
			result = "";
		}
		// L will be closed by UniqueLua deleter
		return result;
	}

	std::string RobotPath::ToString(const std::filesystem::path& script_file,const std::string_view funcName) const
	{
		std::ifstream ifs(script_file, std::ios::binary);
		if (!ifs)
			throw RuntimeError("Failed to open Lua script file: " + script_file.string());
		std::stringstream buffer;
		buffer << ifs.rdbuf();
		auto script = buffer.str();
		return ToString(std::string_view{script}, funcName);
	}

	void RobotPath::Save(const std::filesystem::path& path, std::string_view script, std::string_view funcName) const
	{
		auto txt = ToString(script, funcName);
		std::ofstream ofs(path, std::ios::binary);
		ofs << txt;
	}

	void RobotPath::Save(const std::filesystem::path& path, const std::filesystem::path& script_file, std::string_view funcName) const
	{
		auto txt = ToString(script_file, funcName);
		std::ofstream ofs(path, std::ios::binary);
		ofs << txt;
	}
} // namespace HsBa::Slicer