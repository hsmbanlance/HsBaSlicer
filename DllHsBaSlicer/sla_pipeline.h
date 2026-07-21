#pragma once
#ifndef HSBA_SLICER_SLA_PIPELINE_H
#define HSBA_SLICER_SLA_PIPELINE_H

#include "dllexport.h"
#include "pipelinetypes/pipeline_types.h"

#ifdef __cplusplus
extern "C"
{
#endif  // __cplusplus

    /**
     * @brief Create SLA pipeline config with default values.
     * @return Default configuration struct.
     */
    HSBA_SLICER_API HsBaSlaPipelineConfig_t HsBaCreateDefaultSlaConfig(void);

    /**
     * @brief Run SLA full pipeline synchronously.
     *
     * Pipeline: Preprocess -> Slice -> Floor -> Support -> Export (zip)
     *
     * @param config Pipeline configuration.
     * @param callback Progress callback (can be NULL).
     * @param user_data Callback user data (can be NULL).
     * @return Pipeline result, call HsBaFreeSlaPipelineResult to release after use.
     */
    HSBA_SLICER_API HsBaSlaPipelineResult_t HsBaRunSlaPipeline(const HsBaSlaPipelineConfig_t* config,
                                                               HsBaSlaProgressCallback callback, void* user_data);

    /**
     * @brief Run SLA full pipeline asynchronously (non-blocking).
     *
     * @param config Pipeline configuration.
     * @param callback Progress callback (can be NULL).
     * @param user_data Callback user data (can be NULL).
     * @param result_callback Completion callback receiving result (must not be NULL).
     * @param result_user_data Result callback user data (can be NULL).
     */
    HSBA_SLICER_API void HsBaRunSlaPipelineAsync(const HsBaSlaPipelineConfig_t* config,
                                                  HsBaSlaProgressCallback callback, void* user_data,
                                                  HsBaSlaResultCallback result_callback,
                                                  void* result_user_data);

    /**
     * @brief Free memory allocated in SLA pipeline result.
     *
     * Must be called after HsBaSlaPipelineResult_t is no longer needed.
     *
     * @param result Result to free.
     */
    HSBA_SLICER_API void HsBaFreeSlaPipelineResult(HsBaSlaPipelineResult_t* result);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // !HSBA_SLICER_SLA_PIPELINE_H
