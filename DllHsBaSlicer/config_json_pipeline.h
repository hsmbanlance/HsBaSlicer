/** @file config_json_pipeline.h
 * @brief C ABI for the reflection-driven PipelineConfig <-> JSON codec (default JSON, encode, decode).
 *
 * The field-name/type/default metadata that only the REST server used to expose (via
 * restapi/config_bridge) is now reachable in-process through these exports, so a GUI that
 * direct-links the core can build config editors without an HTTP round-trip.
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_CONFIG_JSON_PIPELINE_H
#define HSBA_SLICER_CONFIG_JSON_PIPELINE_H

#include "dllexport.h"
#include "pipelinetypes/pipeline_types.h"

#ifdef __cplusplus
extern "C"
{
#endif  // __cplusplus

    /**
     * @brief Get the default configuration JSON of a pipeline kind (param_reflect driven).
     *
     * The result mirrors a freshly created config struct: registered fields only, const char*
     * fields serialized as JSON null, numbers at their factory defaults.
     *
     * The returned pointer is library-owned: it stays valid only until the next call of
     * HsBaGetDefaultConfigJson on the same thread and MUST NOT be freed by the caller.
     *
     * @param kind Which PipelineConfig type to describe.
     * @return NUL-terminated pretty JSON, or an empty string for an invalid kind.
     */
    HSBA_SLICER_API const char* HsBaGetDefaultConfigJson(HsBaPipelineKind kind);

    /**
     * @brief Serialize a config struct to pretty JSON.
     *
     * The returned pointer follows the same library-owned rule as HsBaGetDefaultConfigJson
     * (separate buffer; valid until the next HsBaConfigToJson call on the same thread; do not free).
     *
     * @param kind The config type that @p cfg points to.
     * @param cfg  Pointer to the config struct (e.g. HsBaFdmPipelineConfig_t). Must not be NULL.
     * @return NUL-terminated pretty JSON, or an empty string for an invalid kind or NULL @p cfg.
     */
    HSBA_SLICER_API const char* HsBaConfigToJson(HsBaPipelineKind kind, const void* cfg);

    /**
     * @brief Decode a JSON object into a config struct.
     *
     * @p cfg must be pre-initialized via the matching *ConfigDefault() so every const char* field
     * starts NULL. Fields present in the JSON are overwritten; absent fields keep their base value.
     * On success the string fields written from JSON are std::malloc-owned and MUST be released with
     * HsBaFreeLoadedPipelineConfig before discarding @p cfg (same contract as HsBaLoadPipelineParams).
     *
     * @param kind The config type that @p cfg points to.
     * @param json JSON object text. Must not be NULL.
     * @param cfg  Writable pointer to the config struct. Must not be NULL.
     * @return 1 on success, 0 for an invalid kind, NULL pointers or malformed JSON.
     */
    HSBA_SLICER_API int HsBaConfigFromJson(HsBaPipelineKind kind, const char* json, void* cfg);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // !HSBA_SLICER_CONFIG_JSON_PIPELINE_H
