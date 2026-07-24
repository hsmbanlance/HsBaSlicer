#include "model_preprocess.h"

#include <exception>
#include <memory>
#include <string>

#include "LibHsBaSlicer/Preprocess/model_preprocess.hpp"

// Helper: convert shared_ptr<IModel> to opaque handle (adds a reference)
static void* ToHandle(std::shared_ptr<HsBa::Slicer::IModel> model)
{
    if (!model)
        return nullptr;
    // Heap-allocate a shared_ptr to keep the model alive via handle
    auto* ptr = new std::shared_ptr<HsBa::Slicer::IModel>(std::move(model));
    return static_cast<void*>(ptr);
}

// ========== Basic Operations ==========

HSBA_SLICER_API void* HsBaLoadModel(const char* name, const char* file_path)
{
    if (!name || !file_path)
        return nullptr;
    try
    {
        auto model = HsBa::Slicer::LoadModel(name, file_path);
        return ToHandle(std::move(model));
    }
    catch (const std::exception&)
    {
        return nullptr;
    }
}

HSBA_SLICER_API void* HsBaGetModel(const char* name)
{
    if (!name)
        return nullptr;
    try
    {
        auto model = HsBa::Slicer::GetModel(name);
        return ToHandle(std::move(model));
    }
    catch (const std::exception&)
    {
        return nullptr;
    }
}

HSBA_SLICER_API void HsBaRemoveModel(const char* name)
{
    if (!name)
        return;
    try
    {
        HsBa::Slicer::RemoveModel(name);
    }
    catch (const std::exception&)
    {
    }
}

HSBA_SLICER_API int HsBaContainsModel(const char* name)
{
    if (!name)
        return 0;
    try
    {
        return HsBa::Slicer::ContainsModel(name) ? 1 : 0;
    }
    catch (const std::exception&)
    {
        return 0;
    }
}

HSBA_SLICER_API int HsBaModelCount(void)
{
    try
    {
        return static_cast<int>(HsBa::Slicer::ModelCount());
    }
    catch (const std::exception&)
    {
        return 0;
    }
}

HSBA_SLICER_API int HsBaCleanupModels(void)
{
    try
    {
        return static_cast<int>(HsBa::Slicer::CleanupModels());
    }
    catch (const std::exception&)
    {
        return 0;
    }
}

// ========== Transform Operations ==========

HSBA_SLICER_API int HsBaTranslateModel(const char* name, float tx, float ty, float tz)
{
    if (!name)
        return 0;
    try
    {
        HsBa::Slicer::TranslateModel(name, Eigen::Vector3f(tx, ty, tz));
        return 1;
    }
    catch (const std::exception&)
    {
        return 0;
    }
}

HSBA_SLICER_API int HsBaRotateModel(const char* name, float qx, float qy, float qz, float qw)
{
    if (!name)
        return 0;
    try
    {
        HsBa::Slicer::RotateModel(name, Eigen::Quaternionf(qw, qx, qy, qz));
        return 1;
    }
    catch (const std::exception&)
    {
        return 0;
    }
}

HSBA_SLICER_API int HsBaScaleModelUniform(const char* name, float scale)
{
    if (!name)
        return 0;
    try
    {
        HsBa::Slicer::ScaleModel(name, scale);
        return 1;
    }
    catch (const std::exception&)
    {
        return 0;
    }
}

HSBA_SLICER_API int HsBaScaleModel(const char* name, float sx, float sy, float sz)
{
    if (!name)
        return 0;
    try
    {
        HsBa::Slicer::ScaleModel(name, Eigen::Vector3f(sx, sy, sz));
        return 1;
    }
    catch (const std::exception&)
    {
        return 0;
    }
}

// ========== Query Operations ==========

HSBA_SLICER_API int HsBaGetModelInfo(const char* name, float out_bbox_min[3], float out_bbox_max[3],
                                      float* out_volume)
{
    if (!name)
        return 0;
    try
    {
        auto info = HsBa::Slicer::GetModelInfo(name);
        if (out_bbox_min)
        {
            out_bbox_min[0] = info.bbox_min.x();
            out_bbox_min[1] = info.bbox_min.y();
            out_bbox_min[2] = info.bbox_min.z();
        }
        if (out_bbox_max)
        {
            out_bbox_max[0] = info.bbox_max.x();
            out_bbox_max[1] = info.bbox_max.y();
            out_bbox_max[2] = info.bbox_max.z();
        }
        if (out_volume)
        {
            *out_volume = info.volume;
        }
        return 1;
    }
    catch (const std::exception&)
    {
        return 0;
    }
}

// ========== Advanced Operations (CGAL/OCCT) ==========

HSBA_SLICER_API void* HsBaThickSolidModel(const char* source_name, const char* result_name, float thickness)
{
    if (!source_name || !result_name)
        return nullptr;
#ifdef USE_CGAL
    try
    {
        auto model = HsBa::Slicer::ThickSolidModel(source_name, result_name, thickness);
        return ToHandle(std::move(model));
    }
    catch (const std::exception&)
    {
        return nullptr;
    }
#else
    (void)thickness;
    return nullptr;
#endif
}

HSBA_SLICER_API void* HsBaBooleanUnion(const char* left_name, const char* right_name, const char* result_name)
{
    if (!left_name || !right_name || !result_name)
        return nullptr;
#ifdef USE_CGAL
    try
    {
        auto model = HsBa::Slicer::BooleanUnion(left_name, right_name, result_name);
        return ToHandle(std::move(model));
    }
    catch (const std::exception&)
    {
        return nullptr;
    }
#else
    return nullptr;
#endif
}

HSBA_SLICER_API void* HsBaBooleanIntersection(const char* left_name, const char* right_name, const char* result_name)
{
    if (!left_name || !right_name || !result_name)
        return nullptr;
#ifdef USE_CGAL
    try
    {
        auto model = HsBa::Slicer::BooleanIntersection(left_name, right_name, result_name);
        return ToHandle(std::move(model));
    }
    catch (const std::exception&)
    {
        return nullptr;
    }
#else
    return nullptr;
#endif
}

HSBA_SLICER_API void* HsBaBooleanDifference(const char* left_name, const char* right_name, const char* result_name)
{
    if (!left_name || !right_name || !result_name)
        return nullptr;
#ifdef USE_CGAL
    try
    {
        auto model = HsBa::Slicer::BooleanDifference(left_name, right_name, result_name);
        return ToHandle(std::move(model));
    }
    catch (const std::exception&)
    {
        return nullptr;
    }
#else
    return nullptr;
#endif
}

HSBA_SLICER_API void* HsBaBooleanXor(const char* left_name, const char* right_name, const char* result_name)
{
    if (!left_name || !right_name || !result_name)
        return nullptr;
#ifdef USE_CGAL
    try
    {
        auto model = HsBa::Slicer::BooleanXor(left_name, right_name, result_name);
        return ToHandle(std::move(model));
    }
    catch (const std::exception&)
    {
        return nullptr;
    }
#else
    return nullptr;
#endif
}

HSBA_SLICER_API void HsBaReleaseModelHandle(void* handle)
{
    if (!handle)
        return;
    auto* ptr = static_cast<std::shared_ptr<HsBa::Slicer::IModel>*>(handle);
    delete ptr;
}
