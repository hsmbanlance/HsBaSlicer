/** @file param_reflect.hpp
 * @brief AnyObject 反射注册：为 pipelinetypes/pipeline_types.h 中所有 PipelineConfig
 *        C 结构体填充 `TypeInfo::fields`，供 ParamStore 用 `ForeachField` 遍历落库。
 *
 * 每个 C 结构体的字段偏移与字段类型必须一一对应，`Field = (child_type_info, byte_offset)`
 * 语义参见 base/any_object.cpp 的 `AnyObject::ForeachField`。所有 Config 结构体是标准布局
 * POD/enum/const char* 的集合，`offsetof` 与 `std::declval<S>().F` 均良定义。
 *
 * 本头文件的所有 `GetTypeInfo<T>()` 特化必须是 inline，且必须与调用它的每个翻译单元
 * 一起被 include，否则不同 TU 会各自创建 static TypeInfo 实例（memory 提醒：MockRegistry
 * 类头文件内联静态在跨 SHARED 库边界会分裂为多份，TypeInfo 同理）。因此所有下游使用者
 * （HsBaSlicerFileOperator / LibHsBaSlicer / DllHsBaSlicer）都必须显式 include 本头。
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

// 内部宏：向父结构体 TypeInfo 的 fields 表追加一个字段。字段类型由 std::declval 推导，
// 保证枚举/const char*/int/float/double 都能自动映射到对应的 GetTypeInfo<T>() 单例。
#define HSBA_PARAM_FIELD(Info, S, F)                                                                                   \
    (Info).fields.emplace(                                                                                             \
        #F, std::make_pair(Utils::GetTypeInfo<std::remove_cvref_t<decltype(std::declval<S>().F)>>(), offsetof(S, F)))

// 内部宏：为某个 Config 结构体生成 inline GetTypeInfo 特化。Name 使用可读的短名，作为
// 参数键；调用方通过 IsPipelineConfigType 判定。
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
// 注意：HsBaFileTransferPipelineConfig_t::file_paths 是 const char** 指针数组，
// 无法映射为单列。此处不注册为 reflection 字段，由 ParamStore 特判以 JSON TEXT 列保存。
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
/** @brief 9 个 Config 结构体的类型标签，Lua 侧与 ParamStore 用于选择目标表。 */
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

/** @brief 强制实例化所有 PipelineConfig 类型的 TypeInfo 静态对象，保证 fields 表已填充。
 *  线程安全：底层 GetTypeInfo<T>() 使用函数内静态，重复调用无副作用。 */
void RegisterPipelineConfigTypes();

/** @brief 判断给定的 TypeInfo* 是否属于 9 个 PipelineConfig 结构体之一。 */
bool IsPipelineConfigType(const Utils::TypeInfo* ti) noexcept;

/** @brief 返回 TypeInfo* 对应的 PipelineConfigTag；未注册则返回 Unknown。 */
PipelineConfigTag TagOf(const Utils::TypeInfo* ti) noexcept;

/** @brief 返回 tag 对应的默认表名（形如 hsba_param_fdm）。 */
std::string_view DefaultTableName(PipelineConfigTag tag) noexcept;

/** @brief 由 TypeInfo* 反查默认表名；未注册返回空。 */
std::string_view DefaultTableName(const Utils::TypeInfo* ti) noexcept;

/** @brief 由 Lua 传入的短标签（"fdm"/"sla"/...）解析为 PipelineConfigTag；未知返回 Unknown。 */
PipelineConfigTag TagFromName(std::string_view name) noexcept;

/** @brief 返回 tag 对应的 TypeInfo*；Unknown 返回 nullptr。 */
Utils::TypeInfo* TypeInfoOf(PipelineConfigTag tag) noexcept;
}  // namespace HsBa::Slicer

#endif  // HSBA_SLICER_PARAM_REFLECT_HPP
