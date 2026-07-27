#pragma once
#ifndef HSBA_SLICER_SLS_PIPELINE_H
#define HSBA_SLICER_SLS_PIPELINE_H

#include "dllexport.h"
#include "pipelinetypes/pipeline_types.h"

#ifdef __cplusplus
extern "C"
{
#endif  // __cplusplus

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
                                                 HsBaSlsResultCallback result_callback, void* result_user_data);

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
