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

HSBA_SLICER_LIB_API std::shared_ptr<IModel> InsertModel(const std::string& name, std::shared_ptr<IModel> model)
{
    return GetLoader().InsertModel(name, std::move(model));
}

HSBA_SLICER_LIB_API bool ContainsModel(const std::string& name)
{
    return GetLoader().ContainsModel(name);
}

HSBA_SLICER_LIB_API std::size_t ModelCount()
{
    return GetLoader().ModelCount();
}

HSBA_SLICER_LIB_API std::vector<std::string> GetModelNames()
{
    return GetLoader().GetModelNames();
}

HSBA_SLICER_LIB_API std::size_t CleanupModels()
{
    return GetLoader().Cleanup();
}

#ifdef USE_CGAL

HSBA_SLICER_LIB_API std::shared_ptr<IModel> ThickSolidModel(const std::string& sourceName,
                                                            const std::string& resultName, float thickness)
{
    return GetLoader().ThickSolidModel(sourceName, resultName, thickness);
}

HSBA_SLICER_LIB_API std::shared_ptr<IModel>
ThickSolidModel(const std::string& sourceName, const std::string& resultName,
                const std::vector<std::vector<Eigen::Vector3f>>& closingFaces, float thickness)
{
    return GetLoader().ThickSolidModel(sourceName, resultName, closingFaces, thickness);
}

HSBA_SLICER_LIB_API std::shared_ptr<IModel> BooleanUnion(const std::string& leftName, const std::string& rightName,
                                                         const std::string& resultName)
{
    return GetLoader().BooleanUnion(leftName, rightName, resultName);
}

HSBA_SLICER_LIB_API std::shared_ptr<IModel>
BooleanIntersection(const std::string& leftName, const std::string& rightName, const std::string& resultName)
{
    return GetLoader().BooleanIntersection(leftName, rightName, resultName);
}

HSBA_SLICER_LIB_API std::shared_ptr<IModel> BooleanDifference(const std::string& leftName, const std::string& rightName,
                                                              const std::string& resultName)
{
    return GetLoader().BooleanDifference(leftName, rightName, resultName);
}

HSBA_SLICER_LIB_API std::shared_ptr<IModel> BooleanXor(const std::string& leftName, const std::string& rightName,
                                                       const std::string& resultName)
{
    return GetLoader().BooleanXor(leftName, rightName, resultName);
}

#endif  // USE_CGAL

}  // namespace HsBa::Slicer
