/** @file param_reflect.cpp
 * @brief Implements the helper functions declared in param_reflect.hpp.
 *
 * The TypeInfo specializations themselves live in the header (they must be inline for ODR); this
 * translation unit only provides the cross-TU-unique registration/lookup logic. All mappings are
 * maintained centrally in function-local static arrays to avoid scattered if/else chains.
 */
#include "param_reflect.hpp"

#include <algorithm>
#include <array>
#include <mutex>

namespace HsBa::Slicer
{
namespace
{
// Centrally maintain the (tag, TypeInfo*, short label, default table) mapping for the 9 Config types.
// The singletons are obtained through GetTypeInfo<T>() defined inline in the header.
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
        ConfigEntry{.tag = PipelineConfigTag::Fdm,
                    .info = Utils::GetTypeInfo<HsBaFdmPipelineConfig_t>(),
                    .short_name = "fdm",
                    .table_name = "hsba_param_fdm"},
        ConfigEntry{.tag = PipelineConfigTag::Sla,
                    .info = Utils::GetTypeInfo<HsBaSlaPipelineConfig_t>(),
                    .short_name = "sla",
                    .table_name = "hsba_param_sla"},
        ConfigEntry{.tag = PipelineConfigTag::Sls,
                    .info = Utils::GetTypeInfo<HsBaSlsPipelineConfig_t>(),
                    .short_name = "sls",
                    .table_name = "hsba_param_sls"},
        ConfigEntry{.tag = PipelineConfigTag::Slm,
                    .info = Utils::GetTypeInfo<HsBaSlmPipelineConfig_t>(),
                    .short_name = "slm",
                    .table_name = "hsba_param_slm"},
        ConfigEntry{.tag = PipelineConfigTag::Lom,
                    .info = Utils::GetTypeInfo<HsBaLomPipelineConfig_t>(),
                    .short_name = "lom",
                    .table_name = "hsba_param_lom"},
        ConfigEntry{.tag = PipelineConfigTag::Tdp,
                    .info = Utils::GetTypeInfo<HsBaTdpPipelineConfig_t>(),
                    .short_name = "tdp",
                    .table_name = "hsba_param_tdp"},
        ConfigEntry{.tag = PipelineConfigTag::Waam,
                    .info = Utils::GetTypeInfo<HsBaWaamPipelineConfig_t>(),
                    .short_name = "waam",
                    .table_name = "hsba_param_waam"},
        ConfigEntry{.tag = PipelineConfigTag::Custom,
                    .info = Utils::GetTypeInfo<HsBaCustomPipelineConfig_t>(),
                    .short_name = "custom",
                    .table_name = "hsba_param_custom"},
        ConfigEntry{.tag = PipelineConfigTag::FileTransfer,
                    .info = Utils::GetTypeInfo<HsBaFileTransferPipelineConfig_t>(),
                    .short_name = "filetransfer",
                    .table_name = "hsba_param_filetransfer"},
    };
    return entries;
}
}  // namespace

void RegisterPipelineConfigTypes()
{
    // Force the function-local static initialization of every GetTypeInfo<T>() in the header (which
    // populates the fields table). The TypeInfo specializations are inline and cross-TU unique; this
    // only performs one-time evaluation and sentinel registration.
    static std::once_flag once;
    std::call_once(once,
                   []
                   {
                       for (const auto& e : Entries())
                       {
                           // Touch fields to guarantee it is populated; add a sentinel method for debug identification.
                           (void)e.info->fields.size();
                           e.info->methods.emplace("__param_registered",
                                                   [](void*, std::span<Utils::AnyObject>) -> Utils::AnyObject
                                                   { return {}; });
                       }
                   });
}

bool IsPipelineConfigType(const Utils::TypeInfo* ti) noexcept  // NOLINT(bugprone-exception-escape) first-call static
                                                               // init of Entries() is the only throw path
{
    if (!ti)
    {
        return false;
    }
    return std::ranges::any_of(Entries(), [ti](const auto& e) { return e.info == ti; });
}

PipelineConfigTag TagOf(const Utils::TypeInfo* ti) noexcept  // NOLINT(bugprone-exception-escape) Entries() first-call
                                                             // static init is the only throw path
{
    if (!ti)
    {
        return PipelineConfigTag::Unknown;
    }
    for (const auto& e : Entries())
    {
        if (e.info == ti)
        {
            return e.tag;
        }
    }
    return PipelineConfigTag::Unknown;
}

std::string_view DefaultTableName(PipelineConfigTag tag) noexcept  // NOLINT(bugprone-exception-escape) Entries()
                                                                   // first-call static init is the only throw path
{
    for (const auto& e : Entries())
    {
        if (e.tag == tag)
        {
            return e.table_name;
        }
    }
    return {};
}

std::string_view DefaultTableName(const Utils::TypeInfo* ti) noexcept
{
    return DefaultTableName(TagOf(ti));
}

PipelineConfigTag TagFromName(std::string_view name) noexcept  // NOLINT(bugprone-exception-escape) Entries() first-call
                                                               // static init is the only throw path
{
    for (const auto& e : Entries())
    {
        if (name == e.short_name)
        {
            return e.tag;
        }
    }
    return PipelineConfigTag::Unknown;
}

Utils::TypeInfo* TypeInfoOf(PipelineConfigTag tag) noexcept  // NOLINT(bugprone-exception-escape) Entries() first-call
                                                             // static init is the only throw path
{
    for (const auto& e : Entries())
    {
        if (e.tag == tag)
        {
            return e.info;
        }
    }
    return nullptr;
}

}  // namespace HsBa::Slicer
