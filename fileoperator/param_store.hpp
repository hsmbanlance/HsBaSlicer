/** @file param_store.hpp
 * @brief ParamStore: main class of the process-parameter persistence pipeline.
 *
 * Field traversal is driven by AnyObject::ForeachField; rows are stored in one wide table per
 * PipelineConfig type. Provides full CRUD through EnsureSchema / Save / Load / List / Update /
 * Delete; Save runs a staged Validate -> Reflect -> Coerce -> Upsert flow, batch APIs are wrapped
 * in a transaction, and errors are never swallowed: last_error is recorded and the exception is
 * rethrown so callers (including Lua) can decide how to handle it.
 */
#pragma once
#ifndef HSBA_SLICER_PARAM_STORE_HPP
#define HSBA_SLICER_PARAM_STORE_HPP

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/any_object.hpp"
#include "param_convert.hpp"
#include "param_reflect.hpp"
#include "param_schema.hpp"
#include "sql_adapter.hpp"

namespace HsBa::Slicer
{
/**
 * @brief Process-parameter store bound to a single ISQLAdapter.
 */
class ParamStore
{
public:
    /** @brief Progress callback: percent (0-100), stage description. */
    using ProgressFn = std::function<void(int, const char*)>;

    /** @brief Migration hook: runs for the given backend/table/schema version; currently only a lightweight pointer is retained. */
    using MigrationFn = std::function<void(SQL::ISQLAdapter&, Backend, const std::string&, int64_t)>;

    /** @brief When backend is std::nullopt it is auto-detected via ParamSchema::DetectBackend. */
    explicit ParamStore(SQL::ISQLAdapter& db, std::optional<Backend> backend = std::nullopt);

    /** @brief Create tables for all registered Config types (IF NOT EXISTS / existence check). */
    void EnsureSchema();

    /** @brief Create the table for the given tag. */
    void EnsureTable(PipelineConfigTag tag);

    /**
     * @brief Save (upsert) one Config.
     * @param table Target table name; when empty it is derived from the cfg type.
     * @param key   Business unique key.
     * @param cfg   AnyObject wrapping a registered PipelineConfig struct (non-owning).
     * @return param_id of the persisted row.
     */
    int64_t Save(std::string_view table, std::string_view key, Utils::AnyObject cfg);

    /** @brief Batch save wrapped in a single transaction; any failure rolls back and throws. Returns each param_id. */
    std::vector<int64_t> SaveBatch(std::string_view table,
                                   const std::vector<std::pair<std::string, Utils::AnyObject>>& items);

    /**
     * @brief Load by key into outCfg (a non-owning AnyObject wrapping the target struct).
     * @param arena Backing storage for const char* fields; must outlive the target struct.
     * @return true when a row is found, false otherwise.
     */
    bool Load(std::string_view table, std::string_view key, Utils::AnyObject outCfg, StringArena& arena);

    /** @brief List the param_key of rows matching whereJson (an object whose keys are column names and values are scalars). */
    std::vector<std::string> List(std::string_view table, const std::string& whereJson);

    /** @brief Update only the fields listed in changedFields; returns false when the key does not exist. */
    bool Update(std::string_view table, std::string_view key, Utils::AnyObject partial,
                const std::vector<std::string>& changedFields);

    /** @brief Delete by key; returns whether a row was hit (judged by the row count before and after deletion). */
    bool Delete(std::string_view table, std::string_view key);

    /// @brief Install the progress callback.
    void SetProgressCallback(ProgressFn fn) { progress_ = std::move(fn); }
    /// @brief Install the migration hook.
    void SetMigrationHook(MigrationFn fn) { migration_ = std::move(fn); }

    /** @brief Message of the most recent failure (errors are not swallowed; the exception still propagates). */
    std::string GetLastError() const { return last_error_; }

    /// @brief The backend this store targets.
    Backend backend() const noexcept { return backend_; }

    /** @brief Resolve the table name: returned as-is when non-empty, otherwise a default name is derived from the cfg type. */
    static std::string ResolveTable(std::string_view table, Utils::AnyObject cfg);

private:
    void Raise(int percent, const char* stage);
    template <typename Fn>
    auto Guard(Fn&& fn) -> decltype(fn());

    SQL::ISQLAdapter* db_;
    Backend backend_;
    ProgressFn progress_;
    MigrationFn migration_;
    std::string last_error_;
};
}  // namespace HsBa::Slicer

#endif  // HSBA_SLICER_PARAM_STORE_HPP
