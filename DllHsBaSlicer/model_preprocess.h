/** @file model_preprocess.h
 * @brief C ABI for model loading, transforms, queries, boolean/thick-solid preprocessing and text CSG.
 * @author HsBa
 */
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
     *
     * Also drops a process-wide copy created by HsBaPublishModel, so after this call
     * no thread resolves the name anymore.
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

    /**
     * @brief One model's triangle soup, coordinates in millimetres.
     *
     * Both arrays live inside a single malloc block that starts at @p xyz, the same
     * ownership contract as HsBaLayerGeometry_t: the host must not free the arrays
     * individually and must call HsBaFreeModelTriangles once it is done copying the
     * data out. An empty face section still hands back a valid (shared) address
     * inside the block rather than NULL, so a mesh-less model exports successfully
     * with both counts at zero.
     */
    typedef struct HsBaModelTriangles
    {
        int vertex_count;   ///< Number of stored vertices
        const float* xyz;   ///< Flattened interleaved coordinates, length 3 * vertex_count; always the start of the owned block
        int face_count;     ///< Number of stored triangles
        const int* indices; ///< Flattened zero-based vertex indices, length 3 * face_count, inside the same block
    } HsBaModelTriangles_t;

    /**
     * @brief Export a model's triangle mesh into a freshly allocated block.
     *
     * Mesh models (STL/OBJ/PLY/OFF) return their stored triangles; BRep models
     * (STEP/IGES/BREP, text CSG) are tessellated on demand, so their triangle
     * count depends on the kernel's deflection settings.
     *
     * @param name Model name (UTF-8).
     * @param out Destination struct, overwritten on success and left zeroed on failure.
     * @return 1 on success, 0 for an unknown model or a NULL argument.
     */
    HSBA_SLICER_API int HsBaGetModelTriangles(const char* name, HsBaModelTriangles_t* out);

    /**
     * @brief Release the block handed out by HsBaGetModelTriangles and zero the struct.
     * @param tris Triangle export to free; may be NULL or already freed (then a no-op).
     */
    HSBA_SLICER_API void HsBaFreeModelTriangles(HsBaModelTriangles_t* tris);

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
    HSBA_SLICER_API void* HsBaBooleanDifference(const char* left_name, const char* right_name, const char* result_name);

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

    /* ========================================================================
     *  Model Preprocessing - Text CSG (OCCT DRAW Test Harness)
     *
     *  A CSG model is described by text rather than by a file, so the failure modes are
     *  the caller's to read: every entry point below reports why it refused, in prose,
     *  instead of leaving a NULL pointer to be guessed at.
     * ====================================================================== */

    /**
     * @brief Check whether this build can evaluate CSG scripts.
     *
     * Text CSG needs OpenCASCADE built with its Draw module (vcpkg feature
     * opencascade[tcl]); the build normally refuses to configure without it
     * (HSBA_REQUIRE_TEXT_CSG=ON), so 0 means the feature was turned off deliberately.
     * Hosts use this to hide the feature instead of presenting a call that always fails.
     * @return 1 when HsBaBuildCsgModel can produce a model, 0 otherwise.
     */
    HSBA_SLICER_API int HsBaTextCsgAvailable(void);

    /**
     * @brief Build a B-Rep model from an OCCT DRAW script (text CSG) and store it in the pool.
     *
     * The script is DRAW Test Harness text, one command per line: primitives
     * (box, wedge, pcylinder, pcone, psphere, ptorus), transforms (ttranslate, trotate,
     * tscale), bank utilities (compound, explode, checkshape, vprops) and booleans
     * (bcut, bfuse, bcommon), with Tcl set/expr available for parameters. The model can
     * then be queried, transformed and sliced like any model loaded from a file.
     *
     * Scripts are not sandboxed: the interpreter is a full Tcl one, so only scripts from a
     * trusted source should be passed here.
     *
     * @param name Unique model name (UTF-8); an existing name is reported as a failure
     *             rather than silently replaced.
     * @param script DRAW commands, lines separated by '\n' (UTF-8).
     * @param shape_name DRAW variable holding the result; NULL or "" selects "result".
     * @param error_out Optional buffer for a NUL-terminated UTF-8 reason. It is written on
     *                  every call - empty on success - so one print covers both outcomes.
     * @param error_cap Size of @p error_out in bytes including the terminator; ignored when
     *                  @p error_out is NULL. A longer message is truncated, never dropped.
     * @return Opaque model handle (release with HsBaReleaseModelHandle), NULL on failure.
     *
     * @note The pool is thread local: to slice this model from a pipeline coroutine, pass
     *       HsBaPublishModel(name) first; by-name use on the caller's thread needs nothing.
     */
    HSBA_SLICER_API void* HsBaBuildCsgModel(const char* name, const char* script, const char* shape_name,
                                           char* error_out, int error_cap);

    /**
     * @brief Release a model handle returned by HsBaLoadModel/HsBaGetModel/Boolean/ThickSolid/BuildCsgModel.
     *
     * Decrements the reference count. The model remains in the pool until
     * HsBaRemoveModel is called or HsBaCleanupModels reclaims it.
     *
     * @param handle Opaque model handle (can be NULL).
     */
    HSBA_SLICER_API void HsBaReleaseModelHandle(void* handle);

    /**
     * @brief Expose a pooled model to by-name lookups from other threads.
     *
     * The model pool is thread local, and a pipeline coroutine runs its stages on
     * executor threads, so a model built or loaded on the caller's thread is invisible
     * to HsBaRun*Pipeline by name until it is published here. Publishing keeps a
     * process-wide reference under the same name; queries, transforms, triangle export
     * and pipelines then share one instance, and HsBaRemoveModel or
     * HsBaUnpublishModel withdraw it again.
     * @param name Model name (UTF-8).
     * @return 1 when the name exists (locally or already published), 0 otherwise.
     */
    HSBA_SLICER_API int HsBaPublishModel(const char* name);

    /**
     * @brief Withdraw the process-wide reference created by HsBaPublishModel.
     *
     * The publishing thread's own pool entry stays; after this call only threads that
     * still hold a handle or a local copy can see the model.
     * @param name Model name (UTF-8).
     * @return 1 when a published copy was dropped, 0 when there was none.
     */
    HSBA_SLICER_API int HsBaUnpublishModel(const char* name);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // !HSBA_SLICER_MODEL_PREPROCESS_H
