/** @file model_preprocess.hpp
 * @brief Lib-side model loading, transform, query and boolean/thick-solid preprocessing C++ API.
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_MODEL_PREPROCESS_HPP
#define HSBA_SLICER_MODEL_PREPROCESS_HPP

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <Eigen/Core>
#include <Eigen/Geometry>

#include "../export.h"
#include "base/IModel.hpp"

namespace HsBa::Slicer
{
/**
 * @brief Model information struct.
 */
struct ModelInfo
{
    Eigen::Vector3f bbox_min;  ///< Bounding box minimum corner
    Eigen::Vector3f bbox_max;  ///< Bounding box maximum corner
    float volume = 0.0f;       ///< Model volume
};

/**
 * @brief Load a model and store it in the internal pool.
 * @param name Unique model name.
 * @param filePath Model file path (supports STL/OBJ/STEP/IGES, etc.).
 * @return Shared pointer to the loaded model.
 * @throws InvalidArgumentError If name already exists or format is not supported.
 * @throws RuntimeError If the file cannot be loaded.
 */
HSBA_SLICER_LIB_API std::shared_ptr<IModel> LoadModel(const std::string& name, std::string_view filePath);

/**
 * @brief Get a loaded model.
 *
 * Resolves against the caller's thread-local pool first, then against the copies
 * created by PublishModel.
 * @param name Model name.
 * @return Model shared pointer, nullptr if not found.
 */
HSBA_SLICER_LIB_API std::shared_ptr<IModel> GetModel(const std::string& name);

/**
 * @brief Expose a pooled model to by-name lookups from other threads.
 *
 * The model pool is thread local, and a pipeline coroutine starts its stages on executor
 * threads, so a model the caller registered under a name is otherwise invisible to the
 * pipeline that slices it. Publishing keeps a second, process-wide reference under the
 * same name that every lookup in this file falls back to, so the name resolves (and
 * transforms stay visible) on pipeline threads until it is unpublished or removed.
 * Concurrent publishes of one name overwrite: the last publisher's instance is shared.
 * @param name Model name.
 * @return True if a model with this name exists (locally or already published).
 */
HSBA_SLICER_LIB_API bool PublishModel(const std::string& name);

/**
 * @brief Drop the process-wide reference created by PublishModel.
 *
 * The thread-local pool entry of the publishing thread is untouched; after this call only
 * threads that still hold their own reference can see the model.
 * @param name Model name.
 * @return True if a published copy existed and was dropped.
 */
HSBA_SLICER_LIB_API bool UnpublishModel(const std::string& name);

/**
 * @brief Apply translation transform to a model.
 * @param name Model name.
 * @param translation Translation vector.
 */
HSBA_SLICER_LIB_API void TranslateModel(const std::string& name, const Eigen::Vector3f& translation);

/**
 * @brief Apply rotation transform to a model.
 * @param name Model name.
 * @param rotation Rotation quaternion.
 */
HSBA_SLICER_LIB_API void RotateModel(const std::string& name, const Eigen::Quaternionf& rotation);

/**
 * @brief Apply uniform scale transform to a model.
 * @param name Model name.
 * @param scale Scale factor (uniform scaling).
 */
HSBA_SLICER_LIB_API void ScaleModel(const std::string& name, float scale);

/**
 * @brief Apply non-uniform scale transform to a model.
 * @param name Model name.
 * @param scale Per-axis scale factors.
 */
HSBA_SLICER_LIB_API void ScaleModel(const std::string& name, const Eigen::Vector3f& scale);

/**
 * @brief Get model information (bounding box, volume).
 * @param name Model name.
 * @return Model information struct.
 */
HSBA_SLICER_LIB_API ModelInfo GetModelInfo(const std::string& name);

/**
 * @brief Export the triangle mesh of a model in igl style.
 *
 * Mesh models answer from their stored data; BRep models are tessellated on demand, so the
 * returned triangles are an approximation whose deflection follows the kernel's default.
 * @param name Model name.
 * @return Pair of vertex matrix (N x 3, millimetres) and face matrix (M x 3, zero-based indices).
 * @throws NotFoundError If no model exists under @p name.
 * @throws RuntimeError If the model cannot produce a triangle mesh.
 */
HSBA_SLICER_LIB_API std::pair<Eigen::MatrixXf, Eigen::MatrixXi> GetModelTriangles(const std::string& name);

/**
 * @brief Remove a model from the pool.
 *
 * Also drops the process-wide copy created by PublishModel, so after this call no thread
 * resolves the name anymore.
 * @param name Model name.
 */
HSBA_SLICER_LIB_API void RemoveModel(const std::string& name);

/**
 * @brief Insert a pre-constructed model into the pool.
 * @param name Unique model name.
 * @param model Shared pointer to the model.
 * @return The same shared pointer.
 * @throws InvalidArgumentError If name already exists.
 */
HSBA_SLICER_LIB_API std::shared_ptr<IModel> InsertModel(const std::string& name, std::shared_ptr<IModel> model);

/**
 * @brief Check whether a model with the given name exists.
 *
 * Covers both the caller's thread-local pool and the process-wide published copies.
 * @param name Model name.
 * @return true if the model exists.
 */
HSBA_SLICER_LIB_API bool ContainsModel(const std::string& name);

/**
 * @brief Return the number of models currently in the pool.
 */
HSBA_SLICER_LIB_API std::size_t ModelCount();

/**
 * @brief Return the names of all models in the pool.
 */
HSBA_SLICER_LIB_API std::vector<std::string> GetModelNames();

/**
 * @brief Clean up models that are no longer referenced externally.
 * @return Number of models removed.
 */
HSBA_SLICER_LIB_API std::size_t CleanupModels();

#ifdef USE_CGAL
// ----- Advanced model operations (require CGAL, uses IGL for mesh booleans) -----

/**
 * @brief Perform a thick-solid (shell) operation on an OcctModel.
 * @param sourceName Name of the source model (must be an OcctModel).
 * @param resultName Name to store the result under.
 * @param thickness Shell thickness (positive = inward).
 * @return Shared pointer to the resulting model.
 * @throws RuntimeError If the source is not an OcctModel or OCCT is unavailable.
 */
HSBA_SLICER_LIB_API std::shared_ptr<IModel> ThickSolidModel(const std::string& sourceName,
                                                            const std::string& resultName, float thickness);

/**
 * @brief Perform a thick-solid operation, excluding specified faces.
 * @param sourceName Name of the source model (must be an OcctModel).
 * @param resultName Name to store the result under.
 * @param closingFaces Faces to leave open (specified as vertex loops).
 * @param thickness Shell thickness.
 * @return Shared pointer to the resulting model.
 */
HSBA_SLICER_LIB_API std::shared_ptr<IModel>
ThickSolidModel(const std::string& sourceName, const std::string& resultName,
                const std::vector<std::vector<Eigen::Vector3f>>& closingFaces, float thickness);

/**
 * @brief Boolean union of two models.
 * @param leftName Left model name.
 * @param rightName Right model name.
 * @param resultName Name to store the result under.
 * @return Shared pointer to the resulting model.
 */
HSBA_SLICER_LIB_API std::shared_ptr<IModel> BooleanUnion(const std::string& leftName, const std::string& rightName,
                                                         const std::string& resultName);

/**
 * @brief Boolean intersection of two models.
 * @param leftName Left model name.
 * @param rightName Right model name.
 * @param resultName Name to store the result under.
 * @return Shared pointer to the resulting model.
 */
HSBA_SLICER_LIB_API std::shared_ptr<IModel>
BooleanIntersection(const std::string& leftName, const std::string& rightName, const std::string& resultName);

/**
 * @brief Boolean difference of two models (left - right).
 * @param leftName Left model name.
 * @param rightName Right model name.
 * @param resultName Name to store the result under.
 * @return Shared pointer to the resulting model.
 */
HSBA_SLICER_LIB_API std::shared_ptr<IModel> BooleanDifference(const std::string& leftName, const std::string& rightName,
                                                              const std::string& resultName);

/**
 * @brief Boolean XOR of two models.
 * @param leftName Left model name.
 * @param rightName Right model name.
 * @param resultName Name to store the result under.
 * @return Shared pointer to the resulting model.
 */
HSBA_SLICER_LIB_API std::shared_ptr<IModel> BooleanXor(const std::string& leftName, const std::string& rightName,
                                                       const std::string& resultName);
#endif  // USE_CGAL

/**
 * @brief Whether this build can evaluate text CSG scripts.
 *
 * Text CSG needs two things a consumer cannot tell apart from this header: the OCCT CAD
 * kernel (which gates the declaration below) and OpenCASCADE's DRAW Test Harness, reported
 * by the CAD target through a macro that stays inside LibHsBaSlicer. Asking the kernel here
 * instead keeps a caller from reading "BuildCsgModel is declared" as "a script will work".
 * @return True when BuildCsgModel can produce a model, false when it can only throw
 *         NotSupportedError.
 */
HSBA_SLICER_LIB_API bool HasTextCsgSupport();

#ifdef USE_OCCT
// ----- Text CSG model construction (requires the OCCT CAD kernel) -----

/**
 * @brief Build an OcctModel from an OCCT DRAW Test Harness script and store it in the pool.
 *
 * The script is text CSG in the DRAW command language: primitives (box, pcylinder,
 * pcone, psphere, ptorus), transforms (ttranslate, ...) and boolean operations
 * (bcut, bfuse, bcommon), one command per line. Requires OpenCASCADE built with its
 * Draw module (vcpkg feature opencascade[tcl]).
 * @param name Unique model name for the result.
 * @param script DRAW commands, lines separated by '\n'.
 * @param shapeName DRAW variable holding the result; empty selects "result".
 * @return Shared pointer to the resulting model.
 * @throws InvalidArgumentError If the name already exists or the script is malformed.
 * @throws NotFoundError If the script leaves no shape under @p shapeName.
 * @throws NotSupportedError If this build has no DRAW Test Harness.
 */
HSBA_SLICER_LIB_API std::shared_ptr<IModel> BuildCsgModel(const std::string& name, std::string_view script,
                                                          std::string_view shapeName = {});
#endif  // USE_OCCT

}  // namespace HsBa::Slicer

#endif  // !HSBA_SLICER_MODEL_PREPROCESS_HPP
