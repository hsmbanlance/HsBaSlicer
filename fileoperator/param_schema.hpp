/** @file param_schema.hpp
 * @brief Schema 层：由 Config 结构体的 TypeInfo::fields 派生每张宽表的列定义，
 *        针对 SQLite / MySQL / PostgreSQL 三种后端产出方言 DDL 与列类型，并提供后端检测。
 *
 * 每个 Config 表固定追加元数据列：param_id（自增主键）、param_key（业务唯一键）、
 * schema_version、created_at、updated_at。字段列类型按收敛后的 std::any 种类映射：
 *   Double(double/float)  ->  REAL / DOUBLE / DOUBLE PRECISION
 *   Int64 (int/enum/int64)->  BIGINT / BIGINT / BIGINT
 *   Text  (const char*)   ->  TEXT / TEXT / TEXT
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
/** @brief SQL 后端种类。移动端恒定使用 SQLite。 */
enum class Backend
{
    SQLite,
    MySQL,
    PostgreSQL,
};

/** @brief 单个字段的收敛后种类。 */
enum class ColumnKind
{
    Double,
    Int64,
    Text,
};

/** @brief 列的元信息描述。 */
struct ColumnDef
{
    std::string name;
    std::string comment;
    bool nullable = true;
    bool is_key = false;
};

/**
 * @brief 由某个 Config 结构体的反射字段派生的表 Schema。
 *
 * 实例通过 forConfig(TypeInfo*) 获取并按 TypeInfo* 缓存，构造即遍历 fields 计算列种类。
 */
class ParamSchema
{
public:
    /** @brief 取得（并缓存）指定 Config 类型对应的 Schema。cfgType 必须是已注册的 PipelineConfig。 */
    static ParamSchema& forConfig(Utils::TypeInfo* cfgType);

    /** @brief 依据目标后端产出「列名 -> 完整列定义」映射（含元数据列），可直接喂给 CreateTable。 */
    std::unordered_map<std::string, std::string> BuildDdl(Backend backend) const;

    /** @brief 产出完整 CREATE TABLE 语句；SQLite/MySQL 携带 IF NOT EXISTS，PG 交由 ExistenceCheck 前置判断。 */
    std::string CreateStatement(const std::string& table, Backend backend) const;

    /** @brief 产出「若表存在则返回 >=1 行」的检测查询。 */
    std::string ExistenceCheck(const std::string& table, Backend backend) const;

    /** @brief dynamic_cast 判定 ISQLAdapter 的实际后端；未知类型抛 SQLAdapterInvalidArgumentError。 */
    static Backend DetectBackend(const SQL::ISQLAdapter& db);

    /** @brief 返回一个字段收敛后种类。 */
    static ColumnKind KindOf(Utils::TypeInfo* field_ti);

    /** @brief 该 Config 的反射字段名（不含元数据列），按 TypeInfo::fields 顺序。 */
    const std::vector<std::string>& field_columns() const noexcept { return field_columns_; }

    const std::string& type_name() const noexcept { return type_name_; }

private:
    explicit ParamSchema(Utils::TypeInfo* cfgType);

    std::string type_name_;
    // 反射字段名 -> 列种类（按落库 std::any 种类）
    std::vector<std::pair<std::string, ColumnKind>> columns_;
    std::vector<std::string> field_columns_;
};
}  // namespace HsBa::Slicer

#endif  // HSBA_SLICER_PARAM_SCHEMA_HPP
