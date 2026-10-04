/** @file param_store_ops.cpp
 * @brief 实现 Lib 层 ParamStore 写入/读取封装，详见 param_store_ops.hpp。
 */
#include "param_store_ops.hpp"

#include "fileoperator/param_convert.hpp"
#include "fileoperator/param_reflect.hpp"
#include "fileoperator/param_schema.hpp"
#include "fileoperator/param_store.hpp"
#include "fileoperator/sql_adapter.hpp"

namespace HsBa::Slicer
{
namespace
{
// Lib kind -> fileoperator PipelineConfigTag（显式 switch，不依赖底层整型顺序）。
PipelineConfigTag ToTag(ParamPipelineKind kind)
{
    switch (kind)
    {
    case ParamPipelineKind::Fdm:
        return PipelineConfigTag::Fdm;
    case ParamPipelineKind::Sla:
        return PipelineConfigTag::Sla;
    case ParamPipelineKind::Sls:
        return PipelineConfigTag::Sls;
    case ParamPipelineKind::Slm:
        return PipelineConfigTag::Slm;
    case ParamPipelineKind::Lom:
        return PipelineConfigTag::Lom;
    case ParamPipelineKind::Tdp:
        return PipelineConfigTag::Tdp;
    case ParamPipelineKind::Waam:
        return PipelineConfigTag::Waam;
    case ParamPipelineKind::Custom:
        return PipelineConfigTag::Custom;
    case ParamPipelineKind::FileTransfer:
        return PipelineConfigTag::FileTransfer;
    }
    return PipelineConfigTag::Unknown;
}

std::string ResolveTable(PipelineConfigTag tag, std::string_view table)
{
    return table.empty() ? std::string(DefaultTableName(tag)) : std::string(table);
}

// 已在具体后端连接好的 adapter 上执行 Save。异常在此收敛为 outcome，不向外抛出。
ParamStoreOutcome DoSave(SQL::ISQLAdapter& db, PipelineConfigTag tag, std::string_view table, std::string_view key,
                         const void* cfg)
{
    ParamStoreOutcome o;
    if (!cfg)
    {
        o.error = "ParamStore: null config pointer";
        return o;
    }
    Utils::TypeInfo* ti = TypeInfoOf(tag);
    if (!ti)
    {
        o.error = "ParamStore: unregistered pipeline kind";
        return o;
    }
    try
    {
        ParamStore store(db);
        store.EnsureTable(tag);
        Utils::AnyObject obj(ti, const_cast<void*>(cfg));
        o.paramId = static_cast<long long>(store.Save(ResolveTable(tag, table), key, obj));
        o.success = true;
    }
    catch (const SQL::SQLAdapterError& e)
    {
        o.success = false;
        o.error = e.what();
    }
    return o;
}

// 已在具体后端连接好的 adapter 上执行 Load；命中后把 arena 字符串转为 malloc 持有。
ParamStoreOutcome DoLoad(SQL::ISQLAdapter& db, PipelineConfigTag tag, std::string_view table, std::string_view key,
                         void* out_cfg)
{
    ParamStoreOutcome o;
    if (!out_cfg)
    {
        o.error = "ParamStore: null output config pointer";
        return o;
    }
    Utils::TypeInfo* ti = TypeInfoOf(tag);
    if (!ti)
    {
        o.error = "ParamStore: unregistered pipeline kind";
        return o;
    }
    try
    {
        ParamStore store(db);
        store.EnsureTable(tag);
        StringArena arena;
        Utils::AnyObject obj(ti, out_cfg);
        if (!store.Load(ResolveTable(tag, table), key, obj, arena))
        {
            o.success = false;
            o.error = "not found: " + std::string(key);
            return o;
        }
        // arena（new[]）随作用域析构；先把 const char* 换成 malloc 持有，供跨 C ABI 逐个 free。
        ConfigStringsToOwning(ti, out_cfg);
        o.success = true;
    }
    catch (const SQL::SQLAdapterError& e)
    {
        o.success = false;
        o.error = e.what();
    }
    return o;
}

ParamStoreOutcome BackendUnavailable(ParamBackend backend)
{
    ParamStoreOutcome o;
    o.error = (backend == ParamBackend::MySql) ? "ParamStore: MySQL backend not compiled in"
                                               : "ParamStore: PostgreSQL backend not compiled in";
    return o;
}

}  // namespace

ParamStoreOutcome SavePipelineParams(const ParamStoreConn& conn, ParamPipelineKind kind, std::string_view table,
                                     std::string_view key, const void* cfg)
{
    RegisterPipelineConfigTypes();
    const PipelineConfigTag tag = ToTag(kind);
    if (tag == PipelineConfigTag::Unknown)
    {
        ParamStoreOutcome o;
        o.error = "ParamStore: invalid pipeline kind";
        return o;
    }

    switch (conn.backend)
    {
    case ParamBackend::Sqlite:
    {
        SQL::SQLiteAdapter db;
        try
        {
            db.Connect(conn.sqlitePath);
        }
        catch (const SQL::SQLAdapterError& e)
        {
            ParamStoreOutcome o;
            o.error = std::string("ParamStore: sqlite connect failed: ") + e.what();
            return o;
        }
        return DoSave(db, tag, table, key, cfg);
    }
    case ParamBackend::MySql:
#ifdef HSBA_USE_MYSQL
    {
        SQL::MySQLAdapter db;
        try
        {
            if (conn.port)
                db.Connect(conn.host, conn.user, conn.password, conn.database, conn.port);
            else
                db.Connect(conn.host, conn.user, conn.password, conn.database);
        }
        catch (const SQL::SQLAdapterError& e)
        {
            ParamStoreOutcome o;
            o.error = std::string("ParamStore: mysql connect failed: ") + e.what();
            return o;
        }
        return DoSave(db, tag, table, key, cfg);
    }
#else
        return BackendUnavailable(conn.backend);
#endif
    case ParamBackend::PostgreSql:
#ifdef HSBA_USE_PGSQL
    {
        SQL::PostgreSQLAdapter db;
        try
        {
            if (conn.port)
                db.Connect(conn.host, conn.user, conn.password, conn.database, conn.port);
            else
                db.Connect(conn.host, conn.user, conn.password, conn.database);
        }
        catch (const SQL::SQLAdapterError& e)
        {
            ParamStoreOutcome o;
            o.error = std::string("ParamStore: postgresql connect failed: ") + e.what();
            return o;
        }
        return DoSave(db, tag, table, key, cfg);
    }
#else
        return BackendUnavailable(conn.backend);
#endif
    }

    ParamStoreOutcome o;
    o.error = "ParamStore: unsupported backend";
    return o;
}

ParamStoreOutcome LoadPipelineParams(const ParamStoreConn& conn, ParamPipelineKind kind, std::string_view table,
                                     std::string_view key, void* outCfg)
{
    RegisterPipelineConfigTypes();
    const PipelineConfigTag tag = ToTag(kind);
    if (tag == PipelineConfigTag::Unknown)
    {
        ParamStoreOutcome o;
        o.error = "ParamStore: invalid pipeline kind";
        return o;
    }

    switch (conn.backend)
    {
    case ParamBackend::Sqlite:
    {
        SQL::SQLiteAdapter db;
        try
        {
            db.Connect(conn.sqlitePath);
        }
        catch (const SQL::SQLAdapterError& e)
        {
            ParamStoreOutcome o;
            o.error = std::string("ParamStore: sqlite connect failed: ") + e.what();
            return o;
        }
        return DoLoad(db, tag, table, key, outCfg);
    }
    case ParamBackend::MySql:
#ifdef HSBA_USE_MYSQL
    {
        SQL::MySQLAdapter db;
        try
        {
            if (conn.port)
                db.Connect(conn.host, conn.user, conn.password, conn.database, conn.port);
            else
                db.Connect(conn.host, conn.user, conn.password, conn.database);
        }
        catch (const SQL::SQLAdapterError& e)
        {
            ParamStoreOutcome o;
            o.error = std::string("ParamStore: mysql connect failed: ") + e.what();
            return o;
        }
        return DoLoad(db, tag, table, key, outCfg);
    }
#else
        return BackendUnavailable(conn.backend);
#endif
    case ParamBackend::PostgreSql:
#ifdef HSBA_USE_PGSQL
    {
        SQL::PostgreSQLAdapter db;
        try
        {
            if (conn.port)
                db.Connect(conn.host, conn.user, conn.password, conn.database, conn.port);
            else
                db.Connect(conn.host, conn.user, conn.password, conn.database);
        }
        catch (const SQL::SQLAdapterError& e)
        {
            ParamStoreOutcome o;
            o.error = std::string("ParamStore: postgresql connect failed: ") + e.what();
            return o;
        }
        return DoLoad(db, tag, table, key, outCfg);
    }
#else
        return BackendUnavailable(conn.backend);
#endif
    }

    ParamStoreOutcome o;
    o.error = "ParamStore: unsupported backend";
    return o;
}

void FreeLoadedConfigStrings(ParamPipelineKind kind, void* cfg)
{
    RegisterPipelineConfigTypes();
    Utils::TypeInfo* ti = TypeInfoOf(ToTag(kind));
    if (ti)
        FreeConfigStrings(ti, cfg);
}

}  // namespace HsBa::Slicer
