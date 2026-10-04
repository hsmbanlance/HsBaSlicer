/** @file param_schema.hpp
 * @brief Schema layer: derives each wide-table's column definitions from a Config struct's
 *        TypeInfo::fields, emits dialect DDL and column types for the SQLite / MySQL / PostgreSQL
 *        backends, and provides backend detection.
 *
 * Every Config table appends fixed metadata columns: param_id (auto-increment primary key),
 * param_key (business unique key), schema_version, created_at, updated_at. Field column types are
 * mapped from the converged std::any kind:
 *   Double(double/float)   -> REAL / DOUBLE / DOUBLE PRECISION
 *   Int64 (int/enum/int64) -> BIGINT / BIGINT / BIGINT
 *   Text  (const char*)    -> TEXT / TEXT / TEXT
 */
#pragma once
#ifndef HSBA_SLICER_PARAM_SCHEMA_HPP
#define HSBA_SLICER_PARAM_SCHEMA_HPP

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/any_object.hpp"
#include "sql_adapter.hpp"

namespace HsBa::Slicer
{
/** @brief SQL backend kind. Mobile targets always use SQLite. */
enum class Backend
{
    SQLite,
    MySQL,
    PostgreSQL,
};

/** @brief Converged kind of a single field, used to pick a column type. */
enum class ColumnKind
{
    Double,
    Int64,
    Text,
};

/** @brief Metadata describing one table column. */
struct ColumnDef
{
    std::string name;
    std::string comment;
    bool nullable = true;
    bool is_key = false;
};

/**
 * @brief Table schema derived from a Config struct's reflected fields.
 *
 * Instances are obtained through forConfig(TypeInfo*) and cached per TypeInfo*; construction walks
 * the reflected fields to compute each column kind.
 */
class ParamSchema
{
public:
    /** @brief Get (and cache) the Schema for the given Config type; cfgType must be a registered PipelineConfig. */
    static ParamSchema& forConfig(Utils::TypeInfo* cfgType);

    /** @brief Build the "column name -> full column definition" map for the target backend (metadata columns included), ready for CreateTable. */
    std::unordered_map<std::string, std::string> BuildDdl(Backend backend) const;

    /** @brief Emit the full CREATE TABLE statement; SQLite/MySQL carry IF NOT EXISTS, PG relies on a prior ExistenceCheck. */
    std::string CreateStatement(const std::string& table, Backend backend) const;

    /** @brief Emit a probe query that returns at least one row when the table already exists. */
    std::string ExistenceCheck(const std::string& table, Backend backend) const;

    /** @brief Detect the concrete ISQLAdapter backend via dynamic_cast; throws SQLAdapterInvalidArgumentError for unknown types. */
    static Backend DetectBackend(const SQL::ISQLAdapter& db);

    /** @brief Return the converged column kind for a field type. */
    static ColumnKind KindOf(Utils::TypeInfo* field_ti);

    /** @brief Reflected field names of this Config (excluding metadata columns), in TypeInfo::fields order. */
    const std::vector<std::string>& field_columns() const noexcept { return field_columns_; }

    /// @brief Reflected type name of the Config this schema was derived from.
    const std::string& type_name() const noexcept { return type_name_; }

private:
    explicit ParamSchema(Utils::TypeInfo* cfgType);

    std::string type_name_;
    // reflected field name -> column kind (by the std::any kind actually stored)
    std::vector<std::pair<std::string, ColumnKind>> columns_;
    std::vector<std::string> field_columns_;
};
}  // namespace HsBa::Slicer

#endif  // HSBA_SLICER_PARAM_SCHEMA_HPP
