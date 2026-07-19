#pragma once
#ifndef HSBA_SLICER_PIPELINE_CONVERT_H
#define HSBA_SLICER_PIPELINE_CONVERT_H

#include "dllexport.h"
#include "fdm_pipeline.h"
#include "sla_pipeline.h"

#ifdef __cplusplus
extern "C"
{
#endif  // __cplusplus

    /* ========================================================================
     *  Proto serialized bytes  <-->  C struct
     *
     *  Proto bytes are serialized protobuf wire format.
     *  All output buffers are allocated with malloc and must be freed by caller.
     * ====================================================================== */

    /**
     * @brief Deserialize FDM config from proto bytes.
     * @param proto_data Serialized proto bytes (msg_fdm_pipeline_config).
     * @param proto_size Size of proto_data in bytes.
     * @param config Output C config struct.
     * @return 1 on success, 0 on parse failure.
     */
    HSBA_SLICER_API int HsBaFdmConfigFromProtoBytes(const void* proto_data, int proto_size,
                                                     HsBaFdmPipelineConfig_t* config);

    /**
     * @brief Serialize FDM config to proto bytes.
     * @param config Input C config struct.
     * @param out_data Output buffer (malloc-allocated, caller must free).
     * @param out_size Output buffer size in bytes.
     * @return 1 on success, 0 on serialization failure.
     */
    HSBA_SLICER_API int HsBaFdmConfigToProtoBytes(const HsBaFdmPipelineConfig_t* config,
                                                   void** out_data, int* out_size);

    /**
     * @brief Deserialize FDM result from proto bytes.
     * @param proto_data Serialized proto bytes (msg_fdm_pipe_result).
     * @param proto_size Size of proto_data in bytes.
     * @param result Output C result struct.
     * @return 1 on success, 0 on parse failure.
     */
    HSBA_SLICER_API int HsBaFdmResultFromProtoBytes(const void* proto_data, int proto_size,
                                                     HsBaFdmPipelineResult_t* result);

    /**
     * @brief Serialize FDM result to proto bytes.
     * @param result Input C result struct.
     * @param out_data Output buffer (malloc-allocated, caller must free).
     * @param out_size Output buffer size in bytes.
     * @return 1 on success, 0 on serialization failure.
     */
    HSBA_SLICER_API int HsBaFdmResultToProtoBytes(const HsBaFdmPipelineResult_t* result,
                                                   void** out_data, int* out_size);

    /**
     * @brief Deserialize SLA config from proto bytes.
     * @param proto_data Serialized proto bytes (sla_pipe_config).
     * @param proto_size Size of proto_data in bytes.
     * @param config Output C config struct.
     * @return 1 on success, 0 on parse failure.
     */
    HSBA_SLICER_API int HsBaSlaConfigFromProtoBytes(const void* proto_data, int proto_size,
                                                     HsBaSlaPipelineConfig_t* config);

    /**
     * @brief Serialize SLA config to proto bytes.
     * @param config Input C config struct.
     * @param out_data Output buffer (malloc-allocated, caller must free).
     * @param out_size Output buffer size in bytes.
     * @return 1 on success, 0 on serialization failure.
     */
    HSBA_SLICER_API int HsBaSlaConfigToProtoBytes(const HsBaSlaPipelineConfig_t* config,
                                                   void** out_data, int* out_size);

    /**
     * @brief Deserialize SLA result from proto bytes.
     * @param proto_data Serialized proto bytes (sla_pipe_result).
     * @param proto_size Size of proto_data in bytes.
     * @param result Output C result struct.
     * @return 1 on success, 0 on parse failure.
     */
    HSBA_SLICER_API int HsBaSlaResultFromProtoBytes(const void* proto_data, int proto_size,
                                                     HsBaSlaPipelineResult_t* result);

    /**
     * @brief Serialize SLA result to proto bytes.
     * @param result Input C result struct.
     * @param out_data Output buffer (malloc-allocated, caller must free).
     * @param out_size Output buffer size in bytes.
     * @return 1 on success, 0 on serialization failure.
     */
    HSBA_SLICER_API int HsBaSlaResultToProtoBytes(const HsBaSlaPipelineResult_t* result,
                                                   void** out_data, int* out_size);

    /* ========================================================================
     *  C struct memory cleanup helpers
     *
     *  Free malloc'd string fields in converted C structs.
     * ====================================================================== */

    /**
     * @brief Free malloc'd string fields in FDM config struct.
     * @param config Config struct whose string fields should be freed.
     */
    HSBA_SLICER_API void HsBaFreeFdmConfigStrings(HsBaFdmPipelineConfig_t* config);

    /**
     * @brief Free malloc'd string fields in SLA config struct.
     * @param config Config struct whose string fields should be freed.
     */
    HSBA_SLICER_API void HsBaFreeSlaConfigStrings(HsBaSlaPipelineConfig_t* config);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // !HSBA_SLICER_PIPELINE_CONVERT_H
