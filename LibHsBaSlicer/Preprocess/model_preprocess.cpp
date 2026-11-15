/** @file model_preprocess.cpp
 * @brief Implementation of model loading, transform, query and boolean/thick-solid preprocessing.
 * @author HsBa
 */
#include "model_preprocess.hpp"

#include <exception>
#include <format>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

#include "base/error.hpp"
#ifdef USE_OCCT
#include "cadmodel/OcctCsg.hpp"
#include "cadmodel/OcctModel.hpp"
#endif
#include "preprocess/ModelLoader.hpp"

namespace HsBa::Slicer
{
namespace
{
// Global ModelLoader instance (thread-local storage)
ModelLoader& GetLoader()
{
    thread_local ModelLoader loader;
    return loader;
}

// Process-wide second tier for published models.
//
// The pool above is thread local on purpose (jobs stay isolated), but a pipeline
// coroutine starts its stages on executor threads, so a caller-pooled model would
// never be visible there. PublishModel copies the shared_ptr into this map and every
// by-name lookup below falls back to it, which bridges exactly the names the caller
// chose to share without dissolving the thread isolation of ordinary pool entries.
struct SharedTier
{
    std::mutex mutex;
    std::unordered_map<std::string, std::shared_ptr<IModel>> models;
};

SharedTier& Tier()
{
    static SharedTier tier;
    return tier;
}

std::shared_ptr<IModel> FindShared(const std::string& name)
{
    const std::lock_guard<std::mutex> lock(Tier().mutex);
    const auto it = Tier().models.find(name);
    return it != Tier().models.end() ? it->second : nullptr;
}

// The single lookup every exported operation below resolves names through: local pool
// first, published copies second, so both tiers behave like one name space per caller.
std::shared_ptr<IModel> FindModel(const std::string& name)
{
    if (auto model = GetLoader().GetModel(name))
    {
        return model;
    }
    return FindShared(name);
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
        return std::forward<Fn>(fn)();
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
    return GuardModel(
        [&]
        {
            // A published name is owned by another context; loading a second instance
            // under it would silently split the geometry behind one name.
            if (FindShared(name))
            {
                throw AlreadyExistsError(std::format("Model '{}' is published across threads", name));
            }
            return GetLoader().LoadModel(name, filePath);
        });
}

HSBA_SLICER_LIB_API std::shared_ptr<IModel> GetModel(const std::string& name)
{
    return GuardModel([&] { return FindModel(name); });
}

HSBA_SLICER_LIB_API bool PublishModel(const std::string& name)
{
    return GuardModel(
        [&]
        {
            auto model = FindModel(name);
            if (!model)
            {
                return false;
            }
            const std::lock_guard<std::mutex> lock(Tier().mutex);
            Tier().models.insert_or_assign(name, std::move(model));
            return true;
        });
}

HSBA_SLICER_LIB_API bool UnpublishModel(const std::string& name)
{
    return GuardModel(
        [&]
        {
            const std::lock_guard<std::mutex> lock(Tier().mutex);
            return Tier().models.erase(name) > 0;
        });
}

HSBA_SLICER_LIB_API void TranslateModel(const std::string& name, const Eigen::Vector3f& translation)
{
    GuardModel(
        [&]
        {
            auto model = FindModel(name);
            if (model)
            {
                model->Translate(translation);
            }
        });
}

HSBA_SLICER_LIB_API void RotateModel(const std::string& name, const Eigen::Quaternionf& rotation)
{
    GuardModel(
        [&]
        {
            auto model = FindModel(name);
            if (model)
            {
                model->Rotate(rotation);
            }
        });
}

HSBA_SLICER_LIB_API void ScaleModel(const std::string& name, float scale)
{
    GuardModel(
        [&]
        {
            auto model = FindModel(name);
            if (model)
            {
                model->Scale(scale);
            }
        });
}

HSBA_SLICER_LIB_API void ScaleModel(const std::string& name, const Eigen::Vector3f& scale)
{
    GuardModel(
        [&]
        {
            auto model = FindModel(name);
            if (model)
            {
                model->Scale(scale);
            }
        });
}

HSBA_SLICER_LIB_API ModelInfo GetModelInfo(const std::string& name)
{
    return GuardModel(
        [&]
        {
            ModelInfo info;
            auto model = FindModel(name);
            if (model)
            {
                model->BoundingBox(info.bbox_min, info.bbox_max);
                info.volume = model->Volume();
            }
            return info;
        });
}

HSBA_SLICER_LIB_API std::pair<Eigen::MatrixXf, Eigen::MatrixXi> GetModelTriangles(const std::string& name)
{
    return GuardModel(
        [&]
        {
            auto model = FindModel(name);
            if (!model)
            {
                throw NotFoundError(std::format("Model '{}' not found", name));
            }
            return model->TriangleMesh();
        });
}

HSBA_SLICER_LIB_API void RemoveModel(const std::string& name)
{
    GuardModel(
        [&]
        {
            GetLoader().RemoveModel(name);
            // Removing is the one operation that clears both tiers, so a caller that
            // published by accident cannot leave the name resolving on other threads.
            const std::lock_guard<std::mutex> lock(Tier().mutex);
            Tier().models.erase(name);
        });
}

HSBA_SLICER_LIB_API std::shared_ptr<IModel> InsertModel(const std::string& name, std::shared_ptr<IModel> model)
{
    return GuardModel(
        [&]
        {
            if (FindShared(name))
            {
                throw AlreadyExistsError(std::format("Model '{}' is published across threads", name));
            }
            return GetLoader().InsertModel(name, std::move(model));
        });
}

HSBA_SLICER_LIB_API bool ContainsModel(const std::string& name)
{
    return GuardModel([&] { return GetLoader().ContainsModel(name) || FindShared(name) != nullptr; });
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

HSBA_SLICER_LIB_API bool HasTextCsgSupport()
{
#ifdef USE_OCCT
    // The DRAW macro belongs to the CAD target, whose interface this library consumes
    // privately; reading it here is what lets a caller outside ask instead of guess.
    return HasCsgSupport();
#else
    return false;
#endif
}

#ifdef USE_OCCT

HSBA_SLICER_LIB_API std::shared_ptr<IModel> BuildCsgModel(const std::string& name, std::string_view script,
                                                          std::string_view shapeName)
{
    return GuardModel(
        [&]
        {
            // Through the exported InsertModel so a name already published from another
            // context is refused instead of silently shadowed by the local pool.
            return InsertModel(
                name, std::make_shared<OcctModel>(OcctModel::CreateFromCsg(script, shapeName)));
        });
}

#endif  // USE_OCCT

}  // namespace HsBa::Slicer
