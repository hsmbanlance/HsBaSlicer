#pragma once
#ifndef HSBA_SLICER_MODEL_PREPROCESS_HPP
#define HSBA_SLICER_MODEL_PREPROCESS_HPP

#include <memory>
#include <string>
#include <string_view>

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
 * @param name Model name.
 * @return Model shared pointer, nullptr if not found.
 */
HSBA_SLICER_LIB_API std::shared_ptr<IModel> GetModel(const std::string& name);

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
 * @brief Remove a model from the pool.
 * @param name Model name.
 */
HSBA_SLICER_LIB_API void RemoveModel(const std::string& name);

}  // namespace HsBa::Slicer

#endif  // !HSBA_SLICER_MODEL_PREPROCESS_HPP
