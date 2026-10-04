/** @file param_store_test.cpp
 * @brief ParamStore 工艺参数流水线测试：反射完整性 / 类型收敛往返 / SQLite CRUD /
 *        Lua 冒烟 / 批量事务回滚 / Lib 层写入读取往返 / C 导出冒烟 / 后端可用性守卫。
 *        MySQL / PGSQL 实连用例仅在对应宏开启且设置环境变量时启用。
 */
#define BOOST_TEST_MODULE param_store_test
#include <boost/test/included/unit_test.hpp>

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <unordered_map>

#include <lua.hpp>

#include "DllHsBaSlicer/param_store_pipeline.h"
#include "LibHsBaSlicer/ParamStore/param_store_ops.hpp"
#include "fileoperator/LuaAdapter.hpp"
#include "fileoperator/param_convert.hpp"
#include "fileoperator/param_reflect.hpp"
#include "fileoperator/param_schema.hpp"
#include "fileoperator/param_store.hpp"
#include "fileoperator/sql_adapter.hpp"

using namespace HsBa::Slicer;
using Utils::AnyObject;
using Utils::GetTypeInfo;
using Utils::TypeInfo;

namespace
{
// 反射整个 Config -> {列名 -> 收敛后 std::any}。
std::unordered_map<std::string, std::any> ReflectAnyObject(AnyObject obj)
{
    std::unordered_map<std::string, std::any> m;
    obj.ForeachField([&m](std::string_view name, AnyObject child)
                     { m[std::string(name)] = FieldToAny(name, child.get_type_info(), child.get_data()); });
    return m;
}

bool AnyEquals(const std::any& a, const std::any& b)
{
    const bool a_null = !a.has_value() || a.type() == typeid(std::nullptr_t);
    const bool b_null = !b.has_value() || b.type() == typeid(std::nullptr_t);
    if (a_null || b_null)
        return a_null && b_null;
    if (a.type() != b.type())
        return false;
    if (a.type() == typeid(double))
        return std::fabs(std::any_cast<double>(a) - std::any_cast<double>(b)) < 1e-9;
    if (a.type() == typeid(int64_t))
        return std::any_cast<int64_t>(a) == std::any_cast<int64_t>(b);
    if (a.type() == typeid(std::string))
        return std::any_cast<const std::string&>(a) == std::any_cast<const std::string&>(b);
    return false;
}

bool AnyMapsEqual(const std::unordered_map<std::string, std::any>& a,
                  const std::unordered_map<std::string, std::any>& b)
{
    if (a.size() != b.size())
        return false;
    for (const auto& [k, v] : a)
    {
        auto it = b.find(k);
        if (it == b.end() || !AnyEquals(v, it->second))
            return false;
    }
    return true;
}

std::filesystem::path TempDbPath(const std::string& tag)
{
    return std::filesystem::temp_directory_path() / ("hsba_param_store_" + tag + ".db");
}

// SQLite 句柄析构前文件仍被占用，remove 可能失败；测试中以非抛异常版本忽略。
void RemoveQuiet(const std::filesystem::path& p)
{
    std::error_code ec;
    std::filesystem::remove(p, ec);
}
}  // namespace

BOOST_AUTO_TEST_SUITE(param_store_suite)

// 1. 反射完整性
BOOST_AUTO_TEST_CASE(reflection_completeness)
{
    RegisterPipelineConfigTypes();

    auto* fdm = GetTypeInfo<HsBaFdmPipelineConfig_t>();
    auto* sla = GetTypeInfo<HsBaSlaPipelineConfig_t>();
    BOOST_CHECK(IsPipelineConfigType(fdm));
    BOOST_CHECK_EQUAL(fdm->fields.size(), 37u);
    BOOST_CHECK_EQUAL(sla->fields.size(), 32u);
    BOOST_CHECK(fdm->fields.count("layer_height") == 1);
    BOOST_CHECK(fdm->fields.count("model_name") == 1);
    BOOST_CHECK(fdm->fields.count("fill_mode") == 1);

    // FileTransfer 的 file_paths 故意不注册（const char**）
    auto* ft = GetTypeInfo<HsBaFileTransferPipelineConfig_t>();
    BOOST_CHECK(ft->fields.count("file_paths") == 0);
    BOOST_CHECK(ft->fields.count("host") == 1);

    BOOST_CHECK(TagOf(fdm) == PipelineConfigTag::Fdm);
    BOOST_CHECK_EQUAL(DefaultTableName(PipelineConfigTag::Fdm), "hsba_param_fdm");
    BOOST_CHECK(TagFromName("sla") == PipelineConfigTag::Sla);
    BOOST_CHECK(TagFromName("nonsense") == PipelineConfigTag::Unknown);
    BOOST_CHECK(TagOf(GetTypeInfo<int>()) == PipelineConfigTag::Unknown);
}

// 2. 类型收敛往返
BOOST_AUTO_TEST_CASE(convert_round_trip)
{
    RegisterPipelineConfigTypes();

    HsBaFdmPipelineConfig_t src = HsBaFdmConfigDefault();
    src.model_name = "roundtrip_box";
    src.model_path = nullptr;  // NULL const char*
    src.layer_height = 0.15f;  // float
    src.fill_spacing = 0.475;  // double
    src.wall_count = 5;        // int
    src.fill_mode = HSBA_FILL_LINE;
    src.support_pattern = HSBA_SUPPORT_TREE;
    src.enable_support = 0;

    auto src_map = ReflectAnyObject(AnyObject(GetTypeInfo<HsBaFdmPipelineConfig_t>(), &src));

    HsBaFdmPipelineConfig_t dst = HsBaFdmConfigDefault();
    StringArena arena;
    AnyObject(GetTypeInfo<HsBaFdmPipelineConfig_t>(), &dst)
        .ForeachField(
            [&](std::string_view name, AnyObject child)
            {
                auto it = src_map.find(std::string(name));
                if (it != src_map.end())
                    AnyToField(it->second, child.get_type_info(), child.get_data(), arena);
            });

    auto dst_map = ReflectAnyObject(AnyObject(GetTypeInfo<HsBaFdmPipelineConfig_t>(), &dst));
    BOOST_CHECK(AnyMapsEqual(src_map, dst_map));

    BOOST_CHECK_EQUAL(std::string(dst.model_name), "roundtrip_box");
    BOOST_CHECK(dst.model_path == nullptr);
    BOOST_CHECK_SMALL(dst.layer_height - 0.15f, 1e-6f);
    BOOST_CHECK_EQUAL(dst.wall_count, 5);
    BOOST_CHECK(dst.fill_mode == HSBA_FILL_LINE);
    BOOST_CHECK(dst.support_pattern == HSBA_SUPPORT_TREE);
    BOOST_CHECK_EQUAL(dst.enable_support, 0);
}

// 3. SQLite CRUD 端到端
BOOST_AUTO_TEST_CASE(sqlite_crud)
{
    RegisterPipelineConfigTypes();
    auto path = TempDbPath("crud");
    RemoveQuiet(path);

    SQL::SQLiteAdapter db;
    db.Connect(path.string());
    ParamStore store(db);
    BOOST_CHECK(store.backend() == Backend::SQLite);
    store.EnsureSchema();
    // 幂等
    store.EnsureSchema();

    HsBaFdmPipelineConfig_t cfg = HsBaFdmConfigDefault();
    cfg.model_name = "crud_box";
    cfg.layer_height = 0.28f;
    cfg.wall_count = 7;
    cfg.output_path = nullptr;

    int64_t id = store.Save("", "cfg1", AnyObject(GetTypeInfo<HsBaFdmPipelineConfig_t>(), &cfg));
    BOOST_CHECK(id > 0);

    // Load
    HsBaFdmPipelineConfig_t loaded = HsBaFdmConfigDefault();
    StringArena arena;
    bool ok = store.Load("", "cfg1", AnyObject(GetTypeInfo<HsBaFdmPipelineConfig_t>(), &loaded), arena);
    BOOST_CHECK(ok);
    BOOST_CHECK_EQUAL(std::string(loaded.model_name), "crud_box");
    BOOST_CHECK_SMALL(loaded.layer_height - 0.28f, 1e-6f);
    BOOST_CHECK_EQUAL(loaded.wall_count, 7);
    BOOST_CHECK(loaded.output_path == nullptr);

    // 未命中
    HsBaFdmPipelineConfig_t miss = HsBaFdmConfigDefault();
    StringArena arena2;
    BOOST_CHECK(!store.Load("", "no_such_key", AnyObject(GetTypeInfo<HsBaFdmPipelineConfig_t>(), &miss), arena2));

    // List
    auto keys = store.List("hsba_param_fdm", "");
    BOOST_CHECK_EQUAL(keys.size(), 1u);
    BOOST_CHECK_EQUAL(keys[0], "cfg1");

    // List with where (schema_version = 1)
    auto filtered = store.List("hsba_param_fdm", R"({"schema_version":1})");
    BOOST_CHECK_EQUAL(filtered.size(), 1u);

    // Update：仅改 wall_count
    cfg.wall_count = 9;
    BOOST_CHECK(store.Update("", "cfg1", AnyObject(GetTypeInfo<HsBaFdmPipelineConfig_t>(), &cfg), {"wall_count"}));
    HsBaFdmPipelineConfig_t reloaded = HsBaFdmConfigDefault();
    StringArena arena3;
    BOOST_CHECK(store.Load("", "cfg1", AnyObject(GetTypeInfo<HsBaFdmPipelineConfig_t>(), &reloaded), arena3));
    BOOST_CHECK_EQUAL(reloaded.wall_count, 9);

    // Save upsert：同 key 再存，id 不变
    cfg.layer_height = 0.30f;
    int64_t id2 = store.Save("", "cfg1", AnyObject(GetTypeInfo<HsBaFdmPipelineConfig_t>(), &cfg));
    BOOST_CHECK_EQUAL(id2, id);
    auto after_upsert = store.List("hsba_param_fdm", "");
    BOOST_CHECK_EQUAL(after_upsert.size(), 1u);

    // Delete
    BOOST_CHECK(store.Delete("hsba_param_fdm", "cfg1"));
    BOOST_CHECK(!store.Delete("hsba_param_fdm", "cfg1"));  // 再次删除未命中
    BOOST_CHECK(store.List("hsba_param_fdm", "").empty());

    RemoveQuiet(path);
}

// 4. Lua 冒烟：PascalCase 与错误返回
BOOST_AUTO_TEST_CASE(lua_smoke)
{
    RegisterPipelineConfigTypes();
    auto path = TempDbPath("lua");
    RemoveQuiet(path);

    SQL::SQLiteAdapter db;
    db.Connect(path.string());

    lua_State* L = luaL_newstate();
    BOOST_REQUIRE(L != nullptr);
    luaL_openlibs(L);
    HsBa::Slicer::RegisterLuaSQLiteAdapter(L);
    HsBa::Slicer::RegisterLuaParamStore(L);

    // 复用同一个 db 指针：以 lightuserdata 方式不便，改为在 Lua 里重新 new+Connect 同一路径。
    // 使用正斜杠避免 Windows 反斜杠在 Lua 字符串字面量中被当作非法转义序列。
    const std::string chunk = R"lua(
        local ldb = SQLiteAdapter.new()
        ldb:Connect(")lua" + path.generic_string() +
                              R"lua(")
        local store = ParamStore.new(ldb)
        store:EnsureSchema()
        local id = store:Save("fdm", "luacfg1", {
            model_name = "lua_box",
            layer_height = 0.3,
            wall_count = 4,
            fill_mode = 2,
        })
        assert(id and id > 0, "save returned bad id")
        local ok, tbl = store:Load("fdm", "luacfg1")
        assert(ok, "load failed")
        assert(math.abs(tbl.layer_height - 0.3) < 1e-6, "layer_height mismatch")
        assert(tbl.wall_count == 4, "wall_count mismatch")
        assert(tbl.model_name == "lua_box", "model_name mismatch")
        local keys = store:List("fdm", "")
        assert(#keys == 1, "list count mismatch")
        assert(store:Delete("fdm", "luacfg1") == true, "delete failed")
        -- 未命中：Load 返回 (false, err)
        local ok2, err = store:Load("fdm", "ghost")
        assert(ok2 == false and type(err) == "string", "expected miss")
        return 1
    )lua";

    int rc = luaL_dostring(L, chunk.c_str());
    if (rc != LUA_OK)
    {
        const char* err = lua_tostring(L, -1);
        BOOST_TEST_MESSAGE("Lua error: " << (err ? err : "unknown"));
    }
    BOOST_CHECK_EQUAL(rc, LUA_OK);

    lua_close(L);
    RemoveQuiet(path);
}

// 5. 批量事务：中途失败 -> ROLLBACK，全批不落库
BOOST_AUTO_TEST_CASE(batch_transaction_rollback)
{
    RegisterPipelineConfigTypes();
    auto path = TempDbPath("batch");
    RemoveQuiet(path);

    SQL::SQLiteAdapter db;
    db.Connect(path.string());
    ParamStore store(db);
    store.EnsureTable(PipelineConfigTag::Fdm);

    HsBaFdmPipelineConfig_t a = HsBaFdmConfigDefault();
    a.model_name = "a";
    HsBaFdmPipelineConfig_t b = HsBaFdmConfigDefault();
    b.model_name = "b";

    std::vector<std::pair<std::string, AnyObject>> items;
    items.emplace_back("k1", AnyObject(GetTypeInfo<HsBaFdmPipelineConfig_t>(), &a));
    items.emplace_back("k2", AnyObject(GetTypeInfo<HsBaFdmPipelineConfig_t>(), &b));
    // 第三条：非法类型，触发 Save 抛出 -> 回滚
    int junk = 0;
    items.emplace_back("k3", AnyObject(GetTypeInfo<int>(), &junk));

    bool threw = false;
    try
    {
        store.SaveBatch("hsba_param_fdm", items);
    }
    catch (const std::exception&)
    {
        threw = true;
    }
    BOOST_CHECK(threw);
    BOOST_CHECK_EQUAL(store.List("hsba_param_fdm", "").size(), 0u);

    // 正常批量：全部落库
    std::vector<std::pair<std::string, AnyObject>> good;
    good.emplace_back("k1", AnyObject(GetTypeInfo<HsBaFdmPipelineConfig_t>(), &a));
    good.emplace_back("k2", AnyObject(GetTypeInfo<HsBaFdmPipelineConfig_t>(), &b));
    auto ids = store.SaveBatch("hsba_param_fdm", good);
    BOOST_CHECK_EQUAL(ids.size(), 2u);
    BOOST_CHECK_EQUAL(store.List("hsba_param_fdm", "").size(), 2u);

    RemoveQuiet(path);
}

// 6. Schema 方言 DDL 与后端检测
BOOST_AUTO_TEST_CASE(schema_dialect)
{
    RegisterPipelineConfigTypes();
    TypeInfo* fdm = GetTypeInfo<HsBaFdmPipelineConfig_t>();
    const ParamSchema& schema = ParamSchema::forConfig(fdm);

    auto sqlite_ddl = schema.CreateStatement("hsba_param_fdm", Backend::SQLite);
    BOOST_CHECK(sqlite_ddl.find("IF NOT EXISTS") != std::string::npos);
    BOOST_CHECK(sqlite_ddl.find("AUTOINCREMENT") != std::string::npos);
    BOOST_CHECK(sqlite_ddl.find("REAL") != std::string::npos);

    auto pg_ddl = schema.CreateStatement("hsba_param_fdm", Backend::PostgreSQL);
    BOOST_CHECK(pg_ddl.find("IF NOT EXISTS") == std::string::npos);  // PG 走存在性检查
    BOOST_CHECK(pg_ddl.find("BIGSERIAL") != std::string::npos);
    BOOST_CHECK(pg_ddl.find("DOUBLE PRECISION") != std::string::npos);

    auto my_ddl = schema.CreateStatement("hsba_param_fdm", Backend::MySQL);
    BOOST_CHECK(my_ddl.find("AUTO_INCREMENT") != std::string::npos);
    BOOST_CHECK(my_ddl.find("VARCHAR(512) NOT NULL UNIQUE") != std::string::npos);

    // 后端检测
    SQL::SQLiteAdapter db;
    BOOST_CHECK(ParamSchema::DetectBackend(db) == Backend::SQLite);
}

// 7. Lib 层写入/读取往返（SavePipelineParams / LoadPipelineParams）
BOOST_AUTO_TEST_CASE(lib_param_ops_round_trip)
{
    auto path = TempDbPath("libops");
    RemoveQuiet(path);

    ParamStoreConn conn;
    conn.backend = ParamBackend::Sqlite;
    conn.sqlitePath = path.string();

    HsBaFdmPipelineConfig_t src = HsBaFdmConfigDefault();
    src.model_name = "lib_box";
    src.model_path = nullptr;  // NULL const char* 应保持 NULL
    src.layer_height = 0.22f;
    src.wall_count = 6;
    src.fill_mode = HSBA_FILL_LINE;

    auto save = SavePipelineParams(conn, ParamPipelineKind::Fdm, "", "lib1", &src);
    BOOST_CHECK(save.success);
    BOOST_CHECK(save.paramId > 0);

    HsBaFdmPipelineConfig_t dst = HsBaFdmConfigDefault();
    auto load = LoadPipelineParams(conn, ParamPipelineKind::Fdm, "", "lib1", &dst);
    BOOST_CHECK(load.success);
    BOOST_CHECK_EQUAL(std::string(dst.model_name), "lib_box");
    BOOST_CHECK_SMALL(dst.layer_height - 0.22f, 1e-6f);
    BOOST_CHECK_EQUAL(dst.wall_count, 6);
    BOOST_CHECK(dst.fill_mode == HSBA_FILL_LINE);
    BOOST_CHECK(dst.model_path == nullptr);
    // dst.model_name 现由库 malloc 持有，须显式释放（无崩溃 / ASan 干净）
    FreeLoadedConfigStrings(ParamPipelineKind::Fdm, &dst);
    BOOST_CHECK(dst.model_name == nullptr);

    // 未命中：success=false 且 error 非空，不吞码
    HsBaFdmPipelineConfig_t miss = HsBaFdmConfigDefault();
    auto l2 = LoadPipelineParams(conn, ParamPipelineKind::Fdm, "", "ghost", &miss);
    BOOST_CHECK(!l2.success);
    BOOST_CHECK(!l2.error.empty());

    RemoveQuiet(path);
}

// 8. C 导出冒烟（HsBaSavePipelineParams / HsBaLoadPipelineParams / 释放）
BOOST_AUTO_TEST_CASE(c_api_param_store_smoke)
{
    auto path = TempDbPath("capi");
    RemoveQuiet(path);
    const std::string dbstr = path.string();

    HsBaParamStoreConn_t conn{};
    conn.backend = HSBA_PARAM_BACKEND_SQLITE;
    conn.sqlite_path = dbstr.c_str();
    conn.host = nullptr;
    conn.user = nullptr;
    conn.password = nullptr;
    conn.database = nullptr;
    conn.port = 0;

    HsBaFdmPipelineConfig_t cfg = HsBaFdmConfigDefault();
    cfg.model_name = "capi_box";
    cfg.layer_height = 0.33f;
    cfg.wall_count = 3;

    HsBaParamStoreResult_t r1 = HsBaSavePipelineParams(&conn, HSBA_PIPELINE_FDM, nullptr, "c1", &cfg);
    BOOST_CHECK_EQUAL(r1.success, 1);
    BOOST_CHECK(r1.param_id > 0);
    BOOST_CHECK(r1.error_message == nullptr);
    HsBaFreeParamStoreResult(&r1);

    HsBaFdmPipelineConfig_t out = HsBaFdmConfigDefault();
    HsBaParamStoreResult_t r2 = HsBaLoadPipelineParams(&conn, HSBA_PIPELINE_FDM, nullptr, "c1", &out);
    BOOST_CHECK_EQUAL(r2.success, 1);
    BOOST_CHECK_EQUAL(std::string(out.model_name), "capi_box");
    BOOST_CHECK_EQUAL(out.wall_count, 3);
    HsBaFreeLoadedPipelineConfig(HSBA_PIPELINE_FDM, &out);
    HsBaFreeParamStoreResult(&r2);

    // 未命中 key：success=0 + 非空 error_message
    HsBaFdmPipelineConfig_t miss = HsBaFdmConfigDefault();
    HsBaParamStoreResult_t r3 = HsBaLoadPipelineParams(&conn, HSBA_PIPELINE_FDM, nullptr, "ghost", &miss);
    BOOST_CHECK_EQUAL(r3.success, 0);
    BOOST_CHECK(r3.error_message != nullptr);
    HsBaFreeParamStoreResult(&r3);

    // 空指针入参守卫：不崩溃，返回 success=0
    HsBaParamStoreResult_t r4 = HsBaSavePipelineParams(nullptr, HSBA_PIPELINE_FDM, nullptr, "x", &cfg);
    BOOST_CHECK_EQUAL(r4.success, 0);
    BOOST_CHECK(r4.error_message != nullptr);
    HsBaFreeParamStoreResult(&r4);

    RemoveQuiet(path);
}

// 9. 后端可用性守卫：未编译进的 MySQL/PGSQL 安全返回 success=false（不崩溃）
//    注意：MySQL/PostgreSQLAdapter 在无实例时 Connect 会崩溃而非抛异常，
//    因此编译进对应宏时必须仅在有实例（环境变量）时才发起调用。
BOOST_AUTO_TEST_CASE(param_store_backend_guard)
{
#ifdef HSBA_USE_MYSQL
    if (std::getenv("HSBA_TEST_MYSQL"))
    {
        ParamStoreConn conn;
        conn.backend = ParamBackend::MySql;
        conn.host = "localhost";
        conn.user = "root";
        conn.database = "hsba_test";
        HsBaFdmPipelineConfig_t cfg = HsBaFdmConfigDefault();
        cfg.model_name = "guard_mysql";
        auto o = SavePipelineParams(conn, ParamPipelineKind::Fdm, "", "guard_mysql", &cfg);
        BOOST_CHECK(o.success);
    }
    else
    {
        BOOST_TEST_MESSAGE("skip live mysql in guard test (HSBA_TEST_MYSQL unset)");
    }
#else
    {
        // 未编译进：Lib 走 BackendUnavailable 安全路径，success=false 且 error 非空
        ParamStoreConn conn;
        conn.backend = ParamBackend::MySql;
        conn.host = "localhost";
        conn.user = "root";
        conn.database = "hsba_test";
        HsBaFdmPipelineConfig_t cfg = HsBaFdmConfigDefault();
        auto o = SavePipelineParams(conn, ParamPipelineKind::Fdm, "", "guard_mysql", &cfg);
        BOOST_CHECK(!o.success);
        BOOST_CHECK(!o.error.empty());
    }
#endif

#ifdef HSBA_USE_PGSQL
    if (std::getenv("HSBA_TEST_PGSQL"))
    {
        ParamStoreConn conn;
        conn.backend = ParamBackend::PostgreSql;
        conn.host = "localhost";
        conn.user = "postgres";
        conn.database = "hsba_test";
        HsBaFdmPipelineConfig_t cfg = HsBaFdmConfigDefault();
        cfg.model_name = "guard_pg";
        auto o = SavePipelineParams(conn, ParamPipelineKind::Fdm, "", "guard_pg", &cfg);
        BOOST_CHECK(o.success);
    }
    else
    {
        BOOST_TEST_MESSAGE("skip live postgresql in guard test (HSBA_TEST_PGSQL unset)");
    }
#else
    {
        ParamStoreConn conn;
        conn.backend = ParamBackend::PostgreSql;
        conn.host = "localhost";
        conn.user = "postgres";
        conn.database = "hsba_test";
        HsBaFdmPipelineConfig_t cfg = HsBaFdmConfigDefault();
        auto o = SavePipelineParams(conn, ParamPipelineKind::Fdm, "", "guard_pg", &cfg);
        BOOST_CHECK(!o.success);
        BOOST_CHECK(!o.error.empty());
    }
#endif
}

#ifdef HSBA_USE_MYSQL
BOOST_AUTO_TEST_CASE(mysql_crud)
{
    // MySQLAdapter 在无实例时 Connect 会崩溃而非抛异常；仅在显式设置环境变量时执行。
    if (!std::getenv("HSBA_TEST_MYSQL"))
    {
        BOOST_TEST_MESSAGE("skip mysql: set HSBA_TEST_MYSQL to run against a live instance");
        return;
    }
    RegisterPipelineConfigTypes();
    SQL::MySQLAdapter db;
    try
    {
        db.Connect("localhost", "root", "", "hsba_test");
    }
    catch (const std::exception& e)
    {
        BOOST_TEST_MESSAGE("skip mysql (no instance): " << e.what());
        return;
    }
    ParamStore store(db, Backend::MySQL);
    store.EnsureTable(PipelineConfigTag::Fdm);
    HsBaFdmPipelineConfig_t cfg = HsBaFdmConfigDefault();
    cfg.model_name = "mysql_box";
    int64_t id = store.Save("hsba_param_fdm", "mysql1", AnyObject(GetTypeInfo<HsBaFdmPipelineConfig_t>(), &cfg));
    BOOST_CHECK(id > 0);
    store.Delete("hsba_param_fdm", "mysql1");
}
#endif  // HSBA_USE_MYSQL

#ifdef HSBA_USE_PGSQL
BOOST_AUTO_TEST_CASE(pgsql_crud)
{
    // PostgreSQLAdapter 在无实例时可能崩溃；仅在显式设置环境变量时执行。
    if (!std::getenv("HSBA_TEST_PGSQL"))
    {
        BOOST_TEST_MESSAGE("skip pgsql: set HSBA_TEST_PGSQL to run against a live instance");
        return;
    }
    RegisterPipelineConfigTypes();
    SQL::PostgreSQLAdapter db;
    try
    {
        db.Connect("localhost", "postgres", "", "hsba_test");
    }
    catch (const std::exception& e)
    {
        BOOST_TEST_MESSAGE("skip pgsql (no instance): " << e.what());
        return;
    }
    ParamStore store(db, Backend::PostgreSQL);
    store.EnsureTable(PipelineConfigTag::Fdm);
    HsBaFdmPipelineConfig_t cfg = HsBaFdmConfigDefault();
    cfg.model_name = "pg_box";
    int64_t id = store.Save("hsba_param_fdm", "pg1", AnyObject(GetTypeInfo<HsBaFdmPipelineConfig_t>(), &cfg));
    BOOST_CHECK(id > 0);
    store.Delete("hsba_param_fdm", "pg1");
}
#endif  // HSBA_USE_PGSQL

BOOST_AUTO_TEST_SUITE_END()
