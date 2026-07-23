#pragma once
#ifndef HSBA_SLICER_FDM_PIPELINE_H
#define HSBA_SLICER_FDM_PIPELINE_H

#include "dllexport.h"
#include "pipelinetypes/pipeline_types.h"

#ifdef __cplusplus
extern "C"
{
#endif  // __cplusplus

    /**
     * @brief Create FDM pipeline config with default values.
     * @return Default configuration struct.
     */
    HSBA_SLICER_API HsBaFdmPipelineConfig_t HsBaCreateDefaultConfig(void);

    /**
     * @brief Run FDM full pipeline synchronously.
     *
     * Uses C++20 coroutines internally, blocks until result is ready.
     * Pipeline: Preprocess -> Slice -> Support -> Fill -> Path Generation
     *
     * @param config Pipeline configuration.
     * @param callback Progress callback (can be NULL).
     * @param user_data Callback user data (can be NULL).
     * @return Pipeline result, call HsBaFreePipelineResult to release after use.
     */
    HSBA_SLICER_API HsBaFdmPipelineResult_t HsBaRunFdmPipeline(const HsBaFdmPipelineConfig_t* config,
                                                               HsBaProgressCallback callback, void* user_data);

    /**
     * @brief Run FDM full pipeline asynchronously (non-blocking).
     *
     * Uses C++20 coroutines for async execution, returns result via callback.
     *
     * @param config Pipeline configuration.
     * @param callback Progress callback (can be NULL).
     * @param user_data Callback user data (can be NULL).
     * @param result_callback Completion callback receiving result (must not be NULL).
     * @param result_user_data Result callback user data (can be NULL).
     * @return Task handle (reserved for future cancellation, etc.).
     */
    HSBA_SLICER_API void HsBaRunFdmPipelineAsync(const HsBaFdmPipelineConfig_t* config, HsBaProgressCallback callback,
                                                 void* user_data, HsBaResultCallback result_callback,
                                                 void* result_user_data);

    /**
     * @brief Free memory allocated in pipeline result.
     *
     * Must be called after HsBaFdmPipelineResult_t is no longer needed,
     * releases internally allocated memory for gcode_content and error_message.
     *
     * @param result Result to free.
     */
    HSBA_SLICER_API void HsBaFreePipelineResult(HsBaFdmPipelineResult_t* result);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // !HSBA_SLICER_FDM_PIPELINE_H
