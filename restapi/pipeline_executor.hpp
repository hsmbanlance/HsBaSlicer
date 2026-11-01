/** @file pipeline_executor.hpp
 * @brief Kind-dispatched wrapper over the DllHsBaSlicer C ABI (config alloc + sync run + free).
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_RESTAPI_PIPELINE_EXECUTOR_HPP
#define HSBA_SLICER_RESTAPI_PIPELINE_EXECUTOR_HPP

#include <string>
#include <string_view>
#include <vector>

#include "base/any_object.hpp"
#include "pipelinetypes/pipeline_types.h"

#include "content_negotiate.hpp"

namespace HsBa::Slicer::RestApi
{
/**
 * @brief Normalized view of any pipeline result.
 *
 * The per-kind C result structs differ (gcode_content vs export_path vs
 * output_path vs file-transfer counters); this flattens them into fields the
 * REST layer can serialize uniformly. All buffers are copied out before the
 * underlying C result is released with HsBaFree*PipelineResult.
 */
struct SliceOutcome
{
    int success = 0;               ///< 1 when the pipeline reported success.
    int total_layers = 0;          ///< Layer count (0 for file-transfer).
    std::string output_path;       ///< export_path / output_path (empty when none).
    std::string inline_content;    ///< gcode_content / custom result_string (empty when none).
    std::string error_message;     ///< Error text when success == 0.
    double elapsed_seconds = 0.0;  ///< Wall-clock elapsed time.
    int files_transferred = 0;     ///< File-transfer: succeeded count.
    int total_files = 0;           ///< File-transfer: requested count.
    long long geometry_id = 0;     ///< Layer-geometry snapshot id (0 when none was captured).
    int cancelled = 0;             ///< 1 when the run stopped because cancellation was requested.
};

/** @brief Whether @p kind is a supported pipeline kind. */
bool IsValidKind(HsBaPipelineKind kind) noexcept;

/** @brief Lowercase wire name of a pipeline kind ("fdm", "sla", ...). */
std::string_view KindName(HsBaPipelineKind kind);

/** @brief Parse a wire name to a kind; returns HSBA_PIPELINE_KIND_COUNT when unknown. */
HsBaPipelineKind ParseKind(const std::string& name);

/** @brief Reflective TypeInfo* of a kind's config struct (for config_bridge). */
Utils::TypeInfo* ConfigTypeInfo(HsBaPipelineKind kind);

/**
 * @brief Heap-allocate a kind's config struct initialized to the DLL defaults.
 * @return Pointer to the concrete HsBa*PipelineConfig_t (all string fields NULL).
 * @throws InvalidArgumentError for an unsupported kind.
 */
void* CreateDefaultConfig(HsBaPipelineKind kind);

/** @brief Release a config struct produced by CreateDefaultConfig (string fields live in the arena). */
void DestroyConfig(HsBaPipelineKind kind, void* config);

/**
 * @brief Enable or disable per-layer geometry capture of a config.
 *
 * Written straight into the struct field instead of through config_bridge, because the
 * tail-added ABI fields are deliberately not part of the reflected parameter table (no
 * wide-table column, no proto field, no JSON key of a slicing config).
 */
void SetCaptureGeometry(HsBaPipelineKind kind, void* config, int enabled);

/**
 * @brief Point a config at the cancellation flag a running job polls.
 * @param flag Address of a std::atomic<bool> that outlives the run (owned by the job record).
 */
void SetCancelFlag(HsBaPipelineKind kind, void* config, const void* flag);

/**
 * @brief Run a kind's pipeline synchronously and capture a normalized outcome.
 * @param kind      Pipeline kind.
 * @param config    Config from CreateDefaultConfig (fully populated by the caller).
 * @param callback  Progress callback (may be null).
 * @param user_data Progress callback user data (may be null).
 * @return Copied-out outcome; the underlying C result is freed internally.
 */
SliceOutcome RunPipeline(HsBaPipelineKind kind, const void* config, HsBaProgressCallback callback, void* user_data);

/**
 * @brief Decode proto bytes into a config and serialize it to JSON/XML text.
 * @throws InvalidArgumentError for an unsupported kind; RuntimeError when the
 *         proto payload fails to parse.
 */
std::string ConfigFromProto(HsBaPipelineKind kind, const void* data, std::size_t size, DataFormat fmt);

/**
 * @brief Parse a JSON/XML config body into a config and encode it as proto bytes.
 * @return The serialized protobuf payload (owned by the caller).
 * @throws InvalidArgumentError for an unsupported kind; RuntimeError on parse or
 *         serialization failure.
 */
std::vector<unsigned char> ConfigToProto(HsBaPipelineKind kind, const std::string& text, DataFormat inFmt);

}  // namespace HsBa::Slicer::RestApi

#endif  // HSBA_SLICER_RESTAPI_PIPELINE_EXECUTOR_HPP
