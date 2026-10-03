/** @file param_schema.cpp
 * @brief 实现 ParamSchema：字段列派生、方言 DDL 生成、后端检测。
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
// 元数据列名固定，供 ParamStore 与 Lua 层共用。
constexpr const char* kColId = "param_id";
constexpr const char* kColKey = "param_key";
constexpr const char* kColVersion = "schema_version";
constexpr const char* kColCreated = "created_at";
constexpr const char* kColUpdated = "updated_at";

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

std::string KeyColumnType(Backend backend)
{
    // MySQL 的 TEXT 不能直接 UNIQUE，需定长 VARCHAR。
    return backend == Backend::MySQL ? "VARCHAR(512) NOT NULL UNIQUE" : "TEXT NOT NULL UNIQUE";
}

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
    // int / int64_t / 任意受支持枚举 -> Int64
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
    // PG 交由 ExistenceCheck 前置判断后不再携带 IF NOT EXISTS；SQLite/MySQL 直接携带。
    if (backend != Backend::PostgreSQL)
        sql << "IF NOT EXISTS ";
    sql << table << " (";
    auto ddl = BuildDdl(backend);
    bool first = true;
    // 固定顺序输出，避免 unordered_map 哈希序在不同进程/版本间漂移。
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
