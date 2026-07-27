#pragma once
#ifndef HSBA_SLICER_FILE_TRANSFER_PIPELINE_H
#define HSBA_SLICER_FILE_TRANSFER_PIPELINE_H

#include "dllexport.h"
#include "pipelinetypes/pipeline_types.h"

#ifdef __cplusplus
extern "C"
{
#endif  // __cplusplus

    /**
     * @brief Create file transfer pipeline config with default values.
     * @return Default configuration struct.
     */
    HSBA_SLICER_API HsBaFileTransferPipelineConfig_t HsBaCreateDefaultFileTransferConfig(void);

    /**
     * @brief Run file transfer pipeline synchronously.
     *
     * Uses RemoteExecutorConnectionPool internally to send files to a remote service.
     * Pipeline: Validate -> Connect -> Transfer Files
     *
     * @param config Pipeline configuration (host, port, file_paths must not be NULL).
     * @param callback Progress callback (can be NULL).
     * @param user_data Callback user data (can be NULL).
     * @return Pipeline result, call HsBaFreeFileTransferPipelineResult to release after use.
     */
    HSBA_SLICER_API HsBaFileTransferPipelineResult_t HsBaRunFileTransferPipeline(
        const HsBaFileTransferPipelineConfig_t* config, HsBaFileTransferProgressCallback callback, void* user_data);

    /**
     * @brief Run file transfer pipeline asynchronously (non-blocking).
     *
     * Uses C++20 coroutines for async execution, returns result via callback.
     *
     * @param config Pipeline configuration (host, port, file_paths must not be NULL).
     * @param callback Progress callback (can be NULL).
     * @param user_data Callback user data (can be NULL).
     * @param result_callback Completion callback receiving result (must not be NULL).
     * @param result_user_data Result callback user data (can be NULL).
     */
    HSBA_SLICER_API void HsBaRunFileTransferPipelineAsync(const HsBaFileTransferPipelineConfig_t* config,
                                                          HsBaFileTransferProgressCallback callback, void* user_data,
                                                          HsBaFileTransferResultCallback result_callback,
                                                          void* result_user_data);

    /**
     * @brief Free memory allocated in file transfer pipeline result.
     *
     * Must be called after HsBaFileTransferPipelineResult_t is no longer needed.
     *
     * @param result Result to free.
     */
    HSBA_SLICER_API void HsBaFreeFileTransferPipelineResult(HsBaFileTransferPipelineResult_t* result);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // !HSBA_SLICER_FILE_TRANSFER_PIPELINE_H
