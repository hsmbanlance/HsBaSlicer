#define BOOST_TEST_MODULE points_path_test
#include <boost/test/included/unit_test.hpp>

#include "paths/pointspath.hpp"
#include <iostream>

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
	BOOST_CHECK_NE(out.find("G21"), std::string::npos); // units mm
	BOOST_CHECK_NE(out.find("G90"), std::string::npos); // absolute
	BOOST_CHECK_NE(out.find("G0 X0"), std::string::npos); // start move
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

BOOST_AUTO_TEST_SUITE_END()