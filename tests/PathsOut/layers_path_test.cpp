#define BOOST_TEST_MODULE layers_path_test
#include <boost/test/included/unit_test.hpp>

#include "paths/layerspath.hpp"
#include <filesystem>
#include <fstream>

using namespace HsBa::Slicer;

BOOST_AUTO_TEST_SUITE(layers_path_test)

BOOST_AUTO_TEST_CASE(test_to_string_and_formatter_save)
{
    LayersPath lp([](std::string_view, std::string_view){});

    // build a simple layer
    PolygonsD poly;
    poly.emplace_back();
    poly[0].push_back({1.0, 2.0});

    lp.push_back("cfg1", poly);

    auto s = lp.ToString();
    std::cout << "Layers ToString() => " << s << "\n";
    BOOST_CHECK_NE(s.find("cfg1"), std::string::npos);

    // Lua formatter script: return CSV string, use_db = false
    std::string script = R"lua(
local lines = {}
table.insert(lines, "config,x,y")
for i,l in ipairs(layers) do
  for j,p in ipairs(l.data[1]) do
    table.insert(lines, string.format("%s,%.4f,%.4f", l.config, p.x, p.y))
  end
end
return table.concat(lines, "\n")
)lua";

    auto tmp = std::filesystem::temp_directory_path() / "layers_out.txt";
    // remove if exists
    std::error_code ec; std::filesystem::remove(tmp, ec);
    lp.Save(tmp, script);
    BOOST_CHECK(std::filesystem::exists(tmp));
    std::ifstream ifs(tmp, std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    std::cout << "Formatter output file content:\n" << content << "\n";
    BOOST_CHECK_NE(content.find("config,x,y"), std::string::npos);
    std::filesystem::remove(tmp, ec);
}

BOOST_AUTO_TEST_CASE(test_save_with_db_rows)
{
    LayersPath lp([](std::string_view type, std::string_view sql){
      std::cout << "Callback: " << type << ", " << sql << "\n";
    });
    PolygonsD poly;
    poly.emplace_back(); poly[0].push_back({3.0,4.0});
    lp.push_back("cfg_db", poly);

    // script will set use_db = true and provide rows
    std::string script = R"lua(
use_db = true
db_path = ... -- will be ignored if empty
rows = {
  { config = "r1", data = "d1" },
  { config = "r2", data = "d2" }
}
return true
)lua";

  auto tmpdb = std::filesystem::temp_directory_path() / "layers_out.db";
  std::error_code ec; std::filesystem::remove(tmpdb, ec);

  // Save: do not inject Windows path into Lua source (avoid escape issues)
  lp.Save(tmpdb, script);
  BOOST_CHECK(std::filesystem::exists(tmpdb));
  std::filesystem::remove(tmpdb, ec);
}

BOOST_AUTO_TEST_SUITE_END()
