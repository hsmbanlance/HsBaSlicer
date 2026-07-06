#include "model_preprocess.hpp"

#include "preprocess/ModelLoader.hpp"

namespace HsBa::Slicer
{
namespace
{
// 全局ModelLoader实例（线程局部存储）
ModelLoader& GetLoader()
{
    thread_local ModelLoader loader;
    return loader;
}
}  // namespace

HSBA_SLICER_LIB_API std::shared_ptr<IModel> LoadModel(const std::string& name, std::string_view filePath)
{
    return GetLoader().LoadModel(name, filePath);
}

HSBA_SLICER_LIB_API std::shared_ptr<IModel> GetModel(const std::string& name)
{
    return GetLoader().GetModel(name);
}

HSBA_SLICER_LIB_API void TranslateModel(const std::string& name, const Eigen::Vector3f& translation)
{
    auto model = GetLoader().GetModel(name);
    if (model)
    {
        model->Translate(translation);
    }
}

HSBA_SLICER_LIB_API void RotateModel(const std::string& name, const Eigen::Quaternionf& rotation)
{
    auto model = GetLoader().GetModel(name);
    if (model)
    {
        model->Rotate(rotation);
    }
}

HSBA_SLICER_LIB_API void ScaleModel(const std::string& name, float scale)
{
    auto model = GetLoader().GetModel(name);
    if (model)
    {
        model->Scale(scale);
    }
}

HSBA_SLICER_LIB_API void ScaleModel(const std::string& name, const Eigen::Vector3f& scale)
{
    auto model = GetLoader().GetModel(name);
    if (model)
    {
        model->Scale(scale);
    }
}

HSBA_SLICER_LIB_API ModelInfo GetModelInfo(const std::string& name)
{
    ModelInfo info;
    auto model = GetLoader().GetModel(name);
    if (model)
    {
        model->BoundingBox(info.bbox_min, info.bbox_max);
        info.volume = model->Volume();
    }
    return info;
}

HSBA_SLICER_LIB_API void RemoveModel(const std::string& name)
{
    GetLoader().RemoveModel(name);
}

}  // namespace HsBa::Slicer
