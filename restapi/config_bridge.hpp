/** @file config_bridge.hpp
 * @brief Reflection-driven JSON/XML bridge for the flat C-ABI pipeline config structs.
 * @author HsBa
 *
 * The pipeline config structs (HsBa*PipelineConfig_t) are flat C aggregates whose
 * fields are float/double/int/enum/const char*. They cannot be handed to
 * struct_json/struct_xml directly (the raw const char* members are unsupported),
 * so this bridge walks the field table registered in fileoperator/param_reflect.hpp
 * via Utils::AnyObject, converges every field through param_convert's
 * FieldToAny/AnyToField, and serializes the resulting uniform field view with the
 * project's Utils serializers (no hand-assembled JSON/XML).
 */
#pragma once
#ifndef HSBA_SLICER_RESTAPI_CONFIG_BRIDGE_HPP
#define HSBA_SLICER_RESTAPI_CONFIG_BRIDGE_HPP

#include <string>

#include "base/any_object.hpp"
#include "fileoperator/param_convert.hpp"

#include "content_negotiate.hpp"

namespace HsBa::Slicer::RestApi
{
/**
 * @brief Serialize a config struct to JSON or XML text.
 * @param cfgTi Reflective TypeInfo* of the config struct (GetTypeInfo<HsBa*PipelineConfig_t>()).
 * @param cfg   Pointer to the config struct memory.
 * @param fmt   Target wire format.
 * @return Pretty-printed JSON or XML text of the reflected fields.
 * @throws HsBa::Slicer::Error on an unsupported field type.
 */
std::string SerializeConfig(Utils::TypeInfo* cfgTi, const void* cfg, DataFormat fmt);

/**
 * @brief Deserialize JSON or XML text into a config struct.
 *
 * @p cfg must already hold a valid base (e.g. from HsBaCreateDefault*Config());
 * only fields present in the text are overwritten, others keep their base value.
 * const char* fields are filled with buffers owned by @p arena, which MUST
 * outlive every use of @p cfg (in particular the pipeline run call).
 *
 * @param cfgTi Reflective TypeInfo* of the config struct.
 * @param body  Request body text.
 * @param fmt   Source wire format.
 * @param cfg   Output config struct memory.
 * @param arena Backing storage for string fields.
 * @throws HsBa::Slicer::Error on parse/type errors (map to HTTP 400).
 */
void DeserializeConfig(Utils::TypeInfo* cfgTi, const std::string& body, DataFormat fmt, void* cfg, StringArena& arena);

}  // namespace HsBa::Slicer::RestApi

#endif  // HSBA_SLICER_RESTAPI_CONFIG_BRIDGE_HPP
