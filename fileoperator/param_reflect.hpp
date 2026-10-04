/** @file param_reflect.hpp
 * @brief AnyObject reflection registration: fills `TypeInfo::fields` for every PipelineConfig C
 *        struct declared in pipelinetypes/pipeline_types.h so ParamStore can persist them via
 *        `ForeachField`.
 *
 * Field offsets and types must correspond one-to-one; `Field = (child_type_info, byte_offset)`
 * semantics are defined by `AnyObject::ForeachField` in base/any_object.cpp. Every Config struct is
 * a standard-layout aggregate of POD/enum/const char* members, so `offsetof` and
 * `std::declval<S>().F` are both well defined.
 *
 * Every `GetTypeInfo<T>()` specialization here must be inline and included by each translation unit
 * that calls it; otherwise separate TUs create distinct static TypeInfo instances (an inline static
 * splits into multiple copies across SHARED-library boundaries). All downstream users
 * (HsBaSlicerFileOperator / LibHsBaSlicer / DllHsBaSlicer) must therefore include this header.
 */
#pragma once
#ifndef HSBA_SLICER_PARAM_REFLECT_HPP
#define HSBA_SLICER_PARAM_REFLECT_HPP

#include <cstddef>
#include <string_view>
#include <type_traits>
#include <utility>

#include "base/any_object.hpp"
#include "pipelinetypes/pipeline_types.h"

// Internal macro: append one field to the parent struct's TypeInfo fields table. The field type is
// deduced with std::declval so enums/const char*/int/float/double all map to their GetTypeInfo<T>() singleton.
#define HSBA_PARAM_FIELD(Info, S, F)                                                                                   \
    (Info).fields.emplace(                                                                                             \
        #F, std::make_pair(Utils::GetTypeInfo<std::remove_cvref_t<decltype(std::declval<S>().F)>>(), offsetof(S, F)))

// Internal macro: generate an inline GetTypeInfo specialization for a Config struct. Name uses a
// readable short name as the parameter key; callers check membership with IsPipelineConfigType.
#define HSBA_PARAM_DEFINE_CONFIG_TYPEINFO(StructT, CleanName)                                                          \
    template <>                                                                                                        \
    inline Utils::TypeInfo* GetTypeInfo<StructT>()                                                                     \
    {                                                                                                                  \
        static Utils::TypeInfo info;                                                                                   \
        info.Name = CleanName;                                                                                         \
        info.destroy = [](void* p) { delete static_cast<StructT*>(p); };                                               \
        info.copy = [](const void* p) -> void* { return new StructT(*static_cast<const StructT*>(p)); };               \
        info.move = [](void* p) -> void* { return new StructT(std::move(*static_cast<StructT*>(p))); };                \
        info.fields.clear();                                                                                           \
        info.methods.clear();                                                                                          \
        return &info;                                                                                                  \
    }

namespace HsBa::Slicer::Utils
{
// ---------------------------------------------------------------------------
// FDM
// ---------------------------------------------------------------------------
template <>
inline TypeInfo* GetTypeInfo<HsBaFdmPipelineConfig_t>()
{
    static TypeInfo info;
    info.Name = "HsBaFdmPipelineConfig";
    info.destroy = [](void* p) { delete static_cast<HsBaFdmPipelineConfig_t*>(p); };
    info.copy = [](const void* p) -> void*
    { return new HsBaFdmPipelineConfig_t(*static_cast<const HsBaFdmPipelineConfig_t*>(p)); };
    info.move = [](void* p) -> void*
    { return new HsBaFdmPipelineConfig_t(std::move(*static_cast<HsBaFdmPipelineConfig_t*>(p))); };
    info.fields.clear();
    info.methods.clear();
    HSBA_PARAM_FIELD(info, HsBaFdmPipelineConfig_t, model_name);
    HSBA_PARAM_FIELD(info, HsBaFdmPipelineConfig_t, model_path);
    HSBA_PARAM_FIELD(info, HsBaFdmPipelineConfig_t, layer_height);
    HSBA_PARAM_FIELD(info, HsBaFdmPipelineConfig_t, first_layer_height);
    HSBA_PARAM_FIELD(info, HsBaFdmPipelineConfig_t, fill_spacing);
    HSBA_PARAM_FIELD(info, HsBaFdmPipelineConfig_t, fill_mode);
    HSBA_PARAM_FIELD(info, HsBaFdmPipelineConfig_t, fill_angle);
    HSBA_PARAM_FIELD(info, HsBaFdmPipelineConfig_t, wall_count);
    HSBA_PARAM_FIELD(info, HsBaFdmPipelineConfig_t, top_layer_count);
    HSBA_PARAM_FIELD(info, HsBaFdmPipelineConfig_t, bottom_layer_count);
    HSBA_PARAM_FIELD(info, HsBaFdmPipelineConfig_t, infill_density);
    HSBA_PARAM_FIELD(info, HsBaFdmPipelineConfig_t, enable_support);
    HSBA_PARAM_FIELD(info, HsBaFdmPipelineConfig_t, overhang_angle);
    HSBA_PARAM_FIELD(info, HsBaFdmPipelineConfig_t, support_gap);
    HSBA_PARAM_FIELD(info, HsBaFdmPipelineConfig_t, support_diameter);
    HSBA_PARAM_FIELD(info, HsBaFdmPipelineConfig_t, support_density);
    HSBA_PARAM_FIELD(info, HsBaFdmPipelineConfig_t, support_pattern);
    HSBA_PARAM_FIELD(info, HsBaFdmPipelineConfig_t, interface_layers);
    HSBA_PARAM_FIELD(info, HsBaFdmPipelineConfig_t, interface_density);
    HSBA_PARAM_FIELD(info, HsBaFdmPipelineConfig_t, line_width);
    HSBA_PARAM_FIELD(info, HsBaFdmPipelineConfig_t, print_speed);
    HSBA_PARAM_FIELD(info, HsBaFdmPipelineConfig_t, travel_speed);
    HSBA_PARAM_FIELD(info, HsBaFdmPipelineConfig_t, extrusion_multiplier);
    HSBA_PARAM_FIELD(info, HsBaFdmPipelineConfig_t, spiral_mode);
    HSBA_PARAM_FIELD(info, HsBaFdmPipelineConfig_t, gcode_firmware);
    HSBA_PARAM_FIELD(info, HsBaFdmPipelineConfig_t, nozzle_diameter);
    HSBA_PARAM_FIELD(info, HsBaFdmPipelineConfig_t, filament_diameter);
    HSBA_PARAM_FIELD(info, HsBaFdmPipelineConfig_t, nozzle_temp);
    HSBA_PARAM_FIELD(info, HsBaFdmPipelineConfig_t, bed_temp);
    HSBA_PARAM_FIELD(info, HsBaFdmPipelineConfig_t, retract_length);
    HSBA_PARAM_FIELD(info, HsBaFdmPipelineConfig_t, retract_speed);
    HSBA_PARAM_FIELD(info, HsBaFdmPipelineConfig_t, first_layer_speed);
    HSBA_PARAM_FIELD(info, HsBaFdmPipelineConfig_t, support_lua_script);
    HSBA_PARAM_FIELD(info, HsBaFdmPipelineConfig_t, support_lua_func);
    HSBA_PARAM_FIELD(info, HsBaFdmPipelineConfig_t, infill_lua_script);
    HSBA_PARAM_FIELD(info, HsBaFdmPipelineConfig_t, infill_lua_func);
    HSBA_PARAM_FIELD(info, HsBaFdmPipelineConfig_t, output_path);
    return &info;
}

// ---------------------------------------------------------------------------
// SLA
// ---------------------------------------------------------------------------
template <>
inline TypeInfo* GetTypeInfo<HsBaSlaPipelineConfig_t>()
{
    static TypeInfo info;
    info.Name = "HsBaSlaPipelineConfig";
    info.destroy = [](void* p) { delete static_cast<HsBaSlaPipelineConfig_t*>(p); };
    info.copy = [](const void* p) -> void*
    { return new HsBaSlaPipelineConfig_t(*static_cast<const HsBaSlaPipelineConfig_t*>(p)); };
    info.move = [](void* p) -> void*
    { return new HsBaSlaPipelineConfig_t(std::move(*static_cast<HsBaSlaPipelineConfig_t*>(p))); };
    info.fields.clear();
    info.methods.clear();
    HSBA_PARAM_FIELD(info, HsBaSlaPipelineConfig_t, model_name);
    HSBA_PARAM_FIELD(info, HsBaSlaPipelineConfig_t, model_path);
    HSBA_PARAM_FIELD(info, HsBaSlaPipelineConfig_t, layer_height);
    HSBA_PARAM_FIELD(info, HsBaSlaPipelineConfig_t, first_layer_height);
    HSBA_PARAM_FIELD(info, HsBaSlaPipelineConfig_t, bottom_exposure_time);
    HSBA_PARAM_FIELD(info, HsBaSlaPipelineConfig_t, normal_exposure_time);
    HSBA_PARAM_FIELD(info, HsBaSlaPipelineConfig_t, bottom_lift_distance);
    HSBA_PARAM_FIELD(info, HsBaSlaPipelineConfig_t, lift_distance);
    HSBA_PARAM_FIELD(info, HsBaSlaPipelineConfig_t, lift_speed);
    HSBA_PARAM_FIELD(info, HsBaSlaPipelineConfig_t, retract_speed);
    HSBA_PARAM_FIELD(info, HsBaSlaPipelineConfig_t, floor_raft_offset);
    HSBA_PARAM_FIELD(info, HsBaSlaPipelineConfig_t, floor_border_width);
    HSBA_PARAM_FIELD(info, HsBaSlaPipelineConfig_t, floor_fill_spacing);
    HSBA_PARAM_FIELD(info, HsBaSlaPipelineConfig_t, floor_fill_angle);
    HSBA_PARAM_FIELD(info, HsBaSlaPipelineConfig_t, floor_border_count);
    HSBA_PARAM_FIELD(info, HsBaSlaPipelineConfig_t, floor_use_convex_hull);
    HSBA_PARAM_FIELD(info, HsBaSlaPipelineConfig_t, enable_support);
    HSBA_PARAM_FIELD(info, HsBaSlaPipelineConfig_t, overhang_angle);
    HSBA_PARAM_FIELD(info, HsBaSlaPipelineConfig_t, support_gap);
    HSBA_PARAM_FIELD(info, HsBaSlaPipelineConfig_t, support_diameter);
    HSBA_PARAM_FIELD(info, HsBaSlaPipelineConfig_t, support_density);
    HSBA_PARAM_FIELD(info, HsBaSlaPipelineConfig_t, support_pattern);
    HSBA_PARAM_FIELD(info, HsBaSlaPipelineConfig_t, support_lua_script);
    HSBA_PARAM_FIELD(info, HsBaSlaPipelineConfig_t, support_lua_func);
    HSBA_PARAM_FIELD(info, HsBaSlaPipelineConfig_t, floor_lua_script);
    HSBA_PARAM_FIELD(info, HsBaSlaPipelineConfig_t, floor_lua_func);
    HSBA_PARAM_FIELD(info, HsBaSlaPipelineConfig_t, export_lua_script);
    HSBA_PARAM_FIELD(info, HsBaSlaPipelineConfig_t, export_lua_func);
    HSBA_PARAM_FIELD(info, HsBaSlaPipelineConfig_t, output_path);
    HSBA_PARAM_FIELD(info, HsBaSlaPipelineConfig_t, image_type);
    HSBA_PARAM_FIELD(info, HsBaSlaPipelineConfig_t, image_width);
    HSBA_PARAM_FIELD(info, HsBaSlaPipelineConfig_t, image_height);
    return &info;
}

// ---------------------------------------------------------------------------
// SLS
// ---------------------------------------------------------------------------
template <>
inline TypeInfo* GetTypeInfo<HsBaSlsPipelineConfig_t>()
{
    static TypeInfo info;
    info.Name = "HsBaSlsPipelineConfig";
    info.destroy = [](void* p) { delete static_cast<HsBaSlsPipelineConfig_t*>(p); };
    info.copy = [](const void* p) -> void*
    { return new HsBaSlsPipelineConfig_t(*static_cast<const HsBaSlsPipelineConfig_t*>(p)); };
    info.move = [](void* p) -> void*
    { return new HsBaSlsPipelineConfig_t(std::move(*static_cast<HsBaSlsPipelineConfig_t*>(p))); };
    info.fields.clear();
    info.methods.clear();
    HSBA_PARAM_FIELD(info, HsBaSlsPipelineConfig_t, model_name);
    HSBA_PARAM_FIELD(info, HsBaSlsPipelineConfig_t, model_path);
    HSBA_PARAM_FIELD(info, HsBaSlsPipelineConfig_t, layer_height);
    HSBA_PARAM_FIELD(info, HsBaSlsPipelineConfig_t, first_layer_height);
    HSBA_PARAM_FIELD(info, HsBaSlsPipelineConfig_t, laser_power);
    HSBA_PARAM_FIELD(info, HsBaSlsPipelineConfig_t, scan_speed);
    HSBA_PARAM_FIELD(info, HsBaSlsPipelineConfig_t, hatch_spacing);
    HSBA_PARAM_FIELD(info, HsBaSlsPipelineConfig_t, hatch_rotation);
    HSBA_PARAM_FIELD(info, HsBaSlsPipelineConfig_t, bed_temperature);
    HSBA_PARAM_FIELD(info, HsBaSlsPipelineConfig_t, export_lua_script);
    HSBA_PARAM_FIELD(info, HsBaSlsPipelineConfig_t, export_lua_func);
    HSBA_PARAM_FIELD(info, HsBaSlsPipelineConfig_t, output_path);
    return &info;
}

// ---------------------------------------------------------------------------
// SLM
// ---------------------------------------------------------------------------
template <>
inline TypeInfo* GetTypeInfo<HsBaSlmPipelineConfig_t>()
{
    static TypeInfo info;
    info.Name = "HsBaSlmPipelineConfig";
    info.destroy = [](void* p) { delete static_cast<HsBaSlmPipelineConfig_t*>(p); };
    info.copy = [](const void* p) -> void*
    { return new HsBaSlmPipelineConfig_t(*static_cast<const HsBaSlmPipelineConfig_t*>(p)); };
    info.move = [](void* p) -> void*
    { return new HsBaSlmPipelineConfig_t(std::move(*static_cast<HsBaSlmPipelineConfig_t*>(p))); };
    info.fields.clear();
    info.methods.clear();
    HSBA_PARAM_FIELD(info, HsBaSlmPipelineConfig_t, model_name);
    HSBA_PARAM_FIELD(info, HsBaSlmPipelineConfig_t, model_path);
    HSBA_PARAM_FIELD(info, HsBaSlmPipelineConfig_t, layer_height);
    HSBA_PARAM_FIELD(info, HsBaSlmPipelineConfig_t, first_layer_height);
    HSBA_PARAM_FIELD(info, HsBaSlmPipelineConfig_t, laser_power);
    HSBA_PARAM_FIELD(info, HsBaSlmPipelineConfig_t, scan_speed);
    HSBA_PARAM_FIELD(info, HsBaSlmPipelineConfig_t, hatch_spacing);
    HSBA_PARAM_FIELD(info, HsBaSlmPipelineConfig_t, hatch_rotation);
    HSBA_PARAM_FIELD(info, HsBaSlmPipelineConfig_t, bed_temperature);
    HSBA_PARAM_FIELD(info, HsBaSlmPipelineConfig_t, material);
    HSBA_PARAM_FIELD(info, HsBaSlmPipelineConfig_t, light_source);
    HSBA_PARAM_FIELD(info, HsBaSlmPipelineConfig_t, protect_gas);
    HSBA_PARAM_FIELD(info, HsBaSlmPipelineConfig_t, export_lua_script);
    HSBA_PARAM_FIELD(info, HsBaSlmPipelineConfig_t, export_lua_func);
    HSBA_PARAM_FIELD(info, HsBaSlmPipelineConfig_t, output_path);
    return &info;
}

// ---------------------------------------------------------------------------
// LOM
// ---------------------------------------------------------------------------
template <>
inline TypeInfo* GetTypeInfo<HsBaLomPipelineConfig_t>()
{
    static TypeInfo info;
    info.Name = "HsBaLomPipelineConfig";
    info.destroy = [](void* p) { delete static_cast<HsBaLomPipelineConfig_t*>(p); };
    info.copy = [](const void* p) -> void*
    { return new HsBaLomPipelineConfig_t(*static_cast<const HsBaLomPipelineConfig_t*>(p)); };
    info.move = [](void* p) -> void*
    { return new HsBaLomPipelineConfig_t(std::move(*static_cast<HsBaLomPipelineConfig_t*>(p))); };
    info.fields.clear();
    info.methods.clear();
    HSBA_PARAM_FIELD(info, HsBaLomPipelineConfig_t, model_name);
    HSBA_PARAM_FIELD(info, HsBaLomPipelineConfig_t, model_path);
    HSBA_PARAM_FIELD(info, HsBaLomPipelineConfig_t, layer_height);
    HSBA_PARAM_FIELD(info, HsBaLomPipelineConfig_t, first_layer_height);
    HSBA_PARAM_FIELD(info, HsBaLomPipelineConfig_t, cut_speed);
    HSBA_PARAM_FIELD(info, HsBaLomPipelineConfig_t, cut_margin);
    HSBA_PARAM_FIELD(info, HsBaLomPipelineConfig_t, cut_power);
    HSBA_PARAM_FIELD(info, HsBaLomPipelineConfig_t, bond_temperature);
    HSBA_PARAM_FIELD(info, HsBaLomPipelineConfig_t, bond_pressure);
    HSBA_PARAM_FIELD(info, HsBaLomPipelineConfig_t, bond_time);
    HSBA_PARAM_FIELD(info, HsBaLomPipelineConfig_t, seal_contour);
    HSBA_PARAM_FIELD(info, HsBaLomPipelineConfig_t, cut_mode);
    HSBA_PARAM_FIELD(info, HsBaLomPipelineConfig_t, export_lua_script);
    HSBA_PARAM_FIELD(info, HsBaLomPipelineConfig_t, export_lua_func);
    HSBA_PARAM_FIELD(info, HsBaLomPipelineConfig_t, output_path);
    return &info;
}

// ---------------------------------------------------------------------------
// TDP (3DP)
// ---------------------------------------------------------------------------
template <>
inline TypeInfo* GetTypeInfo<HsBaTdpPipelineConfig_t>()
{
    static TypeInfo info;
    info.Name = "HsBaTdpPipelineConfig";
    info.destroy = [](void* p) { delete static_cast<HsBaTdpPipelineConfig_t*>(p); };
    info.copy = [](const void* p) -> void*
    { return new HsBaTdpPipelineConfig_t(*static_cast<const HsBaTdpPipelineConfig_t*>(p)); };
    info.move = [](void* p) -> void*
    { return new HsBaTdpPipelineConfig_t(std::move(*static_cast<HsBaTdpPipelineConfig_t*>(p))); };
    info.fields.clear();
    info.methods.clear();
    HSBA_PARAM_FIELD(info, HsBaTdpPipelineConfig_t, model_name);
    HSBA_PARAM_FIELD(info, HsBaTdpPipelineConfig_t, model_path);
    HSBA_PARAM_FIELD(info, HsBaTdpPipelineConfig_t, layer_height);
    HSBA_PARAM_FIELD(info, HsBaTdpPipelineConfig_t, first_layer_height);
    HSBA_PARAM_FIELD(info, HsBaTdpPipelineConfig_t, head_count);
    HSBA_PARAM_FIELD(info, HsBaTdpPipelineConfig_t, drop_spacing);
    HSBA_PARAM_FIELD(info, HsBaTdpPipelineConfig_t, binder_saturation);
    HSBA_PARAM_FIELD(info, HsBaTdpPipelineConfig_t, ink_curing_time);
    HSBA_PARAM_FIELD(info, HsBaTdpPipelineConfig_t, bed_temperature);
    HSBA_PARAM_FIELD(info, HsBaTdpPipelineConfig_t, binder_mode);
    HSBA_PARAM_FIELD(info, HsBaTdpPipelineConfig_t, export_lua_script);
    HSBA_PARAM_FIELD(info, HsBaTdpPipelineConfig_t, export_lua_func);
    HSBA_PARAM_FIELD(info, HsBaTdpPipelineConfig_t, output_path);
    HSBA_PARAM_FIELD(info, HsBaTdpPipelineConfig_t, spiral_mode);
    return &info;
}

// ---------------------------------------------------------------------------
// WAAM
// ---------------------------------------------------------------------------
template <>
inline TypeInfo* GetTypeInfo<HsBaWaamPipelineConfig_t>()
{
    static TypeInfo info;
    info.Name = "HsBaWaamPipelineConfig";
    info.destroy = [](void* p) { delete static_cast<HsBaWaamPipelineConfig_t*>(p); };
    info.copy = [](const void* p) -> void*
    { return new HsBaWaamPipelineConfig_t(*static_cast<const HsBaWaamPipelineConfig_t*>(p)); };
    info.move = [](void* p) -> void*
    { return new HsBaWaamPipelineConfig_t(std::move(*static_cast<HsBaWaamPipelineConfig_t*>(p))); };
    info.fields.clear();
    info.methods.clear();
    HSBA_PARAM_FIELD(info, HsBaWaamPipelineConfig_t, model_name);
    HSBA_PARAM_FIELD(info, HsBaWaamPipelineConfig_t, model_path);
    HSBA_PARAM_FIELD(info, HsBaWaamPipelineConfig_t, layer_height);
    HSBA_PARAM_FIELD(info, HsBaWaamPipelineConfig_t, first_layer_height);
    HSBA_PARAM_FIELD(info, HsBaWaamPipelineConfig_t, bead_width);
    HSBA_PARAM_FIELD(info, HsBaWaamPipelineConfig_t, travel_speed);
    HSBA_PARAM_FIELD(info, HsBaWaamPipelineConfig_t, wire_feed_speed);
    HSBA_PARAM_FIELD(info, HsBaWaamPipelineConfig_t, arc_current);
    HSBA_PARAM_FIELD(info, HsBaWaamPipelineConfig_t, arc_voltage);
    HSBA_PARAM_FIELD(info, HsBaWaamPipelineConfig_t, gas_flow_rate);
    HSBA_PARAM_FIELD(info, HsBaWaamPipelineConfig_t, material);
    HSBA_PARAM_FIELD(info, HsBaWaamPipelineConfig_t, welding_process);
    HSBA_PARAM_FIELD(info, HsBaWaamPipelineConfig_t, protection);
    HSBA_PARAM_FIELD(info, HsBaWaamPipelineConfig_t, protect_gas);
    HSBA_PARAM_FIELD(info, HsBaWaamPipelineConfig_t, interpass_temperature);
    HSBA_PARAM_FIELD(info, HsBaWaamPipelineConfig_t, robot_type);
    HSBA_PARAM_FIELD(info, HsBaWaamPipelineConfig_t, path_lua_script);
    HSBA_PARAM_FIELD(info, HsBaWaamPipelineConfig_t, path_lua_func);
    HSBA_PARAM_FIELD(info, HsBaWaamPipelineConfig_t, output_path);
    HSBA_PARAM_FIELD(info, HsBaWaamPipelineConfig_t, spiral_mode);
    return &info;
}

// ---------------------------------------------------------------------------
// Custom Lua pipeline (all string fields)
// ---------------------------------------------------------------------------
template <>
inline TypeInfo* GetTypeInfo<HsBaCustomPipelineConfig_t>()
{
    static TypeInfo info;
    info.Name = "HsBaCustomPipelineConfig";
    info.destroy = [](void* p) { delete static_cast<HsBaCustomPipelineConfig_t*>(p); };
    info.copy = [](const void* p) -> void*
    { return new HsBaCustomPipelineConfig_t(*static_cast<const HsBaCustomPipelineConfig_t*>(p)); };
    info.move = [](void* p) -> void*
    { return new HsBaCustomPipelineConfig_t(std::move(*static_cast<HsBaCustomPipelineConfig_t*>(p))); };
    info.fields.clear();
    info.methods.clear();
    HSBA_PARAM_FIELD(info, HsBaCustomPipelineConfig_t, pipeline_lua_script);
    HSBA_PARAM_FIELD(info, HsBaCustomPipelineConfig_t, pipeline_lua_source);
    HSBA_PARAM_FIELD(info, HsBaCustomPipelineConfig_t, entry_func);
    HSBA_PARAM_FIELD(info, HsBaCustomPipelineConfig_t, config_json);
    HSBA_PARAM_FIELD(info, HsBaCustomPipelineConfig_t, model_name);
    HSBA_PARAM_FIELD(info, HsBaCustomPipelineConfig_t, model_path);
    HSBA_PARAM_FIELD(info, HsBaCustomPipelineConfig_t, output_path);
    return &info;
}

// ---------------------------------------------------------------------------
// File transfer
// Note: HsBaFileTransferPipelineConfig_t::file_paths is a const char** pointer array that cannot map
// to a single column, so it is not registered as a reflected field; ParamStore saves it specially as
// a JSON TEXT column.
// ---------------------------------------------------------------------------
template <>
inline TypeInfo* GetTypeInfo<HsBaFileTransferPipelineConfig_t>()
{
    static TypeInfo info;
    info.Name = "HsBaFileTransferPipelineConfig";
    info.destroy = [](void* p) { delete static_cast<HsBaFileTransferPipelineConfig_t*>(p); };
    info.copy = [](const void* p) -> void*
    { return new HsBaFileTransferPipelineConfig_t(*static_cast<const HsBaFileTransferPipelineConfig_t*>(p)); };
    info.move = [](void* p) -> void*
    { return new HsBaFileTransferPipelineConfig_t(std::move(*static_cast<HsBaFileTransferPipelineConfig_t*>(p))); };
    info.fields.clear();
    info.methods.clear();
    HSBA_PARAM_FIELD(info, HsBaFileTransferPipelineConfig_t, host);
    HSBA_PARAM_FIELD(info, HsBaFileTransferPipelineConfig_t, port);
    HSBA_PARAM_FIELD(info, HsBaFileTransferPipelineConfig_t, pool_size);
    HSBA_PARAM_FIELD(info, HsBaFileTransferPipelineConfig_t, file_count);
    return &info;
}

}  // namespace HsBa::Slicer::Utils

#undef HSBA_PARAM_DEFINE_CONFIG_TYPEINFO
#undef HSBA_PARAM_FIELD

namespace HsBa::Slicer
{
/** @brief Type tags for the 9 Config structs; used by the Lua layer and ParamStore to pick a target table. */
enum class PipelineConfigTag : int
{
    Unknown = 0,
    Fdm,
    Sla,
    Sls,
    Slm,
    Lom,
    Tdp,
    Waam,
    Custom,
    FileTransfer,
};

/** @brief Force-instantiate the TypeInfo static object of every PipelineConfig type so its fields table is populated.
 *  Thread-safe: the underlying GetTypeInfo<T>() uses a function-local static, so repeated calls have no side effects. */
void RegisterPipelineConfigTypes();

/** @brief Whether the given TypeInfo* belongs to one of the 9 PipelineConfig structs. */
bool IsPipelineConfigType(const Utils::TypeInfo* ti) noexcept;

/** @brief Return the PipelineConfigTag for a TypeInfo*; Unknown when unregistered. */
PipelineConfigTag TagOf(const Utils::TypeInfo* ti) noexcept;

/** @brief Return the default table name for a tag (of the form hsba_param_fdm). */
std::string_view DefaultTableName(PipelineConfigTag tag) noexcept;

/** @brief Look up the default table name from a TypeInfo*; empty when unregistered. */
std::string_view DefaultTableName(const Utils::TypeInfo* ti) noexcept;

/** @brief Parse a Lua-supplied short label ("fdm"/"sla"/...) into a PipelineConfigTag; Unknown when unknown. */
PipelineConfigTag TagFromName(std::string_view name) noexcept;

/** @brief Return the TypeInfo* for a tag; nullptr for Unknown. */
Utils::TypeInfo* TypeInfoOf(PipelineConfigTag tag) noexcept;
}  // namespace HsBa::Slicer

#endif  // HSBA_SLICER_PARAM_REFLECT_HPP
