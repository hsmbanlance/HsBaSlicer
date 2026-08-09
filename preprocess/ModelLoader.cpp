#include "ModelLoader.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <format>
#include <string_view>

#include "base/ModelFormat.hpp"
#include "base/error.hpp"
#include "meshmodel/IglModel.hpp"
#ifdef USE_OPENVDB
#include "pointcloud/OpenVdbModel.hpp"
#endif

#ifdef USE_OCCT
#include "cadmodel/OcctModel.hpp"
#endif

namespace HsBa::Slicer
{

namespace
{
    std::shared_ptr<IModel> LoadPointCloudModel(const std::string& name, std::string_view filePath)
    {
#ifdef USE_OPENVDB
        auto model = std::make_shared<OpenVdbModel>();
        if (!model->Load(filePath))
        {
            throw RuntimeError(std::format("Failed to load point cloud model: {}", std::string(filePath)));
        }
        return model;
#else
        throw RuntimeError(std::format("Point cloud formats are not supported on this platform (OpenVDB not available): {}", std::string(filePath)));
#endif
    }
}

std::shared_ptr<IModel> ModelLoader::LoadModel(const std::string& name, std::string_view filePath)
{
    if (pool_.Contains(name))
    {
        throw InvalidArgumentError(std::format("Model with name '{}' already exists in pool", name));
    }

    const auto format = ModelTypeFromExtName(std::string(filePath));
    if (IsPointCloudFormat(format))
    {
        return pool_.insert(name, LoadPointCloudModel(name, filePath));
    }

    if (IsMeshFormat(format))
    {
        auto model = std::make_shared<IglModel>();
        if (!model->Load(filePath))
        {
            throw RuntimeError(std::format("Failed to load mesh model: {}", std::string(filePath)));
        }
        return pool_.insert(name, std::static_pointer_cast<IModel>(model));
    }

    if (IsBrepFormat(format))
    {
#ifdef USE_OCCT
        auto model = std::make_shared<OcctModel>();
        if (!model->Load(filePath))
        {
            throw RuntimeError(std::format("Failed to load BRep model: {}", std::string(filePath)));
        }
        return pool_.insert(name, std::static_pointer_cast<IModel>(model));
#else
        throw RuntimeError(std::format("BRep format is not supported on this platform (OCCT not available): {}", std::string(filePath)));
#endif
    }

    throw InvalidArgumentError(std::format("Unsupported model format: {}", std::string(filePath)));
}

const std::shared_ptr<IModel> ModelLoader::GetModel(const std::string& name) const
{
    // NamedObjectPool::get is non-const, but semantically we only read here.
    // Use const_cast to keep the public interface const-correct.
    return pool_.get(name);
}

const std::shared_ptr<IModel> ModelLoader::operator[](const std::string& name) const
{
    return GetModel(name);
}

std::shared_ptr<IModel> ModelLoader::InsertModel(const std::string& name, std::shared_ptr<IModel> model)
{
    if (!model)
    {
        throw InvalidArgumentError("Cannot insert a null model");
    }
    return pool_.insert(name, std::move(model));
}

void ModelLoader::RemoveModel(const std::string& name)
{
    pool_.remove(name);
}

bool ModelLoader::ContainsModel(const std::string& name) const
{
    return pool_.Contains(name);
}

std::size_t ModelLoader::ModelCount() const
{
    return pool_.size();
}

std::vector<std::string> ModelLoader::GetModelNames() const
{
    return pool_.GetNames();
}

std::size_t ModelLoader::Cleanup()
{
    return pool_.Cleanup();
}

#ifdef USE_CGAL

// ---------------------------------------------------------------------------
// Helper: convert any IModel to IglModel (via TriangleMesh if needed)
// ---------------------------------------------------------------------------
static std::shared_ptr<IglModel> ToIglModel(const std::shared_ptr<IModel>& model)
{
    if (auto* igl = dynamic_cast<IglModel*>(model.get()))
    {
        return std::make_shared<IglModel>(*igl);
    }
    // OcctModel or CgalModel -> extract mesh and build IglModel
    auto [verts, faces] = model->TriangleMesh();
    return std::make_shared<IglModel>(std::move(verts), std::move(faces));
}

#ifdef USE_OCCT
// ---------------------------------------------------------------------------
// Helper: convert any IModel to OcctModel (via mesh tessellation if needed)
// ---------------------------------------------------------------------------
static std::shared_ptr<OcctModel> ToOcctModel(const std::shared_ptr<IModel>& model)
{
    if (auto* occt = dynamic_cast<OcctModel*>(model.get()))
    {
        return std::make_shared<OcctModel>(*occt);
    }
    // For mesh models, create OcctModel from triangulated mesh
    auto [verts, faces] = model->TriangleMesh();
    // Build an IglModel first, then convert through mesh data
    // OcctModel does not have a direct mesh constructor, so we go through IglModel
    // and use the mesh data to build a compound of triangles via OCCT
    // For now, throw if conversion is needed
    throw RuntimeError("Direct conversion from mesh to OcctModel is not yet supported. "
                       "Use OcctModel source for boolean operations.");
}
#endif  // USE_OCCT

// ---------------------------------------------------------------------------
// ThickSolid
// ---------------------------------------------------------------------------
std::shared_ptr<IModel> ModelLoader::ThickSolidModel(const std::string& sourceName, const std::string& resultName,
                                                     float thickness)
{
#ifdef USE_OCCT
    auto src = GetModel(sourceName);
    if (!src)
    {
        throw InvalidArgumentError(std::format("Source model '{}' not found", sourceName));
    }
    auto* occtSrc = dynamic_cast<OcctModel*>(src.get());
    if (!occtSrc)
    {
        throw RuntimeError(std::format("ThickSolid requires an OcctModel, but '{}' is not an OcctModel", sourceName));
    }
    auto result = std::make_shared<OcctModel>(ThickSolid(*occtSrc, thickness));
    return pool_.insert(resultName, std::static_pointer_cast<IModel>(result));
#else
    throw NotSupportedError("ThickSolid requires OCCT which is not available on this platform");
#endif
}

std::shared_ptr<IModel> ModelLoader::ThickSolidModel(const std::string& sourceName, const std::string& resultName,
                                                     const std::vector<std::vector<Eigen::Vector3f>>& closingFaces,
                                                     float thickness)
{
#ifdef USE_OCCT
    auto src = GetModel(sourceName);
    if (!src)
    {
        throw InvalidArgumentError(std::format("Source model '{}' not found", sourceName));
    }
    auto* occtSrc = dynamic_cast<OcctModel*>(src.get());
    if (!occtSrc)
    {
        throw RuntimeError(std::format("ThickSolid requires an OcctModel, but '{}' is not an OcctModel", sourceName));
    }
    auto result = std::make_shared<OcctModel>(ThickSolid(*occtSrc, closingFaces, thickness));
    return pool_.insert(resultName, std::static_pointer_cast<IModel>(result));
#else
    throw NotSupportedError("ThickSolid requires OCCT which is not available on this platform");
#endif
}

// ---------------------------------------------------------------------------
// Boolean operations
// ---------------------------------------------------------------------------
std::shared_ptr<IModel> ModelLoader::BooleanUnion(const std::string& leftName, const std::string& rightName,
                                                  const std::string& resultName)
{
    auto left = GetModel(leftName);
    auto right = GetModel(rightName);
    if (!left)
        throw InvalidArgumentError("Left model '" + leftName + "' not found");
    if (!right)
        throw InvalidArgumentError("Right model '" + rightName + "' not found");

#ifdef USE_OCCT
    auto* occtL = dynamic_cast<OcctModel*>(left.get());
    auto* occtR = dynamic_cast<OcctModel*>(right.get());
    if (occtL && occtR)
    {
        auto result = std::make_shared<OcctModel>(Union(*occtL, *occtR));
        return pool_.insert(resultName, std::static_pointer_cast<IModel>(result));
    }
#endif

    // Fallback: use IglModel (requires CGAL)
    auto iglL = ToIglModel(left);
    auto iglR = ToIglModel(right);
    auto result = std::make_shared<IglModel>(Union(*iglL, *iglR));
    return pool_.insert(resultName, std::static_pointer_cast<IModel>(result));
}

std::shared_ptr<IModel> ModelLoader::BooleanIntersection(const std::string& leftName, const std::string& rightName,
                                                         const std::string& resultName)
{
    auto left = GetModel(leftName);
    auto right = GetModel(rightName);
    if (!left)
        throw InvalidArgumentError(std::format("Left model '{}' not found", leftName));
    if (!right)
        throw InvalidArgumentError(std::format("Right model '{}' not found", rightName));

#ifdef USE_OCCT
    auto* occtL = dynamic_cast<OcctModel*>(left.get());
    auto* occtR = dynamic_cast<OcctModel*>(right.get());
    if (occtL && occtR)
    {
        auto result = std::make_shared<OcctModel>(Intersection(*occtL, *occtR));
        return pool_.insert(resultName, std::static_pointer_cast<IModel>(result));
    }
#endif

    auto iglL = ToIglModel(left);
    auto iglR = ToIglModel(right);
    auto result = std::make_shared<IglModel>(Intersection(*iglL, *iglR));
    return pool_.insert(resultName, std::static_pointer_cast<IModel>(result));
}

std::shared_ptr<IModel> ModelLoader::BooleanDifference(const std::string& leftName, const std::string& rightName,
                                                       const std::string& resultName)
{
    auto left = GetModel(leftName);
    auto right = GetModel(rightName);
    if (!left)
        throw InvalidArgumentError(std::format("Left model '{}' not found", leftName));
    if (!right)
        throw InvalidArgumentError(std::format("Right model '{}' not found", rightName));

#ifdef USE_OCCT
    auto* occtL = dynamic_cast<OcctModel*>(left.get());
    auto* occtR = dynamic_cast<OcctModel*>(right.get());
    if (occtL && occtR)
    {
        auto result = std::make_shared<OcctModel>(Difference(*occtL, *occtR));
        return pool_.insert(resultName, std::static_pointer_cast<IModel>(result));
    }
#endif

    auto iglL = ToIglModel(left);
    auto iglR = ToIglModel(right);
    auto result = std::make_shared<IglModel>(Difference(*iglL, *iglR));
    return pool_.insert(resultName, std::static_pointer_cast<IModel>(result));
}

std::shared_ptr<IModel> ModelLoader::BooleanXor(const std::string& leftName, const std::string& rightName,
                                                const std::string& resultName)
{
    auto left = GetModel(leftName);
    auto right = GetModel(rightName);
    if (!left)
        throw InvalidArgumentError(std::format("Left model '{}' not found", leftName));
    if (!right)
        throw InvalidArgumentError(std::format("Right model '{}' not found", rightName));

#ifdef USE_OCCT
    auto* occtL = dynamic_cast<OcctModel*>(left.get());
    auto* occtR = dynamic_cast<OcctModel*>(right.get());
    if (occtL && occtR)
    {
        auto result = std::make_shared<OcctModel>(Xor(*occtL, *occtR));
        return pool_.insert(resultName, std::static_pointer_cast<IModel>(result));
    }
#endif

    auto iglL = ToIglModel(left);
    auto iglR = ToIglModel(right);
    auto result = std::make_shared<IglModel>(Xor(*iglL, *iglR));
    return pool_.insert(resultName, std::static_pointer_cast<IModel>(result));
}

#endif  // USE_CGAL

#ifdef USE_OPENVDB

// ---------------------------------------------------------------------------
// Helper: retrieve and cast a model to OpenVdbModel
// ---------------------------------------------------------------------------
static OpenVdbModel& AsOpenVdbModel(const std::shared_ptr<IModel>& model, const std::string& name)
{
    auto* vdb = dynamic_cast<OpenVdbModel*>(model.get());
    if (!vdb)
    {
        throw RuntimeError(std::format("Model '{}' is not a point cloud (OpenVdbModel)", name));
    }
    return *vdb;
}

// ---------------------------------------------------------------------------
// Point cloud operations
// ---------------------------------------------------------------------------
std::shared_ptr<IModel> ModelLoader::PointCloudToMesh(const std::string& sourceName, const std::string& resultName,
                                                      float voxelSize, float particleRadius)
{
    auto src = GetModel(sourceName);
    if (!src)
    {
        throw InvalidArgumentError(std::format("Source model '{}' not found", sourceName));
    }

    auto& vdbModel = AsOpenVdbModel(src, sourceName);
    auto [vertices, faces] = vdbModel.GenerateMesh(voxelSize, particleRadius);

    if (faces.rows() == 0)
    {
        throw RuntimeError(std::format("Point cloud to mesh reconstruction failed for '{}'", sourceName));
    }

    auto result = std::make_shared<IglModel>(std::move(vertices), std::move(faces));
    return pool_.insert(resultName, std::static_pointer_cast<IModel>(result));
}

std::shared_ptr<IModel> ModelLoader::MergePointClouds(const std::string& leftName, const std::string& rightName,
                                                      const std::string& resultName)
{
    auto left = GetModel(leftName);
    auto right = GetModel(rightName);
    if (!left)
    {
        throw InvalidArgumentError(std::format("Left model '{}' not found", leftName));
    }
    if (!right)
    {
        throw InvalidArgumentError(std::format("Right model '{}' not found", rightName));
    }

    auto& vdbLeft = AsOpenVdbModel(left, leftName);
    auto& vdbRight = AsOpenVdbModel(right, rightName);

    auto result = std::make_shared<OpenVdbModel>(vdbLeft);
    result->Merge(vdbRight);
    return pool_.insert(resultName, std::static_pointer_cast<IModel>(result));
}

std::shared_ptr<IModel> ModelLoader::DownsamplePointCloud(const std::string& sourceName, const std::string& resultName,
                                                          float voxelSize)
{
    auto src = GetModel(sourceName);
    if (!src)
    {
        throw InvalidArgumentError(std::format("Source model '{}' not found", sourceName));
    }

    auto& vdbModel = AsOpenVdbModel(src, sourceName);
    auto result = std::make_shared<OpenVdbModel>(vdbModel);
    result->Downsample(voxelSize);
    return pool_.insert(resultName, std::static_pointer_cast<IModel>(result));
}

std::shared_ptr<IModel> ModelLoader::RemovePointCloudOutliers(const std::string& sourceName,
                                                              const std::string& resultName,
                                                              std::size_t k, float multiplier)
{
    auto src = GetModel(sourceName);
    if (!src)
    {
        throw InvalidArgumentError(std::format("Source model '{}' not found", sourceName));
    }

    auto& vdbModel = AsOpenVdbModel(src, sourceName);
    auto result = std::make_shared<OpenVdbModel>(vdbModel);
    result->RemoveStatisticalOutliers(k, multiplier);
    return pool_.insert(resultName, std::static_pointer_cast<IModel>(result));
}

Eigen::Vector3f ModelLoader::PointCloudCentroid(const std::string& sourceName) const
{
    auto src = GetModel(sourceName);
    if (!src)
    {
        throw InvalidArgumentError(std::format("Source model '{}' not found", sourceName));
    }

    auto& vdbModel = AsOpenVdbModel(src, sourceName);
    return vdbModel.Centroid();
}

Eigen::MatrixXf ModelLoader::PointCloudNormals(const std::string& sourceName, std::size_t k) const
{
    auto src = GetModel(sourceName);
    if (!src)
    {
        throw InvalidArgumentError(std::format("Source model '{}' not found", sourceName));
    }

    auto& vdbModel = AsOpenVdbModel(src, sourceName);
    return vdbModel.ComputeNormals(k);
}

std::size_t ModelLoader::PointCloudCount(const std::string& sourceName) const
{
    auto src = GetModel(sourceName);
    if (!src)
    {
        throw InvalidArgumentError(std::format("Source model '{}' not found", sourceName));
    }

    auto& vdbModel = AsOpenVdbModel(src, sourceName);
    return vdbModel.PointCount();
}

#endif  // USE_OPENVDB

}  // namespace HsBa::Slicer
