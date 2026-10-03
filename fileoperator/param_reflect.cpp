/** @file param_reflect.cpp
 * @brief 实现 param_reflect.hpp 声明的辅助函数。
 *
 * TypeInfo 特化本体位于头文件中（必须 inline 保证 ODR），本 cpp 只提供跨 TU 唯一的
 * 注册/查询辅助逻辑。所有映射通过函数内静态数组集中维护，避免散落的 if/else。
 */
#include "param_reflect.hpp"

#include <array>
#include <mutex>

namespace HsBa::Slicer
{
namespace
{
// 集中维护 9 个 Config 类型的 (tag, TypeInfo*, 短标签, 默认表名) 映射。
// 通过 GetTypeInfo<T>() 拿到头文件中 inline 定义的单例指针。
struct ConfigEntry
{
    PipelineConfigTag tag;
    Utils::TypeInfo* info;
    const char* short_name;
    const char* table_name;
};

const std::array<ConfigEntry, 9>& Entries()
{
    static const std::array<ConfigEntry, 9> entries = {
        ConfigEntry{PipelineConfigTag::Fdm, Utils::GetTypeInfo<HsBaFdmPipelineConfig_t>(), "fdm", "hsba_param_fdm"},
        ConfigEntry{PipelineConfigTag::Sla, Utils::GetTypeInfo<HsBaSlaPipelineConfig_t>(), "sla", "hsba_param_sla"},
        ConfigEntry{PipelineConfigTag::Sls, Utils::GetTypeInfo<HsBaSlsPipelineConfig_t>(), "sls", "hsba_param_sls"},
        ConfigEntry{PipelineConfigTag::Slm, Utils::GetTypeInfo<HsBaSlmPipelineConfig_t>(), "slm", "hsba_param_slm"},
        ConfigEntry{PipelineConfigTag::Lom, Utils::GetTypeInfo<HsBaLomPipelineConfig_t>(), "lom", "hsba_param_lom"},
        ConfigEntry{PipelineConfigTag::Tdp, Utils::GetTypeInfo<HsBaTdpPipelineConfig_t>(), "tdp", "hsba_param_tdp"},
        ConfigEntry{PipelineConfigTag::Waam, Utils::GetTypeInfo<HsBaWaamPipelineConfig_t>(), "waam", "hsba_param_waam"},
        ConfigEntry{PipelineConfigTag::Custom, Utils::GetTypeInfo<HsBaCustomPipelineConfig_t>(), "custom",
                    "hsba_param_custom"},
        ConfigEntry{PipelineConfigTag::FileTransfer, Utils::GetTypeInfo<HsBaFileTransferPipelineConfig_t>(),
                    "filetransfer", "hsba_param_filetransfer"},
    };
    return entries;
}
}  // namespace

void RegisterPipelineConfigTypes()
{
    // 强制触发头文件中每个 GetTypeInfo<T>() 函数内静态的初始化（填充 fields 表）。
    // TypeInfo 特化本体是 inline，跨 TU 唯一；此处仅做一次性求值与哨兵登记。
    static std::once_flag once;
    std::call_once(
        once,
        []
        {
            for (const auto& e : Entries())
            {
                // 触碰 fields 保证已完成填充；写入哨兵方法便于调试期识别。
                (void)e.info->fields.size();
                e.info->methods.emplace("__param_registered",
                                        [](void*, std::span<Utils::AnyObject>) -> Utils::AnyObject { return {}; });
            }
        });
}

bool IsPipelineConfigType(const Utils::TypeInfo* ti) noexcept
{
    if (!ti)
        return false;
    for (const auto& e : Entries())
    {
        if (e.info == ti)
            return true;
    }
    return false;
}

PipelineConfigTag TagOf(const Utils::TypeInfo* ti) noexcept
{
    if (!ti)
        return PipelineConfigTag::Unknown;
    for (const auto& e : Entries())
    {
        if (e.info == ti)
            return e.tag;
    }
    return PipelineConfigTag::Unknown;
}

std::string_view DefaultTableName(PipelineConfigTag tag) noexcept
{
    for (const auto& e : Entries())
    {
        if (e.tag == tag)
            return e.table_name;
    }
    return {};
}

std::string_view DefaultTableName(const Utils::TypeInfo* ti) noexcept
{
    return DefaultTableName(TagOf(ti));
}

PipelineConfigTag TagFromName(std::string_view name) noexcept
{
    for (const auto& e : Entries())
    {
        if (name == e.short_name)
            return e.tag;
    }
    return PipelineConfigTag::Unknown;
}

Utils::TypeInfo* TypeInfoOf(PipelineConfigTag tag) noexcept
{
    for (const auto& e : Entries())
    {
        if (e.tag == tag)
            return e.info;
    }
    return nullptr;
}

}  // namespace HsBa::Slicer
