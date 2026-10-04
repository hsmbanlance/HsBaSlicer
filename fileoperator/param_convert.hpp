/** @file param_convert.hpp
 * @brief Type-convergence bridge between the C field types exposed by AnyObject reflection
 *        (float / int / enum / const char*) and the ISQLAdapter visit whitelist
 *        (nullptr_t / int64_t / double / string / vector<unsigned char>).
 *
 * FieldToAny and AnyToField are the sole bridge interfaces relied upon by stages 3 and 4 of the
 * ParamStore Save / Load pipeline.
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
/** @brief Owner of the new[]-copied strings produced during reverse landing; lifetime is caller-managed. */
using StringArena = std::vector<std::unique_ptr<char[]>>;

/**
 * @brief Read a raw reflected field value and converge it into an ISQLAdapter whitelist type.
 * @param field_name Field name, used only for diagnostic messages.
 * @param field_ti   TypeInfo* of the field (the sub-object type reported by AnyObject::ForeachField).
 * @param ptr        Start address of the field memory (sub-object data).
 * @return Converged std::any: float/double -> double, int/int64/enum -> int64_t,
 *         const char* -> string or nullptr, std::string -> string.
 * @throws SQL::SQLAdapterInvalidArgumentError when the field type is unsupported.
 */
std::any FieldToAny(std::string_view field_name, Utils::TypeInfo* field_ti, const void* ptr);

/**
 * @brief Write a whitelist value returned by Select back into a reflected field.
 * @param value    Column value (double / int64_t / std::string / nullptr / empty any).
 * @param field_ti TypeInfo* of the field.
 * @param ptr      Start address of the field memory.
 * @param arena    Backing storage for const char* fields; must outlive the target struct.
 * @throws SQL::SQLAdapterInvalidArgumentError when the value type mismatches or is unsupported.
 */
void AnyToField(const std::any& value, Utils::TypeInfo* field_ti, void* ptr, StringArena& arena);

/** @brief Whether the given field TypeInfo* is a supported enum type (mapped to int64_t). */
bool IsEnumFieldType(Utils::TypeInfo* field_ti) noexcept;

/**
 * @brief Walk the const char* fields of a Config struct, replacing non-null values with std::malloc
 *        heap copies so each can later be released by std::free (for cross-C-ABI result delivery).
 *
 * ParamStore::Load fills fields from a StringArena (new[]); this runs afterwards and swaps the
 * pointers to malloc copies, so arena destruction frees only its own buffers and the two never
 * conflict (avoiding new[]/free-mixing UB).
 * @param cfgTi TypeInfo* of the Config struct (must come from GetTypeInfo<PipelineConfig*>).
 * @param cfg   Start address of the Config struct memory.
 */
void ConfigStringsToOwning(Utils::TypeInfo* cfgTi, void* cfg);

/**
 * @brief Walk the const char* fields of a Config struct, std::free each non-null value and set it to NULL.
 *        Intended only for releasing strings produced by ConfigStringsToOwning.
 */
void FreeConfigStrings(Utils::TypeInfo* cfgTi, void* cfg);
}  // namespace HsBa::Slicer

#endif  // HSBA_SLICER_PARAM_CONVERT_HPP
