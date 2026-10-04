/** @file slm_pipeline.h
 * @brief C ABI for the SLM (selective laser melting) slicing pipeline.
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_SLM_PIPELINE_H
#define HSBA_SLICER_SLM_PIPELINE_H

#include "dllexport.h"
#include "pipelinetypes/pipeline_types.h"

#ifdef __cplusplus
extern "C"
{
#endif  // __cplusplus

    /**
     * @brief Create SLM pipeline config with default values.
     * @return Default configuration struct.
     */
    HSBA_SLICER_API HsBaSlmPipelineConfig_t HsBaCreateDefaultSlmConfig(void);

    /**
     * @brief Run SLM full pipeline synchronously.
     *
     * Pipeline: Preprocess -> Slice -> Export (Lua script: zip + database)
     *
     * SLM (metal powder-bed) mirrors the SLS flow; metal-specific parameters
     * (material, energy source, shielding gas) are folded into the config JSON.
     *
     * @param config Pipeline configuration (export_lua_script must not be NULL).
     * @param callback Progress callback (can be NULL).
     * @param user_data Callback user data (can be NULL).
     * @return Pipeline result, call HsBaFreeSlmPipelineResult to release after use.
     */
    HSBA_SLICER_API HsBaSlmPipelineResult_t HsBaRunSlmPipeline(const HsBaSlmPipelineConfig_t* config,
                                                               HsBaSlmProgressCallback callback, void* user_data);

    /**
     * @brief Run SLM full pipeline asynchronously (non-blocking).
     *
     * @param config Pipeline configuration (export_lua_script must not be NULL).
     * @param callback Progress callback (can be NULL).
     * @param user_data Callback user data (can be NULL).
     * @param result_callback Completion callback receiving result (must not be NULL).
     * @param result_user_data Result callback user data (can be NULL).
     */
    HSBA_SLICER_API void HsBaRunSlmPipelineAsync(const HsBaSlmPipelineConfig_t* config,
                                                 HsBaSlmProgressCallback callback, void* user_data,
                                                 HsBaSlmResultCallback result_callback, void* result_user_data);

    /**
     * @brief Free memory allocated in SLM pipeline result.
     * @param result Result to free.
     */
    HSBA_SLICER_API void HsBaFreeSlmPipelineResult(HsBaSlmPipelineResult_t* result);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // !HSBA_SLICER_SLM_PIPELINE_H
