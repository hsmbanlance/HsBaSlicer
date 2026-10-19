#define BOOST_TEST_MODULE robot_path_test
#include <boost/test/included/unit_test.hpp>

#include <filesystem>
#include <fstream>
#include <string>

#include "base/error.hpp"
#include "paths/robotpath.hpp"

using namespace HsBa::Slicer;

namespace
{
bool Contains(const std::string& haystack, std::string_view needle)
{
    return haystack.find(needle) != std::string::npos;
}

std::string ReadAll(const std::filesystem::path& p)
{
    std::ifstream ifs(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>()};
}

// One point of every motion/program kind, so native generation and the Lua type
// dump exercise every branch of the RLPointType switch.
std::vector<RLPoint> AllKindPoints()
{
    const RLPointType kinds[] = {RLPointType::MoveJ,     RLPointType::MoveL,     RLPointType::MoveC,
                                 RLPointType::ProgramLStart, RLPointType::ProgramStart, RLPointType::ProgramCStart,
                                 RLPointType::ProgramL,  RLPointType::ProgramC,  RLPointType::ProgramLEnd,
                                 RLPointType::ProgramCEnd};
    std::vector<RLPoint> pts;
    float x = 1.0f;
    for (auto k : kinds)
    {
        RLPoint p;
        p.end = OutPoints3{x, x + 1.0f, x + 2.0f};
        p.middle = OutPoints3{x + 0.5f, x + 1.5f, x + 2.5f};
        p.velocity = 123.0f;
        p.type = k;
        p.programIndex = pts.size();
        pts.push_back(p);
        x += 10.0f;
    }
    return pts;
}
}  // namespace

BOOST_AUTO_TEST_SUITE(robot_path)

BOOST_AUTO_TEST_CASE(get_robot_type_and_operator_index)
{
    RobotPath path{RLType::Abb, OutPoints3{1.0f, 2.0f, 3.0f}};
    BOOST_CHECK(path.getRobotType() == RLType::Abb);

    RLPoint p;
    p.end = OutPoints3{4.0f, 5.0f, 6.0f};
    p.type = RLPointType::MoveL;
    path.push_back(p);

    RLPoint got = path[0];
    BOOST_CHECK_EQUAL(got.end.x, 4.0f);
    BOOST_CHECK(got.type == RLPointType::MoveL);
}

BOOST_AUTO_TEST_CASE(native_abb_covers_all_move_kinds)
{
    RobotPath path{RLType::Abb, OutPoints3{}, "WeldStart", "WeldEnd"};
    for (const auto& p : AllKindPoints())
        path.push_back(p);

    auto code = path.ToString();
    BOOST_CHECK(Contains(code, "# RobotPath default export"));
    BOOST_CHECK(Contains(code, "! Robot: ABB"));
    BOOST_CHECK(Contains(code, "MODULE mainModule"));
    BOOST_CHECK(Contains(code, "PROC main"));
    BOOST_CHECK(Contains(code, "MOVEJ"));
    BOOST_CHECK(Contains(code, "MOVEL"));
    BOOST_CHECK(Contains(code, "MOVEC"));
    BOOST_CHECK(Contains(code, "Start of Program Segment"));
    BOOST_CHECK(Contains(code, "End of Program Segment"));
    BOOST_CHECK(Contains(code, "WeldStart"));
    BOOST_CHECK(Contains(code, "WeldEnd"));
    BOOST_CHECK(Contains(code, "Program Point"));
    BOOST_CHECK(Contains(code, "ENDPROC"));
    BOOST_CHECK(Contains(code, "ENDMODULE"));
}

BOOST_AUTO_TEST_CASE(native_kuka_covers_lin_and_circ)
{
    RobotPath path{RLType::Kuka, OutPoints3{}, "startFn", "endFn"};
    for (const auto& p : AllKindPoints())
        path.push_back(p);

    auto code = path.ToString();
    BOOST_CHECK(Contains(code, "# Robot: KUKA"));
    BOOST_CHECK(Contains(code, "DEF main()"));
    BOOST_CHECK(Contains(code, "LIN"));
    BOOST_CHECK(Contains(code, "CIRC"));
    BOOST_CHECK(Contains(code, "Start of Program Segment"));
    BOOST_CHECK(Contains(code, "End of Program Segment"));
    BOOST_CHECK(Contains(code, "END"));
}

BOOST_AUTO_TEST_CASE(native_fanuc_covers_joint_and_arc)
{
    RobotPath path{RLType::Fanuc};
    for (const auto& p : AllKindPoints())
        path.push_back(p);

    auto code = path.ToString();
    BOOST_CHECK(Contains(code, "# Robot: FANUC"));
    BOOST_CHECK(Contains(code, "PR[1]="));
    BOOST_CHECK(Contains(code, "J P["));
    BOOST_CHECK(Contains(code, "ARC"));
    BOOST_CHECK(Contains(code, "Program Point"));
}

BOOST_AUTO_TEST_CASE(unknown_brands_throw_not_supported)
{
    RobotPath unknown{RLType::Unknown};
    unknown.push_back(RLPoint{});
    BOOST_CHECK_THROW(unknown.ToString(), HsBa::Slicer::NotSupportedError);

    RobotPath undefine{RLType::Undefine};
    undefine.push_back(RLPoint{});
    BOOST_CHECK_THROW(undefine.ToString(), HsBa::Slicer::NotSupportedError);
}

BOOST_AUTO_TEST_CASE(lua_script_sees_points_and_start)
{
    RobotPath path{RLType::Abb, OutPoints3{7.0f, 0.0f, 0.0f}};
    RLPoint p;
    p.end = OutPoints3{1.5f, 2.5f, 3.5f};
    p.velocity = 88.0f;
    p.type = RLPointType::MoveC;
    path.push_back(p);

    // header global is available and marks the script path
    BOOST_CHECK(Contains(path.ToString("return header"), "robot type ignored"));

    // the points table carries positions/velocity/type; 'end' is a Lua keyword
    auto summary = path.ToString(
        "return string.format('%d|%.1f|%s|%.0f', #points, points[1][\"end\"].x, points[1].type, points[1].velocity)");
    BOOST_CHECK_EQUAL(summary, "1|1.5|MoveC|88");

    // the startPoint global is exposed
    BOOST_CHECK_EQUAL(path.ToString("return string.format('%.1f', startPoint.x)"), "7.0");

    // every point kind round-trips through RLPointTypeToString
    auto types = path.ToString(
        "local t = {} for i=1,#points do t[i]=points[i].type end return table.concat(t, ',')");
    BOOST_CHECK_NE(types.find("MoveC"), std::string::npos);
}

BOOST_AUTO_TEST_CASE(lua_result_global_and_empty_return)
{
    RobotPath path{RLType::Abb};
    path.push_back(RLPoint{});

    // no returned value, but the script sets the global 'result'
    BOOST_CHECK_EQUAL(path.ToString("result = 'set_by_result'"), "set_by_result");

    // neither a returned string nor a result global -> empty body
    BOOST_CHECK_EQUAL(path.ToString("local unused = 1"), "");
}

BOOST_AUTO_TEST_CASE(lua_funcname_and_reg)
{
    RobotPath path{RLType::Abb};
    path.push_back(RLPoint{});

    BOOST_CHECK_EQUAL(path.ToString(std::string_view("return funcName"), "myFn"), "myFn");

    bool reg_hit = false;
    auto lua_reg = [&](lua_State*) { reg_hit = true; };
    path.ToString("return 'ok'", lua_reg);
    BOOST_CHECK(reg_hit);
}

BOOST_AUTO_TEST_CASE(lua_errors_throw)
{
    RobotPath path{RLType::Abb};
    path.push_back(RLPoint{});
    BOOST_CHECK_THROW(path.ToString("%%% not lua"), HsBa::Slicer::RuntimeError);
    BOOST_CHECK_THROW(path.ToString("error('robot boom')"), HsBa::Slicer::RuntimeError);
    BOOST_CHECK_THROW(path.ToString(std::string_view("%%% not lua"), "fn"), HsBa::Slicer::RuntimeError);
    BOOST_CHECK_THROW(path.ToString(std::string_view("error('robot boom')"), "fn"), HsBa::Slicer::RuntimeError);
}

BOOST_AUTO_TEST_CASE(save_all_variants)
{
    RobotPath path{RLType::Fanuc, OutPoints3{1.0f, 1.0f, 1.0f}};
    path.push_back(RLPoint{});
    std::error_code ec;

    // native save matches ToString()
    auto native = std::filesystem::temp_directory_path() / "hsba_robot_native.txt";
    std::filesystem::remove(native, ec);
    path.Save(native);
    BOOST_CHECK(std::filesystem::exists(native));
    BOOST_CHECK_EQUAL(ReadAll(native), path.ToString());
    std::filesystem::remove(native, ec);

    // script save writes the returned string
    auto s1 = std::filesystem::temp_directory_path() / "hsba_robot_script.txt";
    std::filesystem::remove(s1, ec);
    path.Save(s1, "return 'SCR'");
    BOOST_CHECK_EQUAL(ReadAll(s1), "SCR");
    std::filesystem::remove(s1, ec);

    // funcName save writes 'FN' + the injected function name
    auto s2 = std::filesystem::temp_directory_path() / "hsba_robot_func.txt";
    std::filesystem::remove(s2, ec);
    path.Save(s2, std::string_view("return 'FN' .. funcName"), "go");
    BOOST_CHECK_EQUAL(ReadAll(s2), "FNgo");
    std::filesystem::remove(s2, ec);

    // script-file save reads and runs the Lua file
    auto lua_file = std::filesystem::temp_directory_path() / "hsba_robot_prog.lua";
    std::filesystem::remove(lua_file, ec);
    {
        std::ofstream ofs(lua_file);
        ofs << "return 'FILE:' .. funcName";
    }
    auto s3 = std::filesystem::temp_directory_path() / "hsba_robot_file.txt";
    std::filesystem::remove(s3, ec);
    path.Save(s3, lua_file, "run");
    BOOST_CHECK_EQUAL(ReadAll(s3), "FILE:run");
    std::filesystem::remove(s3, ec);
    std::filesystem::remove(lua_file, ec);
}

BOOST_AUTO_TEST_CASE(to_string_script_file_and_missing_throws)
{
    RobotPath path{RLType::Abb};
    path.push_back(RLPoint{});
    std::error_code ec;

    auto lua_file = std::filesystem::temp_directory_path() / "hsba_robot_str.lua";
    std::filesystem::remove(lua_file, ec);
    {
        std::ofstream ofs(lua_file);
        ofs << "return 'S:' .. funcName";
    }
    BOOST_CHECK_EQUAL(path.ToString(lua_file, "fn"), "S:fn");
    std::filesystem::remove(lua_file, ec);

    BOOST_CHECK_THROW(path.ToString(std::filesystem::path("hsba_no_such_robot_script.lua"), "fn"),
                      HsBa::Slicer::RuntimeError);
}

BOOST_AUTO_TEST_SUITE_END()
