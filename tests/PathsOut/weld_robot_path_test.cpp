#define BOOST_TEST_MODULE weld_robot_path_test
#include <boost/test/included/unit_test.hpp>

#include <filesystem>
#include <fstream>
#include <string>

#include "base/error.hpp"
#include "paths/weldrobotpath.hpp"

using namespace HsBa::Slicer;

namespace
{
WeldRLPoint MakeWeldPoint(float x, float y, float z, bool welding)
{
    WeldRLPoint wp;
    wp.point.end = OutPoints3{x, y, z};
    wp.point.velocity = 8.0f;
    wp.isWelding = welding;
    wp.weld.current = 180.0f;
    wp.weld.voltage = 21.0f;
    wp.weld.process = WeldProcessType::TIG;
    wp.weld.arcEnd = welding ? ArcEndType::CraterFill : ArcEndType::Normal;
    return wp;
}

bool Contains(const std::string& haystack, std::string_view needle)
{
    return haystack.find(needle) != std::string::npos;
}
}  // namespace

BOOST_AUTO_TEST_SUITE(weld_robot_path_test)

BOOST_AUTO_TEST_CASE(weld_points_are_stored_and_forwarded)
{
    WeldRobotPath path{RLType::Fanuc};
    BOOST_CHECK(path.weldPoints().empty());

    path.push_back(MakeWeldPoint(1.0f, 2.0f, 3.0f, true));
    path.push_back(MakeWeldPoint(4.0f, 5.0f, 6.0f, false));
    // plain movement point through the inherited RobotPath API
    path.push_back(RLPoint{});

    BOOST_CHECK_EQUAL(path.weldPoints().size(), 2u);
    BOOST_CHECK(path.weldPoints()[0].isWelding);
    BOOST_CHECK_EQUAL(path.weldPoints()[0].weld.current, 180.0f);
    BOOST_CHECK(path.weldPoints()[1].weld.process == WeldProcessType::TIG);
}

BOOST_AUTO_TEST_CASE(to_string_per_robot_brand)
{
    WeldRobotPath abb{RLType::Abb};
    abb.push_back(MakeWeldPoint(0.0f, 0.0f, 0.0f, true));
    auto abb_code = abb.ToString();
    BOOST_CHECK(Contains(abb_code, "# WeldRobotPath export"));
    BOOST_CHECK(Contains(abb_code, "! Robot: ABB (Welding)"));

    WeldRobotPath kuka{RLType::Kuka};
    kuka.push_back(MakeWeldPoint(0.0f, 0.0f, 0.0f, true));
    auto kuka_code = kuka.ToString();
    BOOST_CHECK(Contains(kuka_code, "; Robot: KUKA (Welding)"));

    WeldRobotPath fanuc{RLType::Fanuc};
    fanuc.push_back(MakeWeldPoint(1.0f, 1.0f, 1.0f, true));
    fanuc.push_back(MakeWeldPoint(2.0f, 2.0f, 2.0f, false));
    auto fanuc_code = fanuc.ToString();
    BOOST_CHECK(Contains(fanuc_code, "; Robot: FANUC (Welding)"));

    // unknown brand cannot be rendered natively
    WeldRobotPath unknown{RLType::Unknown};
    unknown.push_back(MakeWeldPoint(0.0f, 0.0f, 0.0f, true));
    BOOST_CHECK_THROW(unknown.ToString(), HsBa::Slicer::NotSupportedError);
}

BOOST_AUTO_TEST_CASE(save_writes_generated_program)
{
    WeldRobotPath path{RLType::Fanuc};
    path.push_back(MakeWeldPoint(1.0f, 2.0f, 3.0f, true));

    auto tmp = std::filesystem::temp_directory_path() / "hsba_weld_prog.txt";
    std::error_code ec;
    std::filesystem::remove(tmp, ec);
    path.Save(tmp);
    BOOST_CHECK(std::filesystem::exists(tmp));
    std::ifstream ifs(tmp, std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    BOOST_CHECK(Contains(content, "FANUC"));
    BOOST_CHECK(content == path.ToString());
    std::filesystem::remove(tmp, ec);
}

BOOST_AUTO_TEST_CASE(lua_script_sees_weld_points)
{
    WeldRobotPath path{RLType::Unknown};  // script mode ignores the brand
    path.push_back(MakeWeldPoint(1.5f, 2.5f, 0.5f, true));
    path.push_back(MakeWeldPoint(3.5f, 4.5f, 0.5f, false));

    // header global is available
    auto header = path.ToString("return header");
    BOOST_CHECK(Contains(header, "WeldRobotPath script"));

    // points table carries positions, flags and the nested weld parameter table.
    // 'end' is a Lua keyword, so the field must be reached via ["end"], not .end
    auto summary = path.ToString("return string.format('%d|%.2f|%s|%.0f', #points, "
                                 "points[1][\"end\"].x, tostring(points[1].isWelding), points[1].weld.current)");
    BOOST_CHECK_EQUAL(summary, "2|1.50|true|180");

    // load and runtime errors are wrapped into RuntimeError
    BOOST_CHECK_THROW(path.ToString("%%% not lua"), HsBa::Slicer::RuntimeError);
    BOOST_CHECK_THROW(path.ToString("error('weld boom')"), HsBa::Slicer::RuntimeError);
}

BOOST_AUTO_TEST_SUITE_END()
