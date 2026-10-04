/** @file param_schema.cpp
 * @brief Implements ParamSchema: field column derivation, dialect DDL generation, backend detection.
 */
#include "param_schema.hpp"

#include <memory>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

#include "param_convert.hpp"
#include "param_reflect.hpp"

namespace HsBa::Slicer
{
namespace
{
// Metadata column names are fixed and shared by ParamStore and the Lua layer.
constexpr const char* kColId = "param_id";
constexpr const char* kColKey = "param_key";
constexpr const char* kColVersion = "schema_version";
constexpr const char* kColCreated = "created_at";
constexpr const char* kColUpdated = "updated_at";

// Map a converged column kind plus backend to its concrete SQL column type name.
const char* TypeName(ColumnKind kind, Backend backend)
{
    switch (kind)
    {
    case ColumnKind::Double:
        return backend == Backend::SQLite ? "REAL" : (backend == Backend::MySQL ? "DOUBLE" : "DOUBLE PRECISION");
    case ColumnKind::Int64:
        return "BIGINT";
    case ColumnKind::Text:
        return "TEXT";
    }
    return "TEXT";
}

// Column type for the unique business key.
std::string KeyColumnType(Backend backend)
{
    // MySQL cannot declare TEXT as UNIQUE directly, so a fixed-length VARCHAR is required.
    return backend == Backend::MySQL ? "VARCHAR(512) NOT NULL UNIQUE" : "TEXT NOT NULL UNIQUE";
}

// Column type for the auto-increment primary key.
std::string IdColumnType(Backend backend)
{
    switch (backend)
    {
    case Backend::SQLite:
        return "INTEGER PRIMARY KEY AUTOINCREMENT";
    case Backend::MySQL:
        return "BIGINT PRIMARY KEY AUTO_INCREMENT";
    case Backend::PostgreSQL:
        return "BIGSERIAL PRIMARY KEY";
    }
    return "BIGINT PRIMARY KEY";
}
}  // namespace

ColumnKind ParamSchema::KindOf(Utils::TypeInfo* field_ti)
{
    if (!field_ti)
        throw SQL::SQLAdapterInvalidArgumentError("ParamSchema::KindOf: null field type");
    if (field_ti == Utils::GetTypeInfo<float>() || field_ti == Utils::GetTypeInfo<double>())
        return ColumnKind::Double;
    if (field_ti == Utils::GetTypeInfo<const char*>() || field_ti == Utils::GetTypeInfo<std::string>())
        return ColumnKind::Text;
    // int / int64_t / any supported enum -> Int64
    return ColumnKind::Int64;
}

ParamSchema::ParamSchema(Utils::TypeInfo* cfgType)
{
    if (!cfgType)
        throw SQL::SQLAdapterInvalidArgumentError("ParamSchema: null config TypeInfo");
    type_name_ = std::string(cfgType->Name);
    for (const auto& [name, field] : cfgType->fields)
    {
        ColumnKind kind = KindOf(field.first);
        columns_.emplace_back(std::string(name), kind);
        field_columns_.emplace_back(std::string(name));
    }
}

ParamSchema& ParamSchema::forConfig(Utils::TypeInfo* cfgType)
{
    static std::mutex mu;
    static std::unordered_map<Utils::TypeInfo*, std::unique_ptr<ParamSchema>> cache;
    std::lock_guard<std::mutex> lock(mu);
    auto it = cache.find(cfgType);
    if (it != cache.end())
        return *it->second;
    auto schema = std::unique_ptr<ParamSchema>(new ParamSchema(cfgType));
    ParamSchema* raw = schema.get();
    cache.emplace(cfgType, std::move(schema));
    return *raw;
}

std::unordered_map<std::string, std::string> ParamSchema::BuildDdl(Backend backend) const
{
    std::unordered_map<std::string, std::string> ddl;
    ddl[kColId] = IdColumnType(backend);
    ddl[kColKey] = KeyColumnType(backend);
    ddl[kColVersion] = std::string("BIGINT NOT NULL");
    ddl[kColCreated] = "BIGINT";
    ddl[kColUpdated] = "BIGINT";
    for (const auto& [name, kind] : columns_)
    {
        ddl[name] = TypeName(kind, backend);
    }
    return ddl;
}

std::string ParamSchema::ExistenceCheck(const std::string& table, Backend backend) const
{
    switch (backend)
    {
    case Backend::SQLite:
        return "SELECT name FROM sqlite_master WHERE type='table' AND name='" + table + "'";
    case Backend::MySQL:
        return "SELECT TABLE_NAME FROM information_schema.TABLES WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='" +
               table + "'";
    case Backend::PostgreSQL:
        return "SELECT tablename FROM pg_tables WHERE tablename='" + table + "'";
    }
    return {};
}

std::string ParamSchema::CreateStatement(const std::string& table, Backend backend) const
{
    std::ostringstream sql;
    sql << "CREATE TABLE ";
    // PG defers existence handling to a prior ExistenceCheck and so omits IF NOT EXISTS; SQLite/MySQL carry it inline.
    if (backend != Backend::PostgreSQL)
        sql << "IF NOT EXISTS ";
    sql << table << " (";
    auto ddl = BuildDdl(backend);
    bool first = true;
    // Emit in a fixed order so unordered_map hashing cannot drift column order across processes/versions.
    auto append = [&](const std::string& col)
    {
        auto it = ddl.find(col);
        if (it == ddl.end())
            return;
        if (!first)
            sql << ", ";
        first = false;
        sql << col << " " << it->second;
    };
    append(kColId);
    append(kColKey);
    append(kColVersion);
    append(kColCreated);
    append(kColUpdated);
    for (const auto& [name, kind] : columns_)
    {
        (void)kind;
        append(name);
    }
    sql << ")";
    if (backend == Backend::MySQL)
        sql << " ENGINE=InnoDB DEFAULT CHARSET=utf8mb4";
    return sql.str();
}

Backend ParamSchema::DetectBackend(const SQL::ISQLAdapter& db)
{
    if (dynamic_cast<const SQL::SQLiteAdapter*>(&db) != nullptr)
        return Backend::SQLite;
#ifdef HSBA_USE_MYSQL
    if (dynamic_cast<const SQL::MySQLAdapter*>(&db) != nullptr)
        return Backend::MySQL;
#endif
#ifdef HSBA_USE_PGSQL
    if (dynamic_cast<const SQL::PostgreSQLAdapter*>(&db) != nullptr)
        return Backend::PostgreSQL;
#endif
    throw SQL::SQLAdapterInvalidArgumentError("ParamSchema::DetectBackend: unknown ISQLAdapter implementation");
}

}  // namespace HsBa::Slicer
