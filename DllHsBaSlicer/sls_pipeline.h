#pragma once
#ifndef HSBA_SLICER_SLS_PIPELINE_H
#define HSBA_SLICER_SLS_PIPELINE_H

#include "dllexport.h"

#ifdef __cplusplus
extern "C"
{
#endif  // __cplusplus

    /**
     * @brief SLS pipeline configuration (C-compatible struct).
     *
     * SLS (Selective Laser Sintering) uses a powder bed process.
     * No floor/raft or support structures are needed; the powder bed
     * provides both support and thermal stability.
     * Output format is entirely determined by the Lua export script.
     */
    typedef struct HsBaSlsPipelineConfig
    {
        /* Model Configuration */
        const char* model_name;  ///< Model name
        const char* model_path;  ///< Model file path

        /* Slice Configuration */
        float layer_height;        ///< Layer height (mm), default 0.1
        float first_layer_height;  ///< First layer height (mm), default 0.15

        /* Laser Configuration */
        float laser_power;         ///< Laser power (W), default 30.0
        float scan_speed;          ///< Scan speed (mm/s), default 2000.0
        float hatch_spacing;       ///< Hatch line spacing (mm), default 0.15
        float hatch_rotation;      ///< Hatch rotation between layers (degrees), default 90.0
        float bed_temperature;     ///< Powder bed temperature (°C), default 180.0

        /* Lua Export Configuration (required - no standard output format) */
        const char* export_lua_script;  ///< Export Lua script path (must not be NULL)
        const char* export_lua_func;    ///< Export Lua function name (NULL = "export_sls")

        /* Output Configuration */
        const char* output_path;  ///< Output file path (can be NULL)

    } HsBaSlsPipelineConfig_t;

    /**
     * @brief SLS pipeline result (C-compatible struct).
     *
     * Must call HsBaFreeSlsPipelineResult to release memory after use.
     */
    typedef struct HsBaSlsPipelineResult
    {
        int success;             ///< Success flag (0=false, 1=true)
        int total_layers;        ///< Total layer count
        char* export_path;       ///< Path to exported output file (UTF-8, caller must free)
        char* error_message;     ///< Error message (UTF-8, caller must free)
        double elapsed_seconds;  ///< Elapsed time (seconds)
    } HsBaSlsPipelineResult_t;

    /**
     * @brief Progress callback function type.
     * @param percent Progress percentage (0-100).
     * @param stage Current stage description (UTF-8 string).
     * @param user_data User-defined data pointer.
     */
    typedef void (*HsBaSlsProgressCallback)(int percent, const char* stage, void* user_data);

    /**
     * @brief Result callback for async pipeline.
     */
    typedef void (*HsBaSlsResultCallback)(HsBaSlsPipelineResult_t result, void* user_data);

    /**
     * @brief Create SLS pipeline config with default values.
     * @return Default configuration struct.
     */
    HSBA_SLICER_API HsBaSlsPipelineConfig_t HsBaCreateDefaultSlsConfig(void);

    /**
     * @brief Run SLS full pipeline synchronously.
     *
     * Pipeline: Preprocess -> Slice -> Export (Lua script: zip + database)
     *
     * @param config Pipeline configuration (export_lua_script must not be NULL).
     * @param callback Progress callback (can be NULL).
     * @param user_data Callback user data (can be NULL).
     * @return Pipeline result, call HsBaFreeSlsPipelineResult to release after use.
     */
    HSBA_SLICER_API HsBaSlsPipelineResult_t HsBaRunSlsPipeline(const HsBaSlsPipelineConfig_t* config,
                                                                HsBaSlsProgressCallback callback, void* user_data);

    /**
     * @brief Run SLS full pipeline asynchronously (non-blocking).
     *
     * @param config Pipeline configuration (export_lua_script must not be NULL).
     * @param callback Progress callback (can be NULL).
     * @param user_data Callback user data (can be NULL).
     * @param result_callback Completion callback receiving result (must not be NULL).
     * @param result_user_data Result callback user data (can be NULL).
     */
    HSBA_SLICER_API void HsBaRunSlsPipelineAsync(const HsBaSlsPipelineConfig_t* config,
                                                  HsBaSlsProgressCallback callback, void* user_data,
                                                  HsBaSlsResultCallback result_callback,
                                                  void* result_user_data);

    /**
     * @brief Free memory allocated in SLS pipeline result.
     *
     * Must be called after HsBaSlsPipelineResult_t is no longer needed.
     *
     * @param result Result to free.
     */
    HSBA_SLICER_API void HsBaFreeSlsPipelineResult(HsBaSlsPipelineResult_t* result);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // !HSBA_SLICER_SLS_PIPELINE_H
