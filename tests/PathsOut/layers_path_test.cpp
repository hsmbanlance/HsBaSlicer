#define BOOST_TEST_MODULE layers_path_test
#include <boost/test/included/unit_test.hpp>

#include "paths/layerspath.hpp"
#include <filesystem>
#include <fstream>
#include <lua.hpp>
#include "fileoperator/LuaAdapter.hpp"
#include "fileoperator/sql_adapter.hpp"

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
  // prepare temporary sqlite file
  auto tmpdb = std::filesystem::temp_directory_path() / "layers_out.db";
  std::error_code ec; std::filesystem::remove(tmpdb, ec);

  // register Lua SQLite adapter in a transient lua_State (optional but ensures metatable setup)
  lua_State* Lreg = luaL_newstate();
  if (!Lreg) throw std::runtime_error("Lua init failed in test");
  luaL_openlibs(Lreg);
  HsBa::Slicer::RegisterLuaSQLiteAdapter(Lreg);
  lua_close(Lreg);

  // Lua script: use provided global `db` (already connected to tmpdb)
  std::string script = R"lua(
local db = SQLiteAdapter.new()
db:Connect(output_path)
db:Execute('CREATE TABLE IF NOT EXISTS test_layers (id INTEGER PRIMARY KEY AUTOINCREMENT, layer_config TEXT NOT NULL, layer_data TEXT NOT NULL)')
db:Insert('test_layers', { layer_config = 'cfg_db', layer_data = 'lua_inserted' })
return true
)lua";

  // execute Save which will create Lua env and provide `db`
  lp.Save(tmpdb, script);

  // verify using SQLiteAdapter
  HsBa::Slicer::SQL::SQLiteAdapter sdb;
  sdb.Connect(tmpdb.string());
  auto rows = sdb.Query("SELECT layer_config, layer_data FROM test_layers");
  BOOST_CHECK(!rows.empty());
  auto it = rows[0].find("layer_config");
  BOOST_CHECK(it != rows[0].end());
  BOOST_CHECK(it->second.type() == typeid(std::string));
  BOOST_CHECK(std::any_cast<std::string>(it->second) == "cfg_db");

  // cleanup
  std::filesystem::remove(tmpdb, ec);
}

BOOST_AUTO_TEST_SUITE_END()
