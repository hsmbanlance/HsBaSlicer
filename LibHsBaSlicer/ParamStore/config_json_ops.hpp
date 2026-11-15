/** @file config_json_ops.hpp
 * @brief Lib-layer reflection-driven JSON codec for the flat C-ABI PipelineConfig structs.
 *
 * The field-name/type/default metadata that the REST server exposes through
 * restapi/config_bridge is produced here in the shared core, so the Dll C ABI
 * (config_json_pipeline) and in-process GUI consumers can obtain config JSON
 * without linking the REST server. The codec walks the same param_reflect
 * TypeInfo::fields table and converges each field through param_convert
 * (FieldToAny / AnyToField) exactly as config_bridge does, but JSON-only.
 *
 * Layering: the Dll layer calls only these free functions and never references
 * fileoperator directly.
 */
#pragma once
#ifndef HSBA_SLICER_LIB_CONFIG_JSON_OPS_HPP
#define HSBA_SLICER_LIB_CONFIG_JSON_OPS_HPP

#include <string>
#include <string_view>

#include "../export.h"
#include "param_store_ops.hpp" // ParamPipelineKind

namespace HsBa::Slicer
{
/**
 * @brief Serialize a config struct's reflected fields to pretty JSON.
 * @param kind The pipeline kind that @p cfg points to.
 * @param cfg  Read-only pointer to the corresponding C config struct.
 * @return Pretty-printed JSON of the reflected fields; an empty string for an
 *         unregistered kind or a null @p cfg.
 */
HSBA_SLICER_LIB_API std::string ConfigToJson(ParamPipelineKind kind, const void* cfg);

/**
 * @brief Serialize a kind's default (factory-initialized) config struct to pretty JSON.
 *
 * The defaults come from the same HsBa*ConfigDefault() factories used by the pipeline entry
 * points, so the field values match a freshly created config (const char* fields are null).
 * @param kind The pipeline kind.
 * @return Pretty-printed default JSON; an empty string for an unregistered kind.
 */
HSBA_SLICER_LIB_API std::string DefaultConfigJson(ParamPipelineKind kind);

/**
 * @brief Land a JSON object into an existing config struct.
 *
 * @p cfg must be a default-initialized struct (all const char* fields NULL, as
 * HsBa*ConfigDefault() produces). Only fields present in the JSON are overwritten;
 * absent fields keep their base value. On success every non-null const char* field
 * is std::malloc-owned and must be released via FreeLoadedConfigStrings before the
 * struct is discarded, mirroring the HsBaLoadPipelineParams ownership contract.
 * @param kind The pipeline kind that @p cfg points to.
 * @param json JSON object text.
 * @param cfg  Writable pointer to the config struct.
 * @return true on success; false for an unregistered kind, null pointers or malformed JSON.
 */
HSBA_SLICER_LIB_API bool ConfigFromJson(ParamPipelineKind kind, std::string_view json, void* cfg);

} // namespace HsBa::Slicer

#endif // HSBA_SLICER_LIB_CONFIG_JSON_OPS_HPP
