#pragma once
#ifndef HSBA_SLICER_MODEL_PREPROCESS_H
#define HSBA_SLICER_MODEL_PREPROCESS_H

#include "dllexport.h"

#ifdef __cplusplus
extern "C"
{
#endif  // __cplusplus

    /* ========================================================================
     *  Model Preprocessing - Basic Operations
     * ====================================================================== */

    /**
     * @brief Load a model from file into the internal pool.
     *
     * Mesh formats (STL/OBJ/PLY/OFF) are loaded via IGL.
     * BRep formats (STEP/IGES/VRML/BREP) are loaded via OCCT (if available).
     *
     * @param name Unique model name (UTF-8).
     * @param file_path Model file path (UTF-8).
     * @return Opaque model handle (shared_ptr ref-counted), NULL on failure.
     */
    HSBA_SLICER_API void* HsBaLoadModel(const char* name, const char* file_path);

    /**
     * @brief Get a previously loaded model by name.
     * @param name Model name (UTF-8).
     * @return Opaque model handle, NULL if not found.
     */
    HSBA_SLICER_API void* HsBaGetModel(const char* name);

    /**
     * @brief Remove a model from the pool.
     * @param name Model name (UTF-8).
     */
    HSBA_SLICER_API void HsBaRemoveModel(const char* name);

    /**
     * @brief Check whether a model with the given name exists.
     * @param name Model name (UTF-8).
     * @return 1 if exists, 0 otherwise.
     */
    HSBA_SLICER_API int HsBaContainsModel(const char* name);

    /**
     * @brief Return the number of models currently in the pool.
     */
    HSBA_SLICER_API int HsBaModelCount(void);

    /**
     * @brief Clean up models that are no longer referenced externally.
     * @return Number of models removed.
     */
    HSBA_SLICER_API int HsBaCleanupModels(void);

    /* ========================================================================
     *  Model Preprocessing - Transform Operations
     * ====================================================================== */

    /**
     * @brief Apply translation transform to a model.
     * @param name Model name (UTF-8).
     * @param tx Translation X (mm).
     * @param ty Translation Y (mm).
     * @param tz Translation Z (mm).
     * @return 1 on success, 0 if model not found.
     */
    HSBA_SLICER_API int HsBaTranslateModel(const char* name, float tx, float ty, float tz);

    /**
     * @brief Apply rotation transform to a model (quaternion).
     * @param name Model name (UTF-8).
     * @param qx Quaternion X component.
     * @param qy Quaternion Y component.
     * @param qz Quaternion Z component.
     * @param qw Quaternion W component.
     * @return 1 on success, 0 if model not found.
     */
    HSBA_SLICER_API int HsBaRotateModel(const char* name, float qx, float qy, float qz, float qw);

    /**
     * @brief Apply uniform scale transform to a model.
     * @param name Model name (UTF-8).
     * @param scale Uniform scale factor.
     * @return 1 on success, 0 if model not found.
     */
    HSBA_SLICER_API int HsBaScaleModelUniform(const char* name, float scale);

    /**
     * @brief Apply non-uniform scale transform to a model.
     * @param name Model name (UTF-8).
     * @param sx Scale factor X.
     * @param sy Scale factor Y.
     * @param sz Scale factor Z.
     * @return 1 on success, 0 if model not found.
     */
    HSBA_SLICER_API int HsBaScaleModel(const char* name, float sx, float sy, float sz);

    /* ========================================================================
     *  Model Preprocessing - Query Operations
     * ====================================================================== */

    /**
     * @brief Get model bounding box and volume.
     * @param name Model name (UTF-8).
     * @param out_bbox_min Output bounding box minimum (3 floats: x,y,z).
     * @param out_bbox_max Output bounding box maximum (3 floats: x,y,z).
     * @param out_volume Output model volume.
     * @return 1 on success, 0 if model not found.
     */
    HSBA_SLICER_API int HsBaGetModelInfo(const char* name, float out_bbox_min[3], float out_bbox_max[3],
                                          float* out_volume);

    /* ========================================================================
     *  Model Preprocessing - Advanced Operations (require CGAL/OCCT)
     * ====================================================================== */

    /**
     * @brief Perform a thick-solid (shell) operation on a BRep model.
     *
     * Requires OCCT. The source model must be a BRep model loaded from
     * STEP/IGES/VRML/BREP format.
     *
     * @param source_name Source model name (UTF-8).
     * @param result_name Result model name (UTF-8).
     * @param thickness Shell thickness (positive = inward).
     * @return Opaque model handle to result, NULL on failure.
     */
    HSBA_SLICER_API void* HsBaThickSolidModel(const char* source_name, const char* result_name, float thickness);

    /**
     * @brief Boolean union of two models.
     *
     * Uses OCCT for BRep-BRep operations, falls back to IGL/CGAL for mesh models.
     *
     * @param left_name Left model name (UTF-8).
     * @param right_name Right model name (UTF-8).
     * @param result_name Result model name (UTF-8).
     * @return Opaque model handle to result, NULL on failure.
     */
    HSBA_SLICER_API void* HsBaBooleanUnion(const char* left_name, const char* right_name, const char* result_name);

    /**
     * @brief Boolean intersection of two models.
     *
     * Uses OCCT for BRep-BRep operations, falls back to IGL/CGAL for mesh models.
     *
     * @param left_name Left model name (UTF-8).
     * @param right_name Right model name (UTF-8).
     * @param result_name Result model name (UTF-8).
     * @return Opaque model handle to result, NULL on failure.
     */
    HSBA_SLICER_API void* HsBaBooleanIntersection(const char* left_name, const char* right_name,
                                                   const char* result_name);

    /**
     * @brief Boolean difference of two models (left - right).
     *
     * Uses OCCT for BRep-BRep operations, falls back to IGL/CGAL for mesh models.
     *
     * @param left_name Left model name (UTF-8).
     * @param right_name Right model name (UTF-8).
     * @param result_name Result model name (UTF-8).
     * @return Opaque model handle to result, NULL on failure.
     */
    HSBA_SLICER_API void* HsBaBooleanDifference(const char* left_name, const char* right_name,
                                                 const char* result_name);

    /**
     * @brief Boolean XOR of two models.
     *
     * Uses OCCT for BRep-BRep operations, falls back to IGL/CGAL for mesh models.
     *
     * @param left_name Left model name (UTF-8).
     * @param right_name Right model name (UTF-8).
     * @param result_name Result model name (UTF-8).
     * @return Opaque model handle to result, NULL on failure.
     */
    HSBA_SLICER_API void* HsBaBooleanXor(const char* left_name, const char* right_name, const char* result_name);

    /**
     * @brief Release a model handle returned by HsBaLoadModel/HsBaGetModel/Boolean/ThickSolid.
     *
     * Decrements the reference count. The model remains in the pool until
     * HsBaRemoveModel is called or HsBaCleanupModels reclaims it.
     *
     * @param handle Opaque model handle (can be NULL).
     */
    HSBA_SLICER_API void HsBaReleaseModelHandle(void* handle);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // !HSBA_SLICER_MODEL_PREPROCESS_H
