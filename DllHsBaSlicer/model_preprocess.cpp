/** @file model_preprocess.cpp
 * @brief Implementation of model loading, transform, query, boolean/thick-solid and text CSG C ABI.
 * @author HsBa
 */
#include "model_preprocess.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <memory>
#include <string>
#include <string_view>

#include "LibHsBaSlicer/Preprocess/model_preprocess.hpp"
#include "base/error.hpp"

// Helper: convert shared_ptr<IModel> to opaque handle (adds a reference)
namespace
{
void* ToHandle(std::shared_ptr<HsBa::Slicer::IModel> model)
{
    if (!model)
    {
        return nullptr;
    }
    // Heap-allocate a shared_ptr to keep the model alive via handle
    auto* ptr = new std::shared_ptr<HsBa::Slicer::IModel>(std::move(model));
    return static_cast<void*>(ptr);
}

// Hand a reason over the C boundary as text, the convention HsBaJobGetProgress uses for its
// stage string: the buffer belongs to the caller, so nothing here can leak or be double-freed.
// Passing NULL for out is how a caller says it does not care about the reason.
void WriteReason(char* out, int cap, std::string_view text)
{
    if (!out || cap <= 0)
    {
        return;
    }
    const auto length = std::min<std::size_t>(text.size(), static_cast<std::size_t>(cap - 1));
    if (length > 0)
    {
        std::memcpy(out, text.data(), length);
    }
    out[length] = '\0';
}

// One triangle export into the caller's struct, laid out like the geometry_export contract:
// a single malloc block, vertices first so that freeing starts at xyz. Both element types
// are 4 bytes wide, so the index section stays naturally aligned behind the vertex section.
void FillTriangleBlock(const Eigen::MatrixXf& vertices, const Eigen::MatrixXi& faces, HsBaModelTriangles_t* out)
{
    const int vertex_count = static_cast<int>(vertices.rows());
    const int face_count = static_cast<int>(faces.rows());
    const std::size_t vertex_bytes = 3 * sizeof(float) * static_cast<std::size_t>(vertex_count);
    const std::size_t face_bytes = 3 * sizeof(int) * static_cast<std::size_t>(face_count);
    // The block is owned by the struct, so even an empty mesh must hand back a valid address.
    auto* block = static_cast<char*>(std::malloc(vertex_bytes + face_bytes == 0 ? 1 : vertex_bytes + face_bytes));
    if (!block)
    {
        return;
    }
    auto* xyz = reinterpret_cast<float*>(block);
    for (int i = 0; i < vertex_count; ++i)
    {
        xyz[3 * i + 0] = vertices(i, 0);
        xyz[3 * i + 1] = vertices(i, 1);
        xyz[3 * i + 2] = vertices(i, 2);
    }
    auto* indices = reinterpret_cast<int*>(block + vertex_bytes);
    for (int i = 0; i < face_count; ++i)
    {
        indices[3 * i + 0] = faces(i, 0);
        indices[3 * i + 1] = faces(i, 1);
        indices[3 * i + 2] = faces(i, 2);
    }
    out->vertex_count = vertex_count;
    out->xyz = xyz;
    out->face_count = face_count;
    // Empty section reports the block start, not NULL; only valid while face_count is 0.
    out->indices = face_count > 0 ? indices : reinterpret_cast<const int*>(xyz);
}
}  // namespace

// ========== Basic Operations ==========

HSBA_SLICER_API void* HsBaLoadModel(const char* name, const char* file_path)
{
    if (!name || !file_path)
    {
        return nullptr;
    }
    try
    {
        auto model = HsBa::Slicer::LoadModel(name, file_path);
        return ToHandle(std::move(model));
    }
    catch (const HsBa::Slicer::RuntimeError&)
    {
        return nullptr;
    }
}

HSBA_SLICER_API void* HsBaGetModel(const char* name)
{
    if (!name)
    {
        return nullptr;
    }
    try
    {
        auto model = HsBa::Slicer::GetModel(name);
        return ToHandle(std::move(model));
    }
    catch (const HsBa::Slicer::RuntimeError&)
    {
        return nullptr;
    }
}

HSBA_SLICER_API void HsBaRemoveModel(const char* name)
{
    if (!name)
    {
        return;
    }
    try
    {
        HsBa::Slicer::RemoveModel(name);
    }
    // NOLINTNEXTLINE(bugprone-empty-catch) C ABI: swallow RuntimeError, do not propagate across C boundary
    catch (const HsBa::Slicer::RuntimeError&)
    {
    }
}

HSBA_SLICER_API int HsBaContainsModel(const char* name)
{
    if (!name)
    {
        return 0;
    }
    try
    {
        return HsBa::Slicer::ContainsModel(name) ? 1 : 0;
    }
    catch (const HsBa::Slicer::RuntimeError&)
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
    catch (const HsBa::Slicer::RuntimeError&)
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
    catch (const HsBa::Slicer::RuntimeError&)
    {
        return 0;
    }
}

// ========== Transform Operations ==========

HSBA_SLICER_API int HsBaTranslateModel(const char* name, float tx, float ty, float tz)
{
    if (!name)
    {
        return 0;
    }
    try
    {
        HsBa::Slicer::TranslateModel(name, Eigen::Vector3f(tx, ty, tz));
        return 1;
    }
    catch (const HsBa::Slicer::RuntimeError&)
    {
        return 0;
    }
}

HSBA_SLICER_API int HsBaRotateModel(const char* name, float qx, float qy, float qz, float qw)
{
    if (!name)
    {
        return 0;
    }
    try
    {
        HsBa::Slicer::RotateModel(name, Eigen::Quaternionf(qw, qx, qy, qz));
        return 1;
    }
    catch (const HsBa::Slicer::RuntimeError&)
    {
        return 0;
    }
}

HSBA_SLICER_API int HsBaScaleModelUniform(const char* name, float scale)
{
    if (!name)
    {
        return 0;
    }
    try
    {
        HsBa::Slicer::ScaleModel(name, scale);
        return 1;
    }
    catch (const HsBa::Slicer::RuntimeError&)
    {
        return 0;
    }
}

HSBA_SLICER_API int HsBaScaleModel(const char* name, float sx, float sy, float sz)
{
    if (!name)
    {
        return 0;
    }
    try
    {
        HsBa::Slicer::ScaleModel(name, Eigen::Vector3f(sx, sy, sz));
        return 1;
    }
    catch (const HsBa::Slicer::RuntimeError&)
    {
        return 0;
    }
}

// ========== Query Operations ==========

HSBA_SLICER_API int HsBaGetModelInfo(const char* name, float out_bbox_min[3], float out_bbox_max[3], float* out_volume)
{
    if (!name)
    {
        return 0;
    }
    try
    {
        // The Lib query answers an unknown name with a zeroed struct (pipeline callers rely on
        // that), but this boundary documents 0 for "model not found" - so check the name first.
        if (!HsBa::Slicer::ContainsModel(name))
        {
            return 0;
        }
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
    catch (const HsBa::Slicer::RuntimeError&)
    {
        return 0;
    }
}

HSBA_SLICER_API int HsBaGetModelTriangles(const char* name, HsBaModelTriangles_t* out)
{
    if (!name || !out)
    {
        return 0;
    }
    std::memset(out, 0, sizeof(HsBaModelTriangles_t));
    try
    {
        // GetModelTriangles reports an unknown name as NotFoundError, which the handler below
        // turns into the documented 0; foreign kernel failures arrive as IOError via GuardModel.
        const auto [vertices, faces] = HsBa::Slicer::GetModelTriangles(name);
        FillTriangleBlock(vertices, faces, out);
        return out->xyz != nullptr ? 1 : 0; // the only failure left is the malloc inside the block
    }
    catch (const HsBa::Slicer::RuntimeError&)
    {
        return 0;
    }
}

HSBA_SLICER_API void HsBaFreeModelTriangles(HsBaModelTriangles_t* tris)
{
    if (!tris)
    {
        return;
    }
    // The block starts at xyz, so freeing it releases the index array and both counts with it.
    std::free(const_cast<float*>(tris->xyz));
    std::memset(tris, 0, sizeof(HsBaModelTriangles_t));
}

// ========== Advanced Operations (CGAL/OCCT) ==========

HSBA_SLICER_API void* HsBaThickSolidModel(const char* source_name, const char* result_name, float thickness)
{
    if (!source_name || !result_name)
    {
        return nullptr;
    }
#ifdef USE_CGAL
    try
    {
        auto model = HsBa::Slicer::ThickSolidModel(source_name, result_name, thickness);
        return ToHandle(std::move(model));
    }
    catch (const HsBa::Slicer::RuntimeError&)
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
    {
        return nullptr;
    }
#ifdef USE_CGAL
    try
    {
        auto model = HsBa::Slicer::BooleanUnion(left_name, right_name, result_name);
        return ToHandle(std::move(model));
    }
    catch (const HsBa::Slicer::RuntimeError&)
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
    {
        return nullptr;
    }
#ifdef USE_CGAL
    try
    {
        auto model = HsBa::Slicer::BooleanIntersection(left_name, right_name, result_name);
        return ToHandle(std::move(model));
    }
    catch (const HsBa::Slicer::RuntimeError&)
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
    {
        return nullptr;
    }
#ifdef USE_CGAL
    try
    {
        auto model = HsBa::Slicer::BooleanDifference(left_name, right_name, result_name);
        return ToHandle(std::move(model));
    }
    catch (const HsBa::Slicer::RuntimeError&)
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
    {
        return nullptr;
    }
#ifdef USE_CGAL
    try
    {
        auto model = HsBa::Slicer::BooleanXor(left_name, right_name, result_name);
        return ToHandle(std::move(model));
    }
    catch (const HsBa::Slicer::RuntimeError&)
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
    {
        return;
    }
    auto* ptr = static_cast<std::shared_ptr<HsBa::Slicer::IModel>*>(handle);
    delete ptr;
}

HSBA_SLICER_API int HsBaPublishModel(const char* name)
{
    if (!name)
    {
        return 0;
    }
    try
    {
        return HsBa::Slicer::PublishModel(name) ? 1 : 0;
    }
    catch (const HsBa::Slicer::RuntimeError&)
    {
        return 0;
    }
}

HSBA_SLICER_API int HsBaUnpublishModel(const char* name)
{
    if (!name)
    {
        return 0;
    }
    try
    {
        return HsBa::Slicer::UnpublishModel(name) ? 1 : 0;
    }
    catch (const HsBa::Slicer::RuntimeError&)
    {
        return 0;
    }
}

// ========== Text CSG (OCCT DRAW Test Harness) ==========

HSBA_SLICER_API int HsBaTextCsgAvailable(void)
{
    // A query about the build itself: nothing here can fail, so no handler is wrapped around it.
    return HsBa::Slicer::HasTextCsgSupport() ? 1 : 0;
}

HSBA_SLICER_API void* HsBaBuildCsgModel(const char* name, const char* script, const char* shape_name,
                                       char* error_out, int error_cap)
{
    // Success leaves an empty reason, so the caller may print the buffer either way.
    WriteReason(error_out, error_cap, {});
    if (!name || !script)
    {
        WriteReason(error_out, error_cap, "HsBaBuildCsgModel: model name and script are both required");
        return nullptr;
    }
#ifdef USE_OCCT
    try
    {
        // BuildCsgModel funnels every foreign failure into the RuntimeError family, which is what
        // this boundary is allowed to catch; what() carries the DRAW or OCCT wording already.
        auto model = HsBa::Slicer::BuildCsgModel(name, script, shape_name ? shape_name : "");
        return ToHandle(std::move(model));
    }
    catch (const HsBa::Slicer::RuntimeError& ex)
    {
        WriteReason(error_out, error_cap, ex.what());
        return nullptr;
    }
#else
    WriteReason(error_out, error_cap, "HsBaBuildCsgModel: this build has no OCCT CAD kernel");
    return nullptr;
#endif
}
