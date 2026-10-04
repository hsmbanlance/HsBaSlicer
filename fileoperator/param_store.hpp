/** @file param_store.hpp
 * @brief ParamStore：工艺参数保存流水线主类。
 *
 * 以 AnyObject::ForeachField 驱动字段遍历，按 PipelineConfig 类型分表（宽表）存储；
 * 提供 EnsureSchema / Save / Load / List / Update / Delete 全 CRUD，Save 走
 * Validate -> Reflect -> Coerce -> Upsert 分阶段流程；批量接口以事务包裹；
 * 错误不被吞没：内部记录 last_error 后原样抛出，供调用方（含 Lua）择路处理。
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
 * @brief 绑定单一 ISQLAdapter 的工艺参数存储。
 */
class ParamStore
{
public:
    /** @brief 进度回调：percent(0-100), stage 描述。 */
    using ProgressFn = std::function<void(int, const char*)>;

    /** @brief 迁移钩子：给定后端/表/schema 版本时执行迁移；本期仅保留轻量指针。 */
    using MigrationFn = std::function<void(SQL::ISQLAdapter&, Backend, const std::string&, int64_t)>;

    /** @brief backend 传 std::nullopt 时由 ParamSchema::DetectBackend 自动检测。 */
    explicit ParamStore(SQL::ISQLAdapter& db, std::optional<Backend> backend = std::nullopt);

    /** @brief 为所有已注册 Config 表建表（IF NOT EXISTS / 存在性检查）。 */
    void EnsureSchema();

    /** @brief 为指定 tag 建表。 */
    void EnsureTable(PipelineConfigTag tag);

    /**
     * @brief 保存（upsert）一个 Config。
     * @param table 目标表名；传空则由 cfg 的类型派生。
     * @param key   业务唯一键。
     * @param cfg   包裹某个已注册 PipelineConfig 结构体的 AnyObject（非拥有）。
     * @return 落库行的 param_id。
     */
    int64_t Save(std::string_view table, std::string_view key, Utils::AnyObject cfg);

    /** @brief 批量保存，整批包在一个事务里；任一条失败回滚并抛出。返回各条 param_id。 */
    std::vector<int64_t> SaveBatch(std::string_view table,
                                   const std::vector<std::pair<std::string, Utils::AnyObject>>& items);

    /**
     * @brief 按 key 加载到 outCfg（包裹目标结构体的非拥有 AnyObject）。
     * @param arena const char* 字段的字符串持有者，需比目标结构体活得更久。
     * @return 命中返回 true；未命中返回 false。
     */
    bool Load(std::string_view table, std::string_view key, Utils::AnyObject outCfg, StringArena& arena);

    /** @brief 列出满足 whereJson（对象，键为列名，值为标量）的行的 param_key 列表。 */
    std::vector<std::string> List(std::string_view table, const std::string& whereJson);

    /** @brief 仅更新 changedFields 列出的字段；key 不存在返回 false。 */
    bool Update(std::string_view table, std::string_view key, Utils::AnyObject partial,
                const std::vector<std::string>& changedFields);

    /** @brief 按 key 删除；返回是否命中（以删除前后行数变化判定）。 */
    bool Delete(std::string_view table, std::string_view key);

    void SetProgressCallback(ProgressFn fn) { progress_ = std::move(fn); }
    void SetMigrationHook(MigrationFn fn) { migration_ = std::move(fn); }

    /** @brief 最近一次失败的错误信息（不吞错误，异常仍会向外传播）。 */
    std::string GetLastError() const { return last_error_; }

    Backend backend() const noexcept { return backend_; }

    /** @brief 解析表名：非空则原样返回，否则由 cfg 的类型派生默认表名。 */
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
