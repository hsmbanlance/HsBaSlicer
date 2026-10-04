#pragma once
#ifndef HSBA_SLICER_CUSTOM_PIPELINE_H
#define HSBA_SLICER_CUSTOM_PIPELINE_H

#include "dllexport.h"
#include "pipelinetypes/pipeline_types.h"

#ifdef __cplusplus
extern "C"
{
#endif  // __cplusplus

    /**
     * @brief Create custom Lua pipeline config with default values.
     * @return Default configuration struct.
     */
    HSBA_SLICER_API HsBaCustomPipelineConfig_t HsBaCreateDefaultCustomConfig(void);

    /**
     * @brief Run a fully Lua-driven custom pipeline synchronously.
     *
     * The entire workflow is defined by the Lua entry function (default
     * "run_pipeline") inside the provided script. The Lua environment exposes
     * all pipeline building blocks through the global `HsBa` table (model
     * loading, slicing, support, fill, floor, G-code path output, SLA/SLS
     * packaging) plus the standard polygon/support/fill/file libraries.
     *
     * @param config Pipeline configuration (pipeline_lua_script or
     *               pipeline_lua_source must be set).
     * @param callback Progress callback driven by `HsBa.progress()` in Lua (can be NULL).
     * @param user_data Callback user data (can be NULL).
     * @return Pipeline result, call HsBaFreeCustomPipelineResult to release after use.
     */
    HSBA_SLICER_API HsBaCustomPipelineResult_t HsBaRunCustomPipeline(const HsBaCustomPipelineConfig_t* config,
                                                                     HsBaCustomProgressCallback callback,
                                                                     void* user_data);

    /**
     * @brief Run custom Lua pipeline asynchronously (non-blocking).
     *
     * Uses C++20 coroutines for async execution, returns result via callback.
     *
     * @param config Pipeline configuration.
     * @param callback Progress callback (can be NULL).
     * @param user_data Callback user data (can be NULL).
     * @param result_callback Completion callback receiving result (must not be NULL).
     * @param result_user_data Result callback user data (can be NULL).
     */
    HSBA_SLICER_API void HsBaRunCustomPipelineAsync(const HsBaCustomPipelineConfig_t* config,
                                                    HsBaCustomProgressCallback callback, void* user_data,
                                                    HsBaCustomResultCallback result_callback, void* result_user_data);

    /**
     * @brief Free memory allocated in custom pipeline result.
     *
     * @param result Result to free.
     */
    HSBA_SLICER_API void HsBaFreeCustomPipelineResult(HsBaCustomPipelineResult_t* result);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // !HSBA_SLICER_CUSTOM_PIPELINE_H
