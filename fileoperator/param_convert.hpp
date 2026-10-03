/** @file param_convert.hpp
 * @brief 类型收敛桥：在 AnyObject 反射暴露的 C 字段类型（float / int / enum / const char*）
 *        与 ISQLAdapter 的 `Utils::Visit` 白名单（nullptr_t / int64_t / double / string /
 *        vector<unsigned char>）之间做双向转换。
 *
 * 上游收敛、不改 sql_adapter.cpp 的策略参见计划文档；本文件的 FieldToAny / AnyToField 是
 * ParamStore Save / Load 流水线的第 3、4 阶段所依赖的唯一桥接口。
 */
#pragma once
#ifndef HSBA_SLICER_PARAM_CONVERT_HPP
#define HSBA_SLICER_PARAM_CONVERT_HPP

#include <any>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/any_object.hpp"

namespace HsBa::Slicer
{
/** @brief 反向落地时保存 malloc 复制字符串的持有者，生命周期由调用方保证。 */
using StringArena = std::vector<std::unique_ptr<char[]>>;

/**
 * @brief 读取一个反射字段的原始值并收敛为 ISQLAdapter 白名单类型。
 * @param field_name 字段名，仅用于异常信息。
 * @param field_ti   字段的 TypeInfo*（AnyObject::ForeachField 输出的子对象类型）。
 * @param ptr        字段内存起始地址（子对象 data）。
 * @return 收敛后的 std::any：float/double→double，int/int64/enum→int64_t，
 *         const char*→ string 或 nullptr，std::string→ string。
 * @throws SQL::SQLAdapterInvalidArgumentError 当字段类型不受支持时。
 */
std::any FieldToAny(std::string_view field_name, Utils::TypeInfo* field_ti, const void* ptr);

/**
 * @brief 将 Select 返回的白名单值反向写入反射字段。
 * @param value    列值（double / int64_t / std::string / nullptr / 空 any）。
 * @param field_ti 字段的 TypeInfo*。
 * @param ptr      字段内存起始地址。
 * @param arena    const char* 字段的字符串存储持有者，需比目标结构体活得更久。
 * @throws SQL::SQLAdapterInvalidArgumentError 当类型不匹配或值类型不受支持时。
 */
void AnyToField(const std::any& value, Utils::TypeInfo* field_ti, void* ptr, StringArena& arena);

/** @brief 判断给定的字段 TypeInfo* 是否为受支持的枚举类型（映射到 int64_t）。 */
bool IsEnumFieldType(Utils::TypeInfo* field_ti) noexcept;

/**
 * @brief 遍历 Config 结构体的 const char* 字段，将非空值复制为 std::malloc 持有的堆副本
 *        并回写指针，使其可被 std::free 逐个释放（用于跨 C ABI 交付读取结果）。
 *
 * ParamStore::Load 使用 StringArena（new[]）填充；本函数在其之后调用，把字段指针替换为
 * malloc 副本，arena 析构只释放自身缓冲区，二者互不冲突（避免 new[]/free 混用 UB）。
 * @param cfgTi Config 结构体的 TypeInfo*（须来自 GetTypeInfo<PipelineConfig*>）。
 * @param cfg   Config 结构体内存起始地址。
 */
void ConfigStringsToOwning(Utils::TypeInfo* cfgTi, void* cfg);

/**
 * @brief 遍历 Config 结构体的 const char* 字段，对非空值 std::free 并置 NULL。
 *        仅用于释放由 ConfigStringsToOwning 生成的 malloc 字符串。
 */
void FreeConfigStrings(Utils::TypeInfo* cfgTi, void* cfg);
}  // namespace HsBa::Slicer

#endif  // HSBA_SLICER_PARAM_CONVERT_HPP
