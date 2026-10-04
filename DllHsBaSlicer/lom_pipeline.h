/** @file lom_pipeline.h
 * @brief C ABI for the LOM (laminated object manufacturing) slicing pipeline.
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_LOM_PIPELINE_H
#define HSBA_SLICER_LOM_PIPELINE_H

#include "dllexport.h"
#include "pipelinetypes/pipeline_types.h"

#ifdef __cplusplus
extern "C"
{
#endif  // __cplusplus

    /**
     * @brief Create LOM pipeline config with default values.
     * @return Default configuration struct.
     */
    HSBA_SLICER_API HsBaLomPipelineConfig_t HsBaCreateDefaultLomConfig(void);

    /**
     * @brief Run LOM full pipeline synchronously.
     *
     * Pipeline: Preprocess -> Slice -> Export (Lua script: zip + database)
     *
     * LOM (laminated object manufacturing) slices per sheet thickness and hands
     * the contour outlines plus cut/bond parameters to the Lua export script.
     *
     * @param config Pipeline configuration (export_lua_script must not be NULL).
     * @param callback Progress callback (can be NULL).
     * @param user_data Callback user data (can be NULL).
     * @return Pipeline result, call HsBaFreeLomPipelineResult to release after use.
     */
    HSBA_SLICER_API HsBaLomPipelineResult_t HsBaRunLomPipeline(const HsBaLomPipelineConfig_t* config,
                                                               HsBaLomProgressCallback callback, void* user_data);

    /**
     * @brief Run LOM full pipeline asynchronously (non-blocking).
     *
     * @param config Pipeline configuration (export_lua_script must not be NULL).
     * @param callback Progress callback (can be NULL).
     * @param user_data Callback user data (can be NULL).
     * @param result_callback Completion callback receiving result (must not be NULL).
     * @param result_user_data Result callback user data (can be NULL).
     */
    HSBA_SLICER_API void HsBaRunLomPipelineAsync(const HsBaLomPipelineConfig_t* config,
                                                 HsBaLomProgressCallback callback, void* user_data,
                                                 HsBaLomResultCallback result_callback, void* result_user_data);

    /**
     * @brief Free memory allocated in LOM pipeline result.
     * @param result Result to free.
     */
    HSBA_SLICER_API void HsBaFreeLomPipelineResult(HsBaLomPipelineResult_t* result);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // !HSBA_SLICER_LOM_PIPELINE_H
