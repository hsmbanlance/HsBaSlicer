/** @file param_reflect.cpp
 * @brief Implements the helper functions declared in param_reflect.hpp.
 *
 * The TypeInfo specializations themselves live in the header (they must be inline for ODR); this
 * translation unit only provides the cross-TU-unique registration/lookup logic. All mappings are
 * maintained centrally in function-local static arrays to avoid scattered if/else chains.
 */
#include "param_reflect.hpp"

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
