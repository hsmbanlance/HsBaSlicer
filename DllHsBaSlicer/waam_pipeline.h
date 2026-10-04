/** @file waam_pipeline.h
 * @brief C ABI for the WAAM (wire arc additive manufacturing) slicing pipeline.
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_WAAM_PIPELINE_H
#define HSBA_SLICER_WAAM_PIPELINE_H

#include "dllexport.h"
#include "pipelinetypes/pipeline_types.h"

#ifdef __cplusplus
extern "C"
{
#endif  // __cplusplus

    /**
     * @brief Create WAAM pipeline config with default values.
     * @return Default configuration struct.
     */
    HSBA_SLICER_API HsBaWaamPipelineConfig_t HsBaCreateDefaultWaamConfig(void);

    /**
     * @brief Run WAAM full pipeline synchronously.
     *
     * Pipeline: Preprocess -> Slice -> Robot path export
     *
     * WAAM (wire arc additive manufacturing) deposits metal bead-by-bead along
     * the per-layer contours; the output is a robot language program
     * (ABB / KUKA / FANUC) rather than a layer zip archive.
     *
     * @param config Pipeline configuration.
     * @param callback Progress callback (can be NULL).
     * @param user_data Callback user data (can be NULL).
     * @return Pipeline result, call HsBaFreeWaamPipelineResult to release after use.
     */
    HSBA_SLICER_API HsBaWaamPipelineResult_t HsBaRunWaamPipeline(const HsBaWaamPipelineConfig_t* config,
                                                                 HsBaWaamProgressCallback callback, void* user_data);

    /**
     * @brief Run WAAM full pipeline asynchronously (non-blocking).
     *
     * @param config Pipeline configuration.
     * @param callback Progress callback (can be NULL).
     * @param user_data Callback user data (can be NULL).
     * @param result_callback Completion callback receiving result (must not be NULL).
     * @param result_user_data Result callback user data (can be NULL).
     */
    HSBA_SLICER_API void HsBaRunWaamPipelineAsync(const HsBaWaamPipelineConfig_t* config,
                                                  HsBaWaamProgressCallback callback, void* user_data,
                                                  HsBaWaamResultCallback result_callback, void* result_user_data);

    /**
     * @brief Free memory allocated in WAAM pipeline result.
     * @param result Result to free.
     */
    HSBA_SLICER_API void HsBaFreeWaamPipelineResult(HsBaWaamPipelineResult_t* result);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // !HSBA_SLICER_WAAM_PIPELINE_H
