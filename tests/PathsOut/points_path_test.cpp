#define BOOST_TEST_MODULE points_path_test
#include <boost/test/included/unit_test.hpp>

#include "paths/pointspath.hpp"
#include "paths/robotpath.hpp"
#include "base/error.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <lua.hpp>

BOOST_AUTO_TEST_SUITE(points_path_test)

BOOST_AUTO_TEST_CASE(test_gcode_out)
{
    using namespace HsBa::Slicer;

    PointsPath path(GCodeUnits::mm, {0.0f, 0.0f, 0.0f});

    GPoint p;
    p.type = GcodeType::G1;
    p.p1 = {1.0f, 2.0f, 3.0f};
    p.center = {0.0f, 0.0f, 0.0f};
    p.velocity = 1500.0f;
    p.extrusion = 0.123456;

    path.push_back(p);

    auto out = path.ToString();

    // 显式输出结果，便于人工检查/调试
    std::cout << "----- ToString() GCode output -----\n" << out << "\n";

    // basic checks
    BOOST_CHECK_NE(out.find("G21"), std::string::npos);    // units mm
    BOOST_CHECK_NE(out.find("G90"), std::string::npos);    // absolute
    BOOST_CHECK_NE(out.find("G0 X0"), std::string::npos);  // start move
    // check linear move with coords and feed
    BOOST_CHECK_NE(out.find("G1 X1.0000 Y2.0000 Z3.0000"), std::string::npos);
    BOOST_CHECK_NE(out.find("F1500"), std::string::npos);
    BOOST_CHECK_NE(out.find("E0.123456"), std::string::npos);
}

BOOST_AUTO_TEST_CASE(test_script_out)
{
    using namespace HsBa::Slicer;

    PointsPath path(GCodeUnits::mm, {0.0f, 0.0f, 0.0f});

    GPoint p1;
    p1.type = GcodeType::G1;
    p1.p1 = {1.0f, 2.0f, 3.0f};
    p1.velocity = 1200.0f;
    p1.extrusion = 0.5;
    path.push_back(p1);

    GPoint p2;
    p2.type = GcodeType::G0;
    p2.p1 = {4.0f, 5.0f, 6.0f};
    p2.velocity = 0.0f;
    p2.extrusion = 0.0;
    path.push_back(p2);

    // Lua script: produce CSV and return as string
    std::string script = R"lua(
local lines = {}
table.insert(lines, "index,type,x,y,z,velocity,extrusion")
for i,p in ipairs(points) do
  table.insert(lines, string.format("%d,%s,%.4f,%.4f,%.4f,%.3f,%.6f",
    i, p.type, p.p1.x, p.p1.y, p.p1.z, p.velocity or 0.0, p.extrusion or 0.0))
end
return table.concat(lines, "\n")
)lua";

    auto res = path.ToString(script);

    // 显式输出脚本结果
    std::cout << "----- ToString(script) Lua output -----\n" << res << "\n";

    BOOST_CHECK_NE(res.find("index,type,x,y,z,velocity,extrusion"), std::string::npos);
    BOOST_CHECK_NE(res.find("1,G1,1.0000,2.0000,3.0000"), std::string::npos);
    BOOST_CHECK_NE(res.find("2,G0,4.0000,5.0000,6.0000"), std::string::npos);
}

BOOST_AUTO_TEST_CASE(test_robot_outputs)
{
    using namespace HsBa::Slicer;

    // prepare common points
    RLPoint a;
    a.type = RLPointType::MoveJ;
    a.end = {10.0f, 0.0f, 0.0f};
    a.middle = {0.0f, 0.0f, 0.0f};
    a.velocity = 100.0f;

    RLPoint b;
    b.type = RLPointType::MoveC;  // arc
    b.end = {10.0f, 10.0f, 0.0f};
    b.middle = {5.0f, 5.0f, 0.0f};
    b.velocity = 80.0f;

    // ABB
    RobotPath abb(RLType::Abb);
    abb.push_back(a);
    abb.push_back(b);
    auto outAbb = abb.ToString();
    std::cout << "----- ABB output -----\n" << outAbb << "\n";
    BOOST_CHECK_NE(outAbb.find("MODULE mainModule"), std::string::npos);
    BOOST_CHECK_NE(outAbb.find("MOVEJ"), std::string::npos);
    BOOST_CHECK_NE(outAbb.find("MOVEC"), std::string::npos);

    // KUKA
    RobotPath kuka(RLType::Kuka);
    kuka.push_back(a);
    kuka.push_back(b);
    auto outKuka = kuka.ToString();
    std::cout << "----- KUKA output -----\n" << outKuka << "\n";
    BOOST_CHECK_NE(outKuka.find("CIRC"), std::string::npos);
    BOOST_CHECK_NE(outKuka.find("LIN"), std::string::npos);

    // FANUC
    RobotPath fanuc(RLType::Fanuc);
    fanuc.push_back(a);
    fanuc.push_back(b);
    auto outFanuc = fanuc.ToString();
    std::cout << "----- FANUC output -----\n" << outFanuc << "\n";
    BOOST_CHECK_NE(outFanuc.find("ARC"), std::string::npos);
    BOOST_CHECK_NE(outFanuc.find("J P"), std::string::npos);
}

BOOST_AUTO_TEST_CASE(test_robot_script_out)
{
    using namespace HsBa::Slicer;

    RobotPath rp(RLType::Abb);
    RLPoint p;
    p.type = RLPointType::MoveL;
    p.end = {1.0f, 2.0f, 3.0f};
    p.middle = {0.0f, 0.0f, 0.0f};
    p.velocity = 50.0f;
    rp.push_back(p);

    {
        std::string script_header = R"lua(
local lines = {}
table.insert(lines, "index,type,endx,endy,endz,velocity")
for i,pt in ipairs(points) do
	-- use bracket syntax for field named 'end' because 'end' is a Lua keyword
	table.insert(lines, string.format("%d,%s,%.4f,%.4f,%.4f,%.3f", i, pt.type, pt["end"].x, pt["end"].y, pt["end"].z, pt.velocity or 0.0))
end
result = table.concat(lines, "\n")
result = header .. result
return result
)lua";

        auto res = rp.ToString(script_header);
        std::cout << "----- Robot Lua output with header -----\n" << res << "\n";
        BOOST_CHECK_NE(res.find("// Script provided by user"), std::string::npos);
        BOOST_CHECK_NE(res.find("index,type,endx,endy,endz,velocity"), std::string::npos);
        BOOST_CHECK_NE(res.find("1,MoveL,1.0000,2.0000,3.0000"), std::string::npos);
    }
    {
        std::string script_no_header = R"lua(
local function removeFirstLine(str)
    local firstNewLine = str:find("\n", 1, true)
    if firstNewLine then
        return str:sub(firstNewLine + 1)
    else
        return ""
    end
end
local lines = {}
table.insert(lines, "index,type,endx,endy,endz,velocity")
for i,pt in ipairs(points) do
	-- use bracket syntax for field named 'end' because 'end' is a Lua keyword
	table.insert(lines, string.format("%d,%s,%.4f,%.4f,%.4f,%.3f", i, pt.type, pt["end"].x, pt["end"].y, pt["end"].z, pt.velocity or 0.0))
end
result = table.concat(lines, "\n")
result = header .. result
result = removeFirstLine(result)
return result
)lua";

        auto res = rp.ToString(script_no_header);
        std::cout << "----- Robot Lua output -----\n" << res << "\n";
        BOOST_CHECK_EQUAL(res.find("// Script provided by user"), std::string::npos);
        BOOST_CHECK_NE(res.find("index,type,endx,endy,endz,velocity"), std::string::npos);
        BOOST_CHECK_NE(res.find("1,MoveL,1.0000,2.0000,3.0000"), std::string::npos);
    }
}

// Native ToString(): exercise the arc (G2/G3) branch, inch-unit header, a
// zero-velocity move (no F field) and the `continue` branch for a non-motion
// code (G90 point is skipped, so "G90" appears only from the header line).
BOOST_AUTO_TEST_CASE(test_gcode_arc_and_extra_codes)
{
    using namespace HsBa::Slicer;
    PointsPath path(GCodeUnits::Inch, {1.0f, 1.0f, 1.0f});

    GPoint arc;
    arc.type = GcodeType::G2;
    arc.p1 = {5.0f, 5.0f, 0.0f};
    arc.center = {2.0f, 3.0f, 0.0f};
    arc.velocity = 300.0f;
    arc.extrusion = 0.25;
    path.push_back(arc);

    GPoint ccw;
    ccw.type = GcodeType::G3;
    ccw.p1 = {7.0f, 7.0f, 0.0f};
    ccw.center = {6.0f, 6.0f, 0.0f};
    ccw.velocity = 0.0f;
    ccw.extrusion = 0.0;
    path.push_back(ccw);

    GPoint mode;
    mode.type = GcodeType::G90;  // not a motion code -> hits the `continue` branch
    path.push_back(mode);

    auto out = path.ToString();
    std::cout << "----- arc/inch ToString -----\n" << out << "\n";
    BOOST_CHECK_NE(out.find("G20"), std::string::npos);  // inch units
    BOOST_CHECK_NE(out.find("G2 X5.0000 Y5.0000 Z0.0000 I2.0000 J3.0000 K0.0000"), std::string::npos);
    BOOST_CHECK_NE(out.find("F300"), std::string::npos);
    BOOST_CHECK_NE(out.find("G3 X7.0000"), std::string::npos);
    BOOST_CHECK_NE(out.find("E0.000000"), std::string::npos);

    size_t count = 0, pos = 0;
    while ((pos = out.find("G90", pos)) != std::string::npos)
    {
        ++count;
        pos += 3;
    }
    BOOST_CHECK_EQUAL(count, 1u);
}

// ToString(script, lua_reg): global-result fallback, empty chunk, and the
// load/runtime error paths that return (not throw) a diagnostic string.
BOOST_AUTO_TEST_CASE(test_tostring_script_fallbacks_and_errors)
{
    using namespace HsBa::Slicer;
    PointsPath path(GCodeUnits::mm, {0.0f, 0.0f, 0.0f});
    GPoint p;
    p.type = GcodeType::G1;
    p.p1 = {1.0f, 2.0f, 3.0f};
    path.push_back(p);

    // script sets the global `result` instead of returning a value
    BOOST_CHECK_EQUAL(path.ToString(std::string_view("result = 'FROM_RESULT'")), "FROM_RESULT");
    // empty script: no return, no result -> empty string
    BOOST_CHECK_EQUAL(path.ToString(std::string_view("")), "");
    // load error -> returns a "Lua load error" string (does not throw)
    BOOST_CHECK_NE(path.ToString(std::string_view("bad lua @@@ ###")).find("Lua load error"), std::string::npos);
    // runtime error -> returns a "Lua runtime error" string (does not throw)
    BOOST_CHECK_NE(path.ToString(std::string_view("error('boom')")).find("Lua runtime error"), std::string::npos);
}

// Save overloads: native, script(string-return), and funcName-driven (via lua_reg).
BOOST_AUTO_TEST_CASE(test_save_overloads)
{
    using namespace HsBa::Slicer;
    PointsPath path(GCodeUnits::mm, {0.0f, 0.0f, 0.0f});
    GPoint p;
    p.type = GcodeType::G1;
    p.p1 = {1.0f, 2.0f, 3.0f};
    p.velocity = 100.0f;
    p.extrusion = 0.1;
    path.push_back(p);

    auto dir = std::filesystem::temp_directory_path();
    std::error_code ec;
    auto slurp = [](const std::filesystem::path& f) {
        std::ifstream ifs(f);
        return std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    };

    // native Save writes the G-code text
    auto f1 = dir / "points_native.gcode";
    std::filesystem::remove(f1, ec);
    path.Save(f1);
    BOOST_CHECK(std::filesystem::exists(f1));
    BOOST_CHECK_NE(slurp(f1).find("G21"), std::string::npos);
    std::filesystem::remove(f1, ec);

    // Save(path, script) writes the Lua string result
    auto f2 = dir / "points_script.gcode";
    std::filesystem::remove(f2, ec);
    path.Save(f2, std::string_view("return 'SCRIPT_SAVED'"));
    BOOST_CHECK_EQUAL(slurp(f2), "SCRIPT_SAVED");
    std::filesystem::remove(f2, ec);

    // Save(path, script, funcName) invokes the lua_reg-registered function
    auto f3 = dir / "points_func.gcode";
    std::filesystem::remove(f3, ec);
    auto reg = [](lua_State* L) { luaL_dostring(L, "function sg() return 'FUNC_SAVED' end"); };
    path.Save(f3, std::string_view(""), std::string_view("sg"), reg);
    BOOST_CHECK_EQUAL(slurp(f3), "FUNC_SAVED");
    std::filesystem::remove(f3, ec);
}

// The funcName overloads resolve a Lua function by name (registered through
// lua_reg) and call it; the script argument itself is not compiled here.
BOOST_AUTO_TEST_CASE(test_funcname_and_script_file)
{
    using namespace HsBa::Slicer;
    PointsPath path(GCodeUnits::mm, {0.0f, 0.0f, 0.0f});
    GPoint p;
    p.type = GcodeType::G1;
    p.p1 = {1.0f, 2.0f, 3.0f};
    path.push_back(p);

    auto dir = std::filesystem::temp_directory_path();
    std::error_code ec;
    auto reg = [](lua_State* L) { luaL_dostring(L, "function gen() return 'GEN:' .. tostring(#points) end"); };

    BOOST_CHECK_EQUAL(path.ToString(std::string_view(""), std::string_view("gen"), reg), "GEN:1");
    // funcName that was never registered -> throws
    BOOST_CHECK_THROW(path.ToString(std::string_view(""), std::string_view("missing")), HsBa::Slicer::RuntimeError);

    // ToString(script_file, funcName) reads the file then calls the registered function
    auto scriptFile = dir / "points_gen.lua";
    { std::ofstream ofs(scriptFile); ofs << "-- file body is not executed by this overload"; }
    BOOST_CHECK_EQUAL(path.ToString(scriptFile, std::string_view("gen"), reg), "GEN:1");
    // Save(path, script_file, funcName)
    auto saved = dir / "points_filesave.gcode";
    std::filesystem::remove(saved, ec);
    path.Save(saved, scriptFile, std::string_view("gen"), reg);
    { std::ifstream ifs(saved);
      std::string c((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
      BOOST_CHECK_EQUAL(c, "GEN:1"); }
    std::filesystem::remove(saved, ec);
    std::filesystem::remove(scriptFile, ec);

    // missing script file -> throws before function resolution
    BOOST_CHECK_THROW(path.ToString(std::filesystem::path("hsba_no_such_dir/none.lua"), std::string_view("gen"), reg),
                      HsBa::Slicer::RuntimeError);
}

BOOST_AUTO_TEST_SUITE_END()