/** @file tdp_pipeline.h
 * @brief C ABI for the 3DP (three-dimensional printing) slicing pipeline.
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_TDP_PIPELINE_H
#define HSBA_SLICER_TDP_PIPELINE_H

#include "dllexport.h"
#include "pipelinetypes/pipeline_types.h"

#ifdef __cplusplus
extern "C"
{
#endif  // __cplusplus

    /**
     * @brief Create 3DP pipeline config with default values.
     * @return Default configuration struct.
     */
    HSBA_SLICER_API HsBaTdpPipelineConfig_t HsBaCreateDefaultTdpConfig(void);

    /**
     * @brief Run 3DP full pipeline synchronously.
     *
     * Pipeline: Preprocess -> Slice -> Export (Lua script: zip + database)
     *
     * 3DP (binder jetting) slices per layer and hands the binder-jet outlines
     * and head/curing parameters to the Lua export script.
     *
     * @param config Pipeline configuration (export_lua_script must not be NULL).
     * @param callback Progress callback (can be NULL).
     * @param user_data Callback user data (can be NULL).
     * @return Pipeline result, call HsBaFreeTdpPipelineResult to release after use.
     */
    HSBA_SLICER_API HsBaTdpPipelineResult_t HsBaRunTdpPipeline(const HsBaTdpPipelineConfig_t* config,
                                                               HsBaTdpProgressCallback callback, void* user_data);

    /**
     * @brief Run 3DP full pipeline asynchronously (non-blocking).
     *
     * @param config Pipeline configuration (export_lua_script must not be NULL).
     * @param callback Progress callback (can be NULL).
     * @param user_data Callback user data (can be NULL).
     * @param result_callback Completion callback receiving result (must not be NULL).
     * @param result_user_data Result callback user data (can be NULL).
     */
    HSBA_SLICER_API void HsBaRunTdpPipelineAsync(const HsBaTdpPipelineConfig_t* config,
                                                 HsBaTdpProgressCallback callback, void* user_data,
                                                 HsBaTdpResultCallback result_callback, void* result_user_data);

    /**
     * @brief Free memory allocated in 3DP pipeline result.
     * @param result Result to free.
     */
    HSBA_SLICER_API void HsBaFreeTdpPipelineResult(HsBaTdpPipelineResult_t* result);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // !HSBA_SLICER_TDP_PIPELINE_H
