#ifndef HSBA_NO_DLL_LOADER

#include "UserCustomPointCloudModel.hpp"

#include <boost/dll.hpp>

#include "base/error.hpp"
#include "base/object_pool.hpp"

namespace HsBa::Slicer
{
namespace
{
static NamedObjectPool<UserCustomPointCloudDll, 10> dllPool;
}
class UserCustomPointCloudDll::Impl
{
public:
    boost::dll::shared_library dll_;
    std::string addedFunName_;
    Impl(std::string_view dllPath, std::string_view addedFunName) : dll_(dllPath.data()), addedFunName_(addedFunName) {}
};
UserCustomPointCloudDll::UserCustomPointCloudDll(std::string_view dllPath, std::string_view addedFunName)
    : impl_(std::make_unique<Impl>(dllPath, addedFunName))
{
}

UserCustomPointCloudDll::CreateModelFunc UserCustomPointCloudDll::GetCreateModelFunc() const
{
    if (impl_->dll_.has(impl_->addedFunName_ + "_create_model"))
    {
        // get requires the function type itself, not a function pointer typedef:
        // with a pointer typedef it would dereference the symbol address
        return impl_->dll_.get<IModel*()>(impl_->addedFunName_ + "_create_model");
    }
    return nullptr;
}

UserCustomPointCloudDll::DestroyModelFunc UserCustomPointCloudDll::GetDestroyModelFunc() const
{
    if (impl_->dll_.has(impl_->addedFunName_ + "_destroy_model"))
    {
        return impl_->dll_.get<void(IModel*)>(impl_->addedFunName_ + "_destroy_model");
    }
    return nullptr;
}

UserCustomPointCloudDll::AddPointFunc UserCustomPointCloudDll::GetAddPointFunc() const
{
    if (impl_->dll_.has(impl_->addedFunName_ + "_add_point"))
    {
        return impl_->dll_.get<void(IModel*, HsBaVector3f_t)>(impl_->addedFunName_ + "_add_point");
    }
    return nullptr;
}

UserCustomPointCloudDll::AddPointsFunc UserCustomPointCloudDll::GetAddPointsFunc() const
{
    if (impl_->dll_.has(impl_->addedFunName_ + "_add_points"))
    {
        return impl_->dll_.get<void(IModel*, const HsBaVector3f_t*, size_t)>(impl_->addedFunName_ + "_add_points");
    }
    return nullptr;
}

UserCustomPointCloudDll::GetPointsFunc UserCustomPointCloudDll::GetGetPointsFunc() const
{
    if (impl_->dll_.has(impl_->addedFunName_ + "_get_points"))
    {
        return impl_->dll_.get<void(const IModel*, HsBaVector3f_t*)>(impl_->addedFunName_ + "_get_points");
    }
    return nullptr;
}

UserCustomPointCloudDll::PointCountFunc UserCustomPointCloudDll::GetPointCountFunc() const
{
    if (impl_->dll_.has(impl_->addedFunName_ + "_point_count"))
    {
        return impl_->dll_.get<size_t(const IModel*)>(impl_->addedFunName_ + "_point_count");
    }
    return nullptr;
}

UserCustomPointCloudDll::IsEmptyFunc UserCustomPointCloudDll::GetIsEmptyFunc() const
{
    if (impl_->dll_.has(impl_->addedFunName_ + "_is_empty"))
    {
        return impl_->dll_.get<bool(const IModel*)>(impl_->addedFunName_ + "_is_empty");
    }
    return nullptr;
}

UserCustomPointCloudDll::ClearFunc UserCustomPointCloudDll::GetClearFunc() const
{
    if (impl_->dll_.has(impl_->addedFunName_ + "_clear"))
    {
        return impl_->dll_.get<void(IModel*)>(impl_->addedFunName_ + "_clear");
    }
    return nullptr;
}

UserCustomPointCloudDll::CentroidFunc UserCustomPointCloudDll::GetCentroidFunc() const
{
    if (impl_->dll_.has(impl_->addedFunName_ + "_centroid"))
    {
        return impl_->dll_.get<HsBaVector3f_t(const IModel*)>(impl_->addedFunName_ + "_centroid");
    }
    return nullptr;
}

UserCustomPointCloudDll::MergeFunc UserCustomPointCloudDll::GetMergeFunc() const
{
    if (impl_->dll_.has(impl_->addedFunName_ + "_merge"))
    {
        return impl_->dll_.get<void(IModel*, const IModel*)>(impl_->addedFunName_ + "_merge");
    }
    return nullptr;
}

UserCustomPointCloudDll::RemoveStatisticalOutliersFunc UserCustomPointCloudDll::GetRemoveStatisticalOutliersFunc() const
{
    if (impl_->dll_.has(impl_->addedFunName_ + "_remove_statistical_outliers"))
    {
        return impl_->dll_.get<void(IModel*, size_t, float)>(impl_->addedFunName_ + "_remove_statistical_outliers");
    }
    return nullptr;
}

UserCustomPointCloudDll::ComputeNormalsFunc UserCustomPointCloudDll::GetComputeNormalsFunc() const
{
    if (impl_->dll_.has(impl_->addedFunName_ + "_compute_normals"))
    {
        return impl_->dll_.get<void(const IModel*, size_t, HsBaVector3f_t*)>(impl_->addedFunName_ + "_compute_normals");
    }
    return nullptr;
}

UserCustomPointCloudDll::DownsampleFunc UserCustomPointCloudDll::GetDownsampleFunc() const
{
    if (impl_->dll_.has(impl_->addedFunName_ + "_downsample"))
    {
        return impl_->dll_.get<void(IModel*, float)>(impl_->addedFunName_ + "_downsample");
    }
    return nullptr;
}

UserCustomPointCloudDll::VoxelizeFunc UserCustomPointCloudDll::GetVoxelizeFunc() const
{
    if (impl_->dll_.has(impl_->addedFunName_ + "_voxelize"))
    {
        return impl_->dll_.get<void(IModel*, float)>(impl_->addedFunName_ + "_voxelize");
    }
    return nullptr;
}

UserCustomPointCloudDll::~UserCustomPointCloudDll()
{
}

UserCustomPointCloudModel::~UserCustomPointCloudModel()
{
    if (model_ && dll_)
    {
        if (auto destroyModelFunc = dll_->GetDestroyModelFunc())
        {
            destroyModelFunc(model_);
        }
    }
}

void UserCustomPointCloudModel::LoadDll(std::string_view dllPath, std::string_view addedFunName)
{
    if (dllPool.Contains(dllPath.data()))
    {
        dll_ = dllPool.get(dllPath.data());
    }
    else
    {
        dll_ = dllPool.emplace(dllPath.data(), dllPath, addedFunName);
    }
}

void UserCustomPointCloudModel::UnloadDll()
{
    dll_.reset();
}

void UserCustomPointCloudModel::EnsureModel()
{
    if (model_)
    {
        return;
    }
    if (!dll_)
    {
        throw RuntimeError("Dll not loaded");
    }
    auto createModelFunc = dll_->GetCreateModelFunc();
    if (!createModelFunc)
    {
        throw RuntimeError("Create model function not found in DLL");
    }
    model_ = createModelFunc();
}

bool UserCustomPointCloudModel::Load(std::string_view fileName)
{
    EnsureModel();
    return model_->Load(fileName);
}

bool UserCustomPointCloudModel::Save(std::string_view fileName, const ModelFormat format) const
{
    if (model_)
    {
        return model_->Save(fileName, format);
    }
    return false;
}

void UserCustomPointCloudModel::Translate(const Eigen::Vector3f& translation)
{
    if (model_)
    {
        model_->Translate(translation);
    }
    else
    {
        throw RuntimeError("Model not loaded");
    }
}

void UserCustomPointCloudModel::Rotate(const Eigen::Quaternionf& rotation)
{
    if (model_)
    {
        model_->Rotate(rotation);
    }
    else
    {
        throw RuntimeError("Model not loaded");
    }
}

void UserCustomPointCloudModel::Scale(const float scale)
{
    if (model_)
    {
        model_->Scale(scale);
    }
    else
    {
        throw RuntimeError("Model not loaded");
    }
}

void UserCustomPointCloudModel::Scale(const Eigen::Vector3f& scale)
{
    if (model_)
    {
        model_->Scale(scale);
    }
    else
    {
        throw RuntimeError("Model not loaded");
    }
}

void UserCustomPointCloudModel::Transform(const Eigen::Isometry3f& transform)
{
    if (model_)
    {
        model_->Transform(transform);
    }
    else
    {
        throw RuntimeError("Model not loaded");
    }
}

void UserCustomPointCloudModel::Transform(const Eigen::Matrix4f& transform)
{
    if (model_)
    {
        model_->Transform(transform);
    }
    else
    {
        throw RuntimeError("Model not loaded");
    }
}

void UserCustomPointCloudModel::Transform(const Eigen::Transform<float, 3, Eigen::Affine>& transform)
{
    if (model_)
    {
        model_->Transform(transform);
    }
    else
    {
        throw RuntimeError("Model not loaded");
    }
}

void UserCustomPointCloudModel::BoundingBox(Eigen::Vector3f& min, Eigen::Vector3f& max) const
{
    if (model_)
    {
        model_->BoundingBox(min, max);
    }
    else
    {
        throw RuntimeError("Model not loaded");
    }
}

float UserCustomPointCloudModel::Volume() const
{
    if (model_)
    {
        return model_->Volume();
    }
    else
    {
        throw RuntimeError("Model not loaded");
    }
}

std::pair<Eigen::MatrixXf, Eigen::MatrixXi> UserCustomPointCloudModel::TriangleMesh() const
{
    if (model_)
    {
        return model_->TriangleMesh();
    }
    else
    {
        throw RuntimeError("Model not loaded");
    }
}

void UserCustomPointCloudModel::AddPoint(const Eigen::Vector3f& point)
{
    EnsureModel();
    auto addPointFunc = dll_->GetAddPointFunc();
    if (!addPointFunc)
    {
        throw RuntimeError("Add point function not found in DLL");
    }
    addPointFunc(model_, HsBaVector3f_t{point.x(), point.y(), point.z()});
}

void UserCustomPointCloudModel::AddPoints(const std::vector<Eigen::Vector3f>& points)
{
    EnsureModel();
    auto addPointsFunc = dll_->GetAddPointsFunc();
    if (!addPointsFunc)
    {
        throw RuntimeError("Add points function not found in DLL");
    }
    // Eigen::Vector3f and HsBaVector3f_t share the same {float, float, float} layout
    addPointsFunc(model_, reinterpret_cast<const HsBaVector3f_t*>(points.data()), points.size());
}

std::vector<Eigen::Vector3f> UserCustomPointCloudModel::Points() const
{
    if (!model_)
    {
        return {};
    }
    auto getPointsFunc = dll_->GetGetPointsFunc();
    if (!getPointsFunc)
    {
        throw RuntimeError("Get points function not found in DLL");
    }
    std::vector<Eigen::Vector3f> points(PointCount());
    // Eigen::Vector3f and HsBaVector3f_t share the same {float, float, float} layout
    getPointsFunc(model_, reinterpret_cast<HsBaVector3f_t*>(points.data()));
    return points;
}

std::size_t UserCustomPointCloudModel::PointCount() const
{
    if (!model_)
    {
        return 0;
    }
    auto pointCountFunc = dll_->GetPointCountFunc();
    if (!pointCountFunc)
    {
        throw RuntimeError("Point count function not found in DLL");
    }
    return pointCountFunc(model_);
}

bool UserCustomPointCloudModel::IsEmpty() const
{
    if (!model_)
    {
        return true;
    }
    auto isEmptyFunc = dll_->GetIsEmptyFunc();
    if (!isEmptyFunc)
    {
        throw RuntimeError("Is empty function not found in DLL");
    }
    return isEmptyFunc(model_);
}

void UserCustomPointCloudModel::Clear()
{
    EnsureModel();
    auto clearFunc = dll_->GetClearFunc();
    if (!clearFunc)
    {
        throw RuntimeError("Clear function not found in DLL");
    }
    clearFunc(model_);
}

void UserCustomPointCloudModel::SetFromVertices(const Eigen::MatrixXf& vertices)
{
    EnsureModel();
    Clear();
    std::vector<Eigen::Vector3f> points;
    points.reserve(vertices.rows());
    for (Eigen::Index row = 0; row < vertices.rows(); ++row)
    {
        points.emplace_back(vertices.row(row).transpose());
    }
    AddPoints(points);
}

Eigen::MatrixXf UserCustomPointCloudModel::ToVertices() const
{
    const auto points = Points();
    Eigen::MatrixXf vertices(static_cast<Eigen::Index>(points.size()), 3);
    for (std::size_t index = 0; index < points.size(); ++index)
    {
        vertices.row(static_cast<Eigen::Index>(index)) = points[index].transpose();
    }
    return vertices;
}

void UserCustomPointCloudModel::Voxelize(float voxelSize)
{
    if (!model_)
    {
        throw RuntimeError("Model not loaded");
    }
    auto voxelizeFunc = dll_->GetVoxelizeFunc();
    if (!voxelizeFunc)
    {
        throw RuntimeError("Voxelize function not found in DLL");
    }
    voxelizeFunc(model_, voxelSize);
}

void UserCustomPointCloudModel::Downsample(float voxelSize)
{
    if (!model_)
    {
        throw RuntimeError("Model not loaded");
    }
    auto downsampleFunc = dll_->GetDownsampleFunc();
    if (!downsampleFunc)
    {
        throw RuntimeError("Downsample function not found in DLL");
    }
    downsampleFunc(model_, voxelSize);
}

Eigen::Vector3f UserCustomPointCloudModel::Centroid() const
{
    if (!model_)
    {
        return Eigen::Vector3f::Zero();
    }
    auto centroidFunc = dll_->GetCentroidFunc();
    if (!centroidFunc)
    {
        throw RuntimeError("Centroid function not found in DLL");
    }
    const auto centroid = centroidFunc(model_);
    return Eigen::Vector3f{centroid.x, centroid.y, centroid.z};
}

void UserCustomPointCloudModel::Merge(const UserCustomPointCloudModel& other)
{
    if (model_ && other.dll_ == dll_ && other.model_)
    {
        auto mergeFunc = dll_->GetMergeFunc();
        if (!mergeFunc)
        {
            throw RuntimeError("Merge function not found in DLL");
        }
        mergeFunc(model_, other.model_);
    }
    else
    {
        throw RuntimeError("Model not loaded or incompatible DLLs");
    }
}

void UserCustomPointCloudModel::RemoveStatisticalOutliers(std::size_t k, float multiplier)
{
    if (!model_)
    {
        throw RuntimeError("Model not loaded");
    }
    auto removeStatisticalOutliersFunc = dll_->GetRemoveStatisticalOutliersFunc();
    if (!removeStatisticalOutliersFunc)
    {
        throw RuntimeError("Remove statistical outliers function not found in DLL");
    }
    removeStatisticalOutliersFunc(model_, k, multiplier);
}

Eigen::MatrixXf UserCustomPointCloudModel::ComputeNormals(std::size_t k) const
{
    if (!model_)
    {
        return Eigen::MatrixXf(0, 3);
    }
    auto computeNormalsFunc = dll_->GetComputeNormalsFunc();
    if (!computeNormalsFunc)
    {
        throw RuntimeError("Compute normals function not found in DLL");
    }
    // Eigen::MatrixXf is column major, so use a HsBaVector3f_t buffer to match the C layout
    std::vector<HsBaVector3f_t> normalsBuffer(PointCount());
    computeNormalsFunc(model_, k, normalsBuffer.data());
    Eigen::MatrixXf normals(static_cast<Eigen::Index>(normalsBuffer.size()), 3);
    for (std::size_t index = 0; index < normalsBuffer.size(); ++index)
    {
        normals.row(static_cast<Eigen::Index>(index)) =
            Eigen::Vector3f{normalsBuffer[index].x, normalsBuffer[index].y, normalsBuffer[index].z}.transpose();
    }
    return normals;
}
}  // namespace HsBa::Slicer

#endif  // !HSBA_NO_DLL_LOADER
