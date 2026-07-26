#include "weldrobotpath.hpp"

#include <format>
#include <fstream>
#include <iomanip>
#include <sstream>

#include "utils/LuaNewObject.hpp"
#include <lua.hpp>

#include "base/error.hpp"

namespace HsBa::Slicer
{
namespace
{
const char* WeldProcessToString(WeldProcessType t)
{
    switch (t)
    {
    case WeldProcessType::MIG_MAG:
        return "MIG/MAG";
    case WeldProcessType::TIG:
        return "TIG";
    case WeldProcessType::Plasma:
        return "Plasma";
    case WeldProcessType::Laser:
        return "Laser";
    default:
        return "Unknown";
    }
}

const char* ArcEndToString(ArcEndType t)
{
    switch (t)
    {
    case ArcEndType::Normal:
        return "Normal";
    case ArcEndType::CraterFill:
        return "CraterFill";
    case ArcEndType::SlowRetract:
        return "SlowRetract";
    default:
        return "Normal";
    }
}
}  // namespace

WeldRobotPath::WeldRobotPath(RLType robotType, OutPoints3 startPoint, std::string startProgramFunc,
                             std::string endProgramFunc)
    : RobotPath(robotType, startPoint, std::move(startProgramFunc), std::move(endProgramFunc))
{
}

void WeldRobotPath::push_back(const WeldRLPoint& weldPoint)
{
    weldPoints_.emplace_back(weldPoint);
    // Also push to base class for compatibility
    RobotPath::push_back(weldPoint.point);
}

const std::vector<WeldRLPoint>& WeldRobotPath::weldPoints() const
{
    return weldPoints_;
}

void WeldRobotPath::Save(const std::filesystem::path& p) const
{
    auto txt = ToString();
    std::ofstream ofs(p, std::ios::binary);
    ofs << txt;
}

std::string WeldRobotPath::ToString() const
{
    std::ostringstream ss;
    ss << "# WeldRobotPath export (with welding parameters)\n";
    switch (getRobotType())
    {
    case RLType::Abb:
        ss << "! Robot: ABB (Welding)\n";
        ss << GenerateWeldAbbCode();
        break;
    case RLType::Kuka:
        ss << "; Robot: KUKA (Welding)\n";
        ss << GenerateWeldKukaCode();
        break;
    case RLType::Fanuc:
        ss << "; Robot: FANUC (Welding)\n";
        ss << GenerateWeldFanucCode();
        break;
    default:
        throw NotSupportedError("WeldRobotPath: unsupported robot type, please use lua script");
    }
    return ss.str();
}

std::string WeldRobotPath::ToString(std::string_view script, const std::function<void(lua_State*)>& lua_reg) const
{
    auto L = MakeUniqueLuaState();
    if (!L)
        throw RuntimeError("Lua init failed");
    luaL_openlibs(L.get());
    if (lua_reg)
        lua_reg(L.get());

    // Push header
    const std::string header = "// WeldRobotPath script - robot type ignored, welding params available\n";
    lua_pushstring(L.get(), header.c_str());
    lua_setglobal(L.get(), "header");

    // Push weld points with parameters
    lua_newtable(L.get());
    int idx = 1;
    for (const auto& wpt : weldPoints_)
    {
        lua_newtable(L.get());
        const auto& pt = wpt.point;
        // end position
        lua_newtable(L.get());
        lua_pushnumber(L.get(), pt.end.x);
        lua_setfield(L.get(), -2, "x");
        lua_pushnumber(L.get(), pt.end.y);
        lua_setfield(L.get(), -2, "y");
        lua_pushnumber(L.get(), pt.end.z);
        lua_setfield(L.get(), -2, "z");
        lua_setfield(L.get(), -2, "end");
        // middle position
        lua_newtable(L.get());
        lua_pushnumber(L.get(), pt.middle.x);
        lua_setfield(L.get(), -2, "x");
        lua_pushnumber(L.get(), pt.middle.y);
        lua_setfield(L.get(), -2, "y");
        lua_pushnumber(L.get(), pt.middle.z);
        lua_setfield(L.get(), -2, "z");
        lua_setfield(L.get(), -2, "middle");
        // velocity and type
        lua_pushnumber(L.get(), pt.velocity);
        lua_setfield(L.get(), -2, "velocity");
        lua_pushstring(L.get(), "WeldPoint");
        lua_setfield(L.get(), -2, "type");
        // isWelding flag
        lua_pushboolean(L.get(), wpt.isWelding ? 1 : 0);
        lua_setfield(L.get(), -2, "isWelding");
        // weld parameters
        PushWeldParamToLua(L.get(), wpt.weld);
        lua_setfield(L.get(), -2, "weld");
        // set into table
        lua_rawseti(L.get(), -2, idx);
        ++idx;
    }
    lua_setglobal(L.get(), "points");

    // Execute script
    int loadStatus = luaL_loadbuffer(L.get(), script.data(), script.size(), "WeldRobotPathScript");
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
    return body;
}

void WeldRobotPath::PushWeldParamToLua(lua_State* L, const WeldParam& wp)
{
    lua_newtable(L);
    lua_pushnumber(L, wp.current);
    lua_setfield(L, -2, "current");
    lua_pushnumber(L, wp.voltage);
    lua_setfield(L, -2, "voltage");
    lua_pushnumber(L, wp.wireFeedSpeed);
    lua_setfield(L, -2, "wireFeedSpeed");
    lua_pushnumber(L, wp.gasFlowRate);
    lua_setfield(L, -2, "gasFlowRate");
    lua_pushnumber(L, wp.travelSpeed);
    lua_setfield(L, -2, "travelSpeed");
    lua_pushnumber(L, wp.arcStartCurrent);
    lua_setfield(L, -2, "arcStartCurrent");
    lua_pushnumber(L, wp.arcStartVoltage);
    lua_setfield(L, -2, "arcStartVoltage");
    lua_pushnumber(L, wp.craterFillCurrent);
    lua_setfield(L, -2, "craterFillCurrent");
    lua_pushnumber(L, wp.craterFillVoltage);
    lua_setfield(L, -2, "craterFillVoltage");
    lua_pushstring(L, WeldProcessToString(wp.process));
    lua_setfield(L, -2, "process");
    lua_pushstring(L, ArcEndToString(wp.arcEnd));
    lua_setfield(L, -2, "arcEnd");
    lua_pushinteger(L, wp.weldSchedule);
    lua_setfield(L, -2, "weldSchedule");
}

std::string WeldRobotPath::GenerateWeldAbbCode() const
{
    std::ostringstream ss;
    ss << "! ABB WeldRobotPath - Arc welding instructions\n";
    ss << "! WeldData format: [current, voltage, wire_feed_speed]\n";
    ss << "MODULE WeldModule\n";
    ss << "  CONST welddata wd1 := [200, 22, 10];\n";
    ss << "  CONST seamdata seam1 := [1, 0, 0, 0, 0];\n";
    ss << "  CONST weavedata weave1 := [0, 0, 0, 0, 0, 0, 0];\n\n";
    ss << "  PROC main()\n";

    bool arcActive = false;
    for (size_t i = 0; i < weldPoints_.size(); ++i)
    {
        const auto& wpt = weldPoints_[i];
        const auto& pt = wpt.point;
        const auto& wp = wpt.weld;

        ss << std::fixed << std::setprecision(2);

        if (wpt.isWelding && !arcActive)
        {
            // Arc start
            ss << std::format("    ! Arc Start - {} process, I={:.1f}A U={:.1f}V\n", WeldProcessToString(wp.process),
                              wp.arcStartCurrent, wp.arcStartVoltage);
            ss << std::format("    ArcLStart [{},{},{},0,0,0], [{},{},{},{}], seam1, [{},{},{}], weave1, fine, "
                              "tool1\\Wobj=wobj1;\n",
                              pt.end.x, pt.end.y, pt.end.z, wp.arcStartCurrent, wp.arcStartVoltage, wp.wireFeedSpeed,
                              wp.gasFlowRate, wp.current, wp.voltage, wp.wireFeedSpeed);
            arcActive = true;
        }
        else if (wpt.isWelding && arcActive)
        {
            // Check if this is the last welding point or next is non-welding
            bool nextIsWeld = (i + 1 < weldPoints_.size()) && weldPoints_[i + 1].isWelding;
            if (!nextIsWeld)
            {
                // Arc end
                ss << std::format("    ! Arc End - crater: {}\n", ArcEndToString(wp.arcEnd));
                ss << std::format("    ArcLEnd [{},{},{},0,0,0], [{},{},{}], seam1, [{},{},{}], weave1, fine, "
                                  "tool1\\Wobj=wobj1;\n",
                                  pt.end.x, pt.end.y, pt.end.z, wp.craterFillCurrent, wp.craterFillVoltage,
                                  wp.wireFeedSpeed, wp.gasFlowRate, wp.current, wp.voltage, wp.wireFeedSpeed);
                arcActive = false;
            }
            else
            {
                // Continue welding (ArcL)
                ss << std::format("    ArcL [{},{},{},0,0,0], [{},{},{}], seam1, [{},{},{}], weave1, z10, "
                                  "tool1\\Wobj=wobj1;\n",
                                  pt.end.x, pt.end.y, pt.end.z, wp.current, wp.voltage, wp.wireFeedSpeed,
                                  wp.gasFlowRate, wp.current, wp.voltage, wp.wireFeedSpeed);
            }
        }
        else
        {
            // Non-welding movement
            switch (pt.type)
            {
            case RLPointType::MoveJ:
                ss << std::format("    MoveJ [{},{},{},0,0,0], v{:.0f}, z10, tool1\\Wobj=wobj1;\n", pt.end.x, pt.end.y,
                                  pt.end.z, pt.velocity);
                break;
            case RLPointType::MoveC:
            case RLPointType::ProgramCStart:
            case RLPointType::ProgramC:
            case RLPointType::ProgramCEnd:
                ss << std::format("    MoveC [{},{},{},0,0,0], [{},{},{},0,0,0], v{:.0f}, z10, tool1\\Wobj=wobj1;\n",
                                  pt.middle.x, pt.middle.y, pt.middle.z, pt.end.x, pt.end.y, pt.end.z, pt.velocity);
                break;
            default:
                ss << std::format("    MoveL [{},{},{},0,0,0], v{:.0f}, z10, tool1\\Wobj=wobj1;\n", pt.end.x, pt.end.y,
                                  pt.end.z, pt.velocity);
                break;
            }
        }
    }
    ss << "  ENDPROC\n";
    ss << "ENDMODULE\n";
    return ss.str();
}

std::string WeldRobotPath::GenerateWeldKukaCode() const
{
    std::ostringstream ss;
    ss << "; KUKA WeldRobotPath - Arc welding (KRL + ArcTech)\n";
    ss << "&ACCESS RVP\n";
    ss << "&REL 1\n";
    ss << "&PARAM TEMPLATE = C:\\KRC\\Roboter\\Template\\vorgabe\n";
    ss << "&PARAM EDITMASK = *\n";
    ss << "DEF WeldMain()\n";
    ss << "  ; Initialize welding technology package\n";
    ss << "  ARC_Init()\n\n";

    bool arcActive = false;
    for (size_t i = 0; i < weldPoints_.size(); ++i)
    {
        const auto& wpt = weldPoints_[i];
        const auto& pt = wpt.point;
        const auto& wp = wpt.weld;

        ss << std::fixed << std::setprecision(2);

        if (wpt.isWelding && !arcActive)
        {
            // Set welding parameters and start arc
            ss << std::format("  ; Arc Start - {} I={:.1f}A U={:.1f}V WFS={:.1f}m/min Gas={:.1f}L/min\n",
                              WeldProcessToString(wp.process), wp.current, wp.voltage, wp.wireFeedSpeed,
                              wp.gasFlowRate);
            ss << std::format("  ARC_SetWeldParam(Current:={:.1f}, Voltage:={:.1f}, WireFeed:={:.1f}, "
                              "GasFlow:={:.1f})\n",
                              wp.current, wp.voltage, wp.wireFeedSpeed, wp.gasFlowRate);
            ss << std::format("  ARC_SetStartParam(Current:={:.1f}, Voltage:={:.1f})\n", wp.arcStartCurrent,
                              wp.arcStartVoltage);
            ss << std::format("  LIN {{X {:.2f}, Y {:.2f}, Z {:.2f}, A 0, B 0, C 0}} C_DIS\n", pt.end.x, pt.end.y,
                              pt.end.z);
            ss << "  ARC_On()\n";
            arcActive = true;
        }
        else if (wpt.isWelding && arcActive)
        {
            bool nextIsWeld = (i + 1 < weldPoints_.size()) && weldPoints_[i + 1].isWelding;
            if (!nextIsWeld)
            {
                // Arc end
                ss << std::format("  ; Arc End - crater: {}\n", ArcEndToString(wp.arcEnd));
                ss << std::format("  LIN {{X {:.2f}, Y {:.2f}, Z {:.2f}, A 0, B 0, C 0}} C_DIS\n", pt.end.x, pt.end.y,
                                  pt.end.z);
                if (wp.arcEnd == ArcEndType::CraterFill)
                {
                    ss << std::format("  ARC_SetCraterParam(Current:={:.1f}, Voltage:={:.1f})\n", wp.craterFillCurrent,
                                      wp.craterFillVoltage);
                    ss << "  ARC_CraterFill()\n";
                }
                ss << "  ARC_Off()\n";
                arcActive = false;
            }
            else
            {
                // Continue welding
                ss << std::format("  LIN {{X {:.2f}, Y {:.2f}, Z {:.2f}, A 0, B 0, C 0}} C_DIS\n", pt.end.x, pt.end.y,
                                  pt.end.z);
            }
        }
        else
        {
            // Non-welding movement
            if (pt.type == RLPointType::MoveC || pt.type == RLPointType::ProgramCStart ||
                pt.type == RLPointType::ProgramC || pt.type == RLPointType::ProgramCEnd)
            {
                ss << std::format("  CIRC {{X {:.2f}, Y {:.2f}, Z {:.2f}, A 0, B 0, C 0}}, "
                                  "{{X {:.2f}, Y {:.2f}, Z {:.2f}, A 0, B 0, C 0}} C_DIS\n",
                                  pt.middle.x, pt.middle.y, pt.middle.z, pt.end.x, pt.end.y, pt.end.z);
            }
            else if (pt.type == RLPointType::MoveJ)
            {
                ss << std::format("  PTP {{X {:.2f}, Y {:.2f}, Z {:.2f}, A 0, B 0, C 0}} C_DIS\n", pt.end.x, pt.end.y,
                                  pt.end.z);
            }
            else
            {
                ss << std::format("  LIN {{X {:.2f}, Y {:.2f}, Z {:.2f}, A 0, B 0, C 0}} C_DIS\n", pt.end.x, pt.end.y,
                                  pt.end.z);
            }
        }
    }
    ss << "\n  ARC_Exit()\n";
    ss << "END\n";
    return ss.str();
}

std::string WeldRobotPath::GenerateWeldFanucCode() const
{
    std::ostringstream ss;
    ss << "; FANUC WeldRobotPath - Arc welding TP program\n";
    ss << "; Weld schedules define current/voltage/wire feed\n";
    ss << "/PROG  WELD_MAIN\n";
    ss << "/ATTR\n";
    ss << "OWNER\t\t= MNEDITOR;\n";
    ss << "COMMENT\t\t= \"WeldRobotPath\";\n";
    ss << "PROG_SIZE\t= 0;\n";
    ss << "CREATE\t\t= DATE 25-01-01  TIME 00:00:00;\n";
    ss << "MODIFIED\t= DATE 25-01-01  TIME 00:00:00;\n";
    ss << "FILE_NAME\t= ;\n";
    ss << "VERSION\t\t= 0;\n";
    ss << "LINE_COUNT\t= 0;\n";
    ss << "MEMORY_SIZE\t= 0;\n";
    ss << "PROTECT\t\t= READ_WRITE;\n";
    ss << "TCD:  STACK_SIZE\t= 0,\n";
    ss << "      TASK_PRIORITY\t= 50,\n";
    ss << "      TIME_SLICE\t= 0,\n";
    ss << "      BUSY_LAMP_OFF\t= 0,\n";
    ss << "      ABORT_REQUEST\t= 0,\n";
    ss << "      PAUSE_REQUEST\t= 0;\n";
    ss << "DEFAULT_GROUP\t= 1,*,*,*,*;\n";
    ss << "CONTROL_CODE\t= 00000000 00000000;\n";
    ss << "/MN\n";

    bool arcActive = false;
    int lineNum = 1;
    for (size_t i = 0; i < weldPoints_.size(); ++i)
    {
        const auto& wpt = weldPoints_[i];
        const auto& pt = wpt.point;
        const auto& wp = wpt.weld;

        ss << std::fixed << std::setprecision(2);

        if (wpt.isWelding && !arcActive)
        {
            // Arc start
            ss << std::format("{:>4}:  ; Arc Start - {} Schedule={}\n", lineNum++, WeldProcessToString(wp.process),
                              wp.weldSchedule);
            ss << std::format("{:>4}:  ARC START ;\n", lineNum++);
            ss << std::format("{:>4}:  L P[{}] {:.0f}mm/sec FINE ;\n", lineNum++, i + 1, wp.travelSpeed);
            arcActive = true;
        }
        else if (wpt.isWelding && arcActive)
        {
            bool nextIsWeld = (i + 1 < weldPoints_.size()) && weldPoints_[i + 1].isWelding;
            if (!nextIsWeld)
            {
                // Arc end
                ss << std::format("{:>4}:  ; Arc End - crater: {}\n", lineNum++, ArcEndToString(wp.arcEnd));
                ss << std::format("{:>4}:  L P[{}] {:.0f}mm/sec FINE ;\n", lineNum++, i + 1, wp.travelSpeed);
                ss << std::format("{:>4}:  ARC END[Schedule {}] ;\n", lineNum++, wp.weldSchedule);
                arcActive = false;
            }
            else
            {
                // Continue welding
                ss << std::format("{:>4}:  L P[{}] {:.0f}mm/sec CNT100 ;\n", lineNum++, i + 1, wp.travelSpeed);
            }
        }
        else
        {
            // Non-welding movement
            if (pt.type == RLPointType::MoveJ)
            {
                ss << std::format("{:>4}:  J P[{}] {:.0f}% FINE ;\n", lineNum++, i + 1, pt.velocity);
            }
            else if (pt.type == RLPointType::MoveC || pt.type == RLPointType::ProgramCStart ||
                     pt.type == RLPointType::ProgramC || pt.type == RLPointType::ProgramCEnd)
            {
                ss << std::format("{:>4}:  C P[{}] P[{}] {:.0f}mm/sec FINE ;\n", lineNum++, i, i + 1, pt.velocity);
            }
            else
            {
                ss << std::format("{:>4}:  L P[{}] {:.0f}mm/sec FINE ;\n", lineNum++, i + 1, pt.velocity);
            }
        }
    }
    ss << "/POS\n";
    ss << "; Position data should be taught on actual robot\n";
    ss << "/END\n";
    return ss.str();
}
}  // namespace HsBa::Slicer
