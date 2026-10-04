#include "model_preprocess.hpp"

#include <exception>
#include <string>

#include "base/error.hpp"
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

// Translation boundary for the model/geometry subsystem.
//
// The functions below reach third-party libraries (OpenCASCADE, CGAL,
// OpenVDB) whose failure modes are reported as exception types outside the
// project's hierarchy (e.g. Standard_Failure, CGAL::Exception) or as plain
// std::exception. Such types must never propagate across the extern "C" Dll
// boundary, so every exported model operation is funnelled through this
// wrapper: project exceptions (already RuntimeError-derived) pass through
// unchanged, while any foreign exception is converted into the project's
// IOError (a RuntimeError) preserving the original message. Callers -
// including the C-ABI layer - therefore only ever observe RuntimeError-family
// types and can narrow their own handlers accordingly.
template <typename Fn>
auto GuardModel(Fn&& fn) -> decltype(fn())
{
    try
    {
        return fn();
    }
    catch (const RuntimeError&)
    {
        throw;
    }
    catch (const std::exception& e)
    {
        throw IOError(std::string("Model operation failed: ") + e.what());
    }
}
}  // namespace

HSBA_SLICER_LIB_API std::shared_ptr<IModel> LoadModel(const std::string& name, std::string_view filePath)
{
    return GuardModel([&] { return GetLoader().LoadModel(name, filePath); });
}

HSBA_SLICER_LIB_API std::shared_ptr<IModel> GetModel(const std::string& name)
{
    return GuardModel([&] { return GetLoader().GetModel(name); });
}

HSBA_SLICER_LIB_API void TranslateModel(const std::string& name, const Eigen::Vector3f& translation)
{
    GuardModel([&] {
        auto model = GetLoader().GetModel(name);
        if (model)
        {
            model->Translate(translation);
        }
    });
}

HSBA_SLICER_LIB_API void RotateModel(const std::string& name, const Eigen::Quaternionf& rotation)
{
    GuardModel([&] {
        auto model = GetLoader().GetModel(name);
        if (model)
        {
            model->Rotate(rotation);
        }
    });
}

HSBA_SLICER_LIB_API void ScaleModel(const std::string& name, float scale)
{
    GuardModel([&] {
        auto model = GetLoader().GetModel(name);
        if (model)
        {
            model->Scale(scale);
        }
    });
}

HSBA_SLICER_LIB_API void ScaleModel(const std::string& name, const Eigen::Vector3f& scale)
{
    GuardModel([&] {
        auto model = GetLoader().GetModel(name);
        if (model)
        {
            model->Scale(scale);
        }
    });
}

HSBA_SLICER_LIB_API ModelInfo GetModelInfo(const std::string& name)
{
    return GuardModel([&] {
        ModelInfo info;
        auto model = GetLoader().GetModel(name);
        if (model)
        {
            model->BoundingBox(info.bbox_min, info.bbox_max);
            info.volume = model->Volume();
        }
        return info;
    });
}

HSBA_SLICER_LIB_API void RemoveModel(const std::string& name)
{
    GuardModel([&] { GetLoader().RemoveModel(name); });
}

HSBA_SLICER_LIB_API std::shared_ptr<IModel> InsertModel(const std::string& name, std::shared_ptr<IModel> model)
{
    return GuardModel([&] { return GetLoader().InsertModel(name, std::move(model)); });
}

HSBA_SLICER_LIB_API bool ContainsModel(const std::string& name)
{
    return GuardModel([&] { return GetLoader().ContainsModel(name); });
}

HSBA_SLICER_LIB_API std::size_t ModelCount()
{
    return GuardModel([&] { return GetLoader().ModelCount(); });
}

HSBA_SLICER_LIB_API std::vector<std::string> GetModelNames()
{
    return GuardModel([&] { return GetLoader().GetModelNames(); });
}

HSBA_SLICER_LIB_API std::size_t CleanupModels()
{
    return GuardModel([&] { return GetLoader().Cleanup(); });
}

#ifdef USE_CGAL

HSBA_SLICER_LIB_API std::shared_ptr<IModel> ThickSolidModel(const std::string& sourceName,
                                                            const std::string& resultName, float thickness)
{
    return GuardModel([&] { return GetLoader().ThickSolidModel(sourceName, resultName, thickness); });
}

HSBA_SLICER_LIB_API std::shared_ptr<IModel>
ThickSolidModel(const std::string& sourceName, const std::string& resultName,
                 const std::vector<std::vector<Eigen::Vector3f>>& closingFaces, float thickness)
{
    return GuardModel([&] { return GetLoader().ThickSolidModel(sourceName, resultName, closingFaces, thickness); });
}

HSBA_SLICER_LIB_API std::shared_ptr<IModel> BooleanUnion(const std::string& leftName, const std::string& rightName,
                                                         const std::string& resultName)
{
    return GuardModel([&] { return GetLoader().BooleanUnion(leftName, rightName, resultName); });
}

HSBA_SLICER_LIB_API std::shared_ptr<IModel>
BooleanIntersection(const std::string& leftName, const std::string& rightName, const std::string& resultName)
{
    return GuardModel([&] { return GetLoader().BooleanIntersection(leftName, rightName, resultName); });
}

HSBA_SLICER_LIB_API std::shared_ptr<IModel> BooleanDifference(const std::string& leftName, const std::string& rightName,
                                                              const std::string& resultName)
{
    return GuardModel([&] { return GetLoader().BooleanDifference(leftName, rightName, resultName); });
}

HSBA_SLICER_LIB_API std::shared_ptr<IModel> BooleanXor(const std::string& leftName, const std::string& rightName,
                                                       const std::string& resultName)
{
    return GuardModel([&] { return GetLoader().BooleanXor(leftName, rightName, resultName); });
}

#endif  // USE_CGAL

}  // namespace HsBa::Slicer
