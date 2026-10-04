/** @file param_store_ops.cpp
 * @brief Implements the Lib-layer ParamStore write/read wrappers; see param_store_ops.hpp.
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
// Lib kind -> fileoperator PipelineConfigTag (explicit switch, not relying on the underlying integer order).
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

// Run Save on an adapter already connected to a concrete backend; exceptions are converged into the outcome here and not rethrown.
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

// Run Load on an adapter already connected to a concrete backend; on a hit convert the arena strings to malloc ownership.
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
        // The arena (new[]) is destroyed with its scope; first swap the const char* to malloc ownership so each can be freed across the C ABI.
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
