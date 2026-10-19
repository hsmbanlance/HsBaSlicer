#define BOOST_TEST_MODULE layers_path_test
#include <boost/test/included/unit_test.hpp>

#include "fileoperator/LuaAdapter.hpp"
#include "fileoperator/sql_adapter.hpp"
#include "paths/layerspath.hpp"
#include "base/error.hpp"
#include <filesystem>
#include <fstream>
#include <lua.hpp>

using namespace HsBa::Slicer;

BOOST_AUTO_TEST_SUITE(layers_path_test)

BOOST_AUTO_TEST_CASE(test_to_string_and_formatter_save)
{
    LayersPath lp([](std::string_view, std::string_view) {});

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
    std::error_code ec;
    std::filesystem::remove(tmp, ec);
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
    LayersPath lp([](std::string_view type, std::string_view sql)
                  { std::cout << "Callback: " << type << ", " << sql << "\n"; });
    PolygonsD poly;
    poly.emplace_back();
    poly[0].push_back({3.0, 4.0});
    lp.push_back("cfg_db", poly);
    // prepare temporary sqlite file
    auto tmpdb = std::filesystem::temp_directory_path() / "layers_out.db";
    std::error_code ec;
    std::filesystem::remove(tmpdb, ec);

    // register Lua SQLite adapter in a transient lua_State (optional but ensures metatable setup)
    auto Lreg = HsBa::Slicer::MakeUniqueLuaState();
    if (!Lreg)
        throw std::runtime_error("Lua init failed in test");
    luaL_openlibs(Lreg.get());
    HsBa::Slicer::RegisterLuaSQLiteAdapter(Lreg.get());

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

// Native single-argument Save writes a real SQLite table 'layers'.
BOOST_AUTO_TEST_CASE(test_save_native_sqlite_rows)
{
    LayersPath lp([](std::string_view, std::string_view) {});
    PolygonsD poly;
    poly.emplace_back();
    poly[0].push_back({1.5, 2.5});
    poly[0].push_back({3.0, 4.0});
    PolygonsD poly2;
    poly2.emplace_back();
    poly2[0].push_back({5.0, 6.0});
    lp.push_back("cfgA", poly);
    lp.push_back("cfgB", poly2);

    auto tmpdb = std::filesystem::temp_directory_path() / "lp_native.db";
    std::error_code ec;
    std::filesystem::remove(tmpdb, ec);
    lp.Save(tmpdb);
    BOOST_CHECK(std::filesystem::exists(tmpdb));

    HsBa::Slicer::SQL::SQLiteAdapter db;
    db.Connect(tmpdb.string());
    auto rows = db.Query("SELECT layer_config FROM layers");
    BOOST_CHECK_EQUAL(rows.size(), 2u);
    bool foundA = false, foundB = false;
    for (auto& r : rows)
    {
        auto c = r.find("layer_config");
        if (c != r.end() && c->second.type() == typeid(std::string))
        {
            std::string v = std::any_cast<std::string>(c->second);
            foundA = foundA || (v == "cfgA");
            foundB = foundB || (v == "cfgB");
        }
    }
    BOOST_CHECK(foundA);
    BOOST_CHECK(foundB);
    std::filesystem::remove(tmpdb, ec);
}

// ToString(script, lua_reg): empty-script native shortcut, string return,
// global-result fallback, empty body, and the two error paths (which THROW).
BOOST_AUTO_TEST_CASE(test_tostring_script_variants)
{
    LayersPath lp([](std::string_view, std::string_view) {});
    PolygonsD poly;
    poly.emplace_back();
    poly[0].push_back({1.0, 2.0});
    lp.push_back("cfg1", poly);

    // empty script short-circuits to the native formatter
    BOOST_CHECK_NE(lp.ToString(std::string_view("")).find("cfg1"), std::string::npos);
    // direct string return
    BOOST_CHECK_EQUAL(lp.ToString(std::string_view("return 'RET'")), "RET");
    // global result fallback
    BOOST_CHECK_EQUAL(lp.ToString(std::string_view("result = 'RES'")), "RES");
    // neither returns nor sets result -> empty body
    BOOST_CHECK_EQUAL(lp.ToString(std::string_view("local x = 1")), "");
    // load and runtime errors throw (differs from PointsPath)
    BOOST_CHECK_THROW(lp.ToString(std::string_view("bad lua @@@ ###")), HsBa::Slicer::RuntimeError);
    BOOST_CHECK_THROW(lp.ToString(std::string_view("error('boom')")), HsBa::Slicer::RuntimeError);
}

// ToString(script, funcName): LayersPath exposes funcName as a global that the
// executed script can read.
BOOST_AUTO_TEST_CASE(test_tostring_with_funcname)
{
    LayersPath lp([](std::string_view, std::string_view) {});
    PolygonsD poly;
    poly.emplace_back();
    poly[0].push_back({1.0, 2.0});
    lp.push_back("cfg1", poly);

    std::string script = "return 'FN=' .. funcName";
    BOOST_CHECK_EQUAL(lp.ToString(std::string_view(script), std::string_view("myFunc")), "FN=myFunc");
}

// Save(path, script, funcName), file-based Save/ToString, and their missing-file errors.
BOOST_AUTO_TEST_CASE(test_save_funcname_and_script_file)
{
    LayersPath lp([](std::string_view, std::string_view) {});
    PolygonsD poly;
    poly.emplace_back();
    poly[0].push_back({1.0, 2.0});
    lp.push_back("cfg1", poly);

    auto dir = std::filesystem::temp_directory_path();
    std::error_code ec;
    auto slurp = [](const std::filesystem::path& f) {
        std::ifstream ifs(f);
        return std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    };

    // Save(path, script, funcName): script sees funcName + #layers, result written to path
    auto out = dir / "lp_func.txt";
    std::filesystem::remove(out, ec);
    std::string script = "return 'OUT:' .. funcName .. ':' .. tostring(#layers)";
    lp.Save(out, std::string_view(script), std::string_view("fn1"));
    BOOST_CHECK_EQUAL(slurp(out), "OUT:fn1:1");
    std::filesystem::remove(out, ec);

    // Save(path, script_file, funcName): script read from file
    auto scriptFile = dir / "lp_save.lua";
    { std::ofstream ofs(scriptFile); ofs << "return 'FROM_FILE:' .. funcName"; }
    auto out2 = dir / "lp_file.txt";
    std::filesystem::remove(out2, ec);
    lp.Save(out2, scriptFile, std::string_view("fnF"));
    BOOST_CHECK_EQUAL(slurp(out2), "FROM_FILE:fnF");
    std::filesystem::remove(out2, ec);
    std::filesystem::remove(scriptFile, ec);

    // missing script file -> Save throws
    BOOST_CHECK_THROW(lp.Save(out, std::filesystem::path("hsba_no_such_dir/none.lua"), std::string_view("x")),
                      HsBa::Slicer::RuntimeError);

    // ToString(script_file, funcName): file-based serialize
    auto tsFile = dir / "lp_ts.lua";
    { std::ofstream ofs(tsFile); ofs << "return 'TSFILE:' .. funcName"; }
    BOOST_CHECK_EQUAL(lp.ToString(tsFile, std::string_view("tfn")), "TSFILE:tfn");
    std::filesystem::remove(tsFile, ec);

    // missing script file -> ToString throws
    BOOST_CHECK_THROW(lp.ToString(std::filesystem::path("hsba_no_such_dir/none.lua"), std::string_view("x")),
                      HsBa::Slicer::RuntimeError);
}

BOOST_AUTO_TEST_SUITE_END()
