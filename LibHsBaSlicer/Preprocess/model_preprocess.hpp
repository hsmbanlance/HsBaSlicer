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
 * @brief 模型信息结构体
 */
struct ModelInfo
{
    Eigen::Vector3f bbox_min;  ///< 包围盒最小角
    Eigen::Vector3f bbox_max;  ///< 包围盒最大角
    float volume = 0.0f;       ///< 模型体积
};

/**
 * @brief 加载模型并存储到内部池中。
 * @param name 模型唯一名称。
 * @param filePath 模型文件路径（支持STL/OBJ/STEP/IGES等）。
 * @return 加载后的模型共享指针。
 * @throws InvalidArgumentError 名称已存在或格式不支持。
 * @throws RuntimeError 文件无法加载。
 */
HSBA_SLICER_LIB_API std::shared_ptr<IModel> LoadModel(const std::string& name, std::string_view filePath);

/**
 * @brief 获取已加载的模型。
 * @param name 模型名称。
 * @return 模型共享指针，未找到返回nullptr。
 */
HSBA_SLICER_LIB_API std::shared_ptr<IModel> GetModel(const std::string& name);

/**
 * @brief 对模型施加平移变换。
 * @param name 模型名称。
 * @param translation 平移向量。
 */
HSBA_SLICER_LIB_API void TranslateModel(const std::string& name, const Eigen::Vector3f& translation);

/**
 * @brief 对模型施加旋转变换。
 * @param name 模型名称。
 * @param rotation 旋转四元数。
 */
HSBA_SLICER_LIB_API void RotateModel(const std::string& name, const Eigen::Quaternionf& rotation);

/**
 * @brief 对模型施加缩放变换。
 * @param name 模型名称。
 * @param scale 缩放因子（均匀缩放）。
 */
HSBA_SLICER_LIB_API void ScaleModel(const std::string& name, float scale);

/**
 * @brief 对模型施加缩放变换（非均匀）。
 * @param name 模型名称。
 * @param scale 各轴缩放因子。
 */
HSBA_SLICER_LIB_API void ScaleModel(const std::string& name, const Eigen::Vector3f& scale);

/**
 * @brief 获取模型信息（包围盒、体积）。
 * @param name 模型名称。
 * @return 模型信息结构体。
 */
HSBA_SLICER_LIB_API ModelInfo GetModelInfo(const std::string& name);

/**
 * @brief 从池中移除模型。
 * @param name 模型名称。
 */
HSBA_SLICER_LIB_API void RemoveModel(const std::string& name);

}  // namespace HsBa::Slicer

#endif  // !HSBA_SLICER_MODEL_PREPROCESS_HPP
