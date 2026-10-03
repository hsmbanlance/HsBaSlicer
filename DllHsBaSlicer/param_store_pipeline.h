#pragma once
#ifndef HSBA_SLICER_PARAM_STORE_PIPELINE_H
#define HSBA_SLICER_PARAM_STORE_PIPELINE_H

#include "dllexport.h"
#include "pipelinetypes/pipeline_types.h"

#ifdef __cplusplus
extern "C"
{
#endif  // __cplusplus

    /**
     * @brief Save (upsert) a pipeline config struct into the parameter store.
     *
     * Pipeline: Connect -> EnsureTable -> Reflect -> Coerce -> Upsert.
     *
     * @param conn   Connection parameters (backend + path/host/user/...). Must not be NULL.
     * @param kind   Which PipelineConfig type the config pointer refers to.
     * @param table  Target table name; NULL or "" selects the type-derived default table.
     * @param key    Business-unique key for the row. Must not be NULL.
     * @param config Pointer to the config struct (e.g. HsBaFdmPipelineConfig_t). Must not be NULL.
     * @return Result; on success param_id is the persisted row id. Free error_message with
     *         HsBaFreeParamStoreResult.
     */
    HSBA_SLICER_API HsBaParamStoreResult_t HsBaSavePipelineParams(const HsBaParamStoreConn_t* conn,
                                                                  HsBaPipelineKind kind, const char* table,
                                                                  const char* key, const void* config);

    /**
     * @brief Load a pipeline config by key into a caller-provided struct.
     *
     * The config's const char* fields are filled with malloc-allocated strings owned by the
     * caller; release them with HsBaFreeLoadedPipelineConfig before reusing/freeing the struct.
     *
     * @param conn        Connection parameters. Must not be NULL.
     * @param kind        Which PipelineConfig type out_config points to.
     * @param table       Target table name; NULL or "" selects the type-derived default table.
     * @param key         Business-unique key to look up. Must not be NULL.
     * @param out_config  Pointer to a config struct (should be pre-initialized via *ConfigDefault()).
     * @return Result; success=0 when the key is not found or the backend is unavailable. Free
     *         error_message with HsBaFreeParamStoreResult.
     */
    HSBA_SLICER_API HsBaParamStoreResult_t HsBaLoadPipelineParams(const HsBaParamStoreConn_t* conn,
                                                                  HsBaPipelineKind kind, const char* table,
                                                                  const char* key, void* out_config);

    /**
     * @brief Free the malloc-owned const char* string fields inside a loaded pipeline config.
     *
     * Must be called for a config filled by HsBaLoadPipelineParams before discarding the struct.
     *
     * @param kind    The config type.
     * @param config  Pointer to the config struct previously loaded.
     */
    HSBA_SLICER_API void HsBaFreeLoadedPipelineConfig(HsBaPipelineKind kind, void* config);

    /**
     * @brief Free memory allocated in a ParamStore result.
     *
     * Must be called after HsBaParamStoreResult_t is no longer needed.
     *
     * @param result Result to free.
     */
    HSBA_SLICER_API void HsBaFreeParamStoreResult(HsBaParamStoreResult_t* result);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // !HSBA_SLICER_PARAM_STORE_PIPELINE_H
