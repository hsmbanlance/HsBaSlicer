/** @file mock_point_cloud_dll.cpp
 * @brief A mock point cloud dynamic library used by user_custom_point_cloud_model_test.
 * It exports C style functions with the "mockpc_" prefix that mimic the user custom point cloud dll
 * contract of UserCustomPointCloudModel, backed by a simple in-memory point cloud model.
 */
#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>
#include <tuple>
#include <vector>

#include <Eigen/Core>
#include <Eigen/Geometry>

#include "base/IModel.hpp"

#ifdef _WIN32
#define MOCK_PC_EXPORT extern "C" __declspec(dllexport)
#else
#define MOCK_PC_EXPORT extern "C" __attribute__((visibility("default")))
#endif

namespace
{
/// @brief A minimal in-memory point cloud model used to mock a user custom point cloud model.
class MockPointCloudModel final : public HsBa::Slicer::IModel
{
public:
    std::vector<Eigen::Vector3f> points_;

    bool Load(std::string_view) override { return true; }
    bool Save(std::string_view, const HsBa::Slicer::ModelFormat) const override { return true; }

    void Translate(const Eigen::Vector3f& translation) override
    {
        for (auto& point : points_)
        {
            point += translation;
        }
    }
    void Rotate(const Eigen::Quaternionf& rotation) override
    {
        for (auto& point : points_)
        {
            point = rotation * point;
        }
    }
    void Scale(const float scale) override
    {
        for (auto& point : points_)
        {
            point *= scale;
        }
    }
    void Scale(const Eigen::Vector3f& scale) override
    {
        for (auto& point : points_)
        {
            point = point.cwiseProduct(scale);
        }
    }
    void Transform(const Eigen::Isometry3f& transform) override
    {
        for (auto& point : points_)
        {
            point = transform * point;
        }
    }
    void Transform(const Eigen::Matrix4f& transform) override
    {
        for (auto& point : points_)
        {
            point = (transform * point.homogeneous()).hnormalized();
        }
    }
    void Transform(const Eigen::Transform<float, 3, Eigen::Affine>& transform) override
    {
        for (auto& point : points_)
        {
            point = transform * point;
        }
    }
    void BoundingBox(Eigen::Vector3f& min, Eigen::Vector3f& max) const override
    {
        if (points_.empty())
        {
            min = Eigen::Vector3f::Zero();
            max = Eigen::Vector3f::Zero();
            return;
        }
        min = points_.front();
        max = points_.front();
        for (const auto& point : points_)
        {
            min = min.cwiseMin(point);
            max = max.cwiseMax(point);
        }
    }
    float Volume() const override { return 0.0f; }
    std::pair<Eigen::MatrixXf, Eigen::MatrixXi> TriangleMesh() const override
    {
        Eigen::MatrixXf vertices(static_cast<Eigen::Index>(points_.size()), 3);
        for (std::size_t index = 0; index < points_.size(); ++index)
        {
            vertices.row(static_cast<Eigen::Index>(index)) = points_[index].transpose();
        }
        return {vertices, Eigen::MatrixXi(0, 3)};
    }
};

MockPointCloudModel* AsMockModel(HsBa::Slicer::IModel* model) { return dynamic_cast<MockPointCloudModel*>(model); }

const MockPointCloudModel* AsMockModel(const HsBa::Slicer::IModel* model)
{
    return dynamic_cast<const MockPointCloudModel*>(model);
}

Eigen::Vector3f ToEigen(const HsBaVector3f_t& point) { return Eigen::Vector3f{point.x, point.y, point.z}; }

HsBaVector3f_t ToHsBa(const Eigen::Vector3f& point) { return HsBaVector3f_t{point.x(), point.y(), point.z()}; }
}  // namespace

MOCK_PC_EXPORT HsBa::Slicer::IModel* mockpc_create_model() { return new MockPointCloudModel(); }

MOCK_PC_EXPORT void mockpc_destroy_model(HsBa::Slicer::IModel* model) { delete model; }

MOCK_PC_EXPORT void mockpc_add_point(HsBa::Slicer::IModel* model, HsBaVector3f_t point)
{
    if (auto mockModel = AsMockModel(model))
    {
        mockModel->points_.push_back(ToEigen(point));
    }
}

MOCK_PC_EXPORT void mockpc_add_points(HsBa::Slicer::IModel* model, const HsBaVector3f_t* points, size_t count)
{
    if (auto mockModel = AsMockModel(model))
    {
        for (size_t index = 0; index < count; ++index)
        {
            mockModel->points_.push_back(ToEigen(points[index]));
        }
    }
}

MOCK_PC_EXPORT void mockpc_get_points(const HsBa::Slicer::IModel* model, HsBaVector3f_t* outPoints)
{
    if (const auto mockModel = AsMockModel(model))
    {
        for (std::size_t index = 0; index < mockModel->points_.size(); ++index)
        {
            outPoints[index] = ToHsBa(mockModel->points_[index]);
        }
    }
}

MOCK_PC_EXPORT size_t mockpc_point_count(const HsBa::Slicer::IModel* model)
{
    if (const auto mockModel = AsMockModel(model))
    {
        return mockModel->points_.size();
    }
    return 0;
}

MOCK_PC_EXPORT bool mockpc_is_empty(const HsBa::Slicer::IModel* model)
{
    if (const auto mockModel = AsMockModel(model))
    {
        return mockModel->points_.empty();
    }
    return true;
}

MOCK_PC_EXPORT void mockpc_clear(HsBa::Slicer::IModel* model)
{
    if (auto mockModel = AsMockModel(model))
    {
        mockModel->points_.clear();
    }
}

MOCK_PC_EXPORT HsBaVector3f_t mockpc_centroid(const HsBa::Slicer::IModel* model)
{
    if (const auto mockModel = AsMockModel(model); mockModel && !mockModel->points_.empty())
    {
        Eigen::Vector3f sum = Eigen::Vector3f::Zero();
        for (const auto& point : mockModel->points_)
        {
            sum += point;
        }
        return ToHsBa(sum / static_cast<float>(mockModel->points_.size()));
    }
    return HsBaVector3f_t{0.0f, 0.0f, 0.0f};
}

MOCK_PC_EXPORT void mockpc_merge(HsBa::Slicer::IModel* model, const HsBa::Slicer::IModel* other)
{
    if (auto mockModel = AsMockModel(model))
    {
        if (const auto mockOther = AsMockModel(other))
        {
            mockModel->points_.insert(mockModel->points_.end(), mockOther->points_.begin(), mockOther->points_.end());
        }
    }
}

MOCK_PC_EXPORT void mockpc_remove_statistical_outliers(HsBa::Slicer::IModel* model, size_t k, float multiplier)
{
    auto mockModel = AsMockModel(model);
    if (!mockModel || mockModel->points_.size() <= 1 || k == 0)
    {
        return;
    }
    const auto& points = mockModel->points_;
    const size_t neighborCount = std::min(k, points.size() - 1);
    // mean distance to the k nearest neighbors of each point
    std::vector<float> meanDistances(points.size(), 0.0f);
    for (std::size_t index = 0; index < points.size(); ++index)
    {
        std::vector<float> distances;
        distances.reserve(points.size() - 1);
        for (std::size_t other = 0; other < points.size(); ++other)
        {
            if (other != index)
            {
                distances.push_back((points[index] - points[other]).norm());
            }
        }
        std::nth_element(distances.begin(), distances.begin() + neighborCount, distances.end());
        float sum = 0.0f;
        for (std::size_t neighbor = 0; neighbor < neighborCount; ++neighbor)
        {
            sum += distances[neighbor];
        }
        meanDistances[index] = sum / static_cast<float>(neighborCount);
    }
    const float mean =
        std::accumulate(meanDistances.begin(), meanDistances.end(), 0.0f) / static_cast<float>(meanDistances.size());
    float variance = 0.0f;
    for (const float meanDistance : meanDistances)
    {
        variance += (meanDistance - mean) * (meanDistance - mean);
    }
    const float stdDev = std::sqrt(variance / static_cast<float>(meanDistances.size()));
    const float threshold = mean + multiplier * stdDev;
    std::vector<Eigen::Vector3f> inliers;
    inliers.reserve(points.size());
    for (std::size_t index = 0; index < points.size(); ++index)
    {
        if (meanDistances[index] <= threshold)
        {
            inliers.push_back(points[index]);
        }
    }
    mockModel->points_ = std::move(inliers);
}

MOCK_PC_EXPORT void mockpc_compute_normals(const HsBa::Slicer::IModel* model, size_t, HsBaVector3f_t* outNormals)
{
    const auto mockModel = AsMockModel(model);
    if (!mockModel)
    {
        return;
    }
    // mock normal estimation: radial direction from the centroid, +Z for degenerate points
    Eigen::Vector3f centroid = Eigen::Vector3f::Zero();
    for (const auto& point : mockModel->points_)
    {
        centroid += point;
    }
    if (!mockModel->points_.empty())
    {
        centroid /= static_cast<float>(mockModel->points_.size());
    }
    for (std::size_t index = 0; index < mockModel->points_.size(); ++index)
    {
        Eigen::Vector3f normal = mockModel->points_[index] - centroid;
        if (normal.squaredNorm() < 1e-12f)
        {
            normal = Eigen::Vector3f::UnitZ();
        }
        outNormals[index] = ToHsBa(normal.normalized());
    }
}

MOCK_PC_EXPORT void mockpc_downsample(HsBa::Slicer::IModel* model, float voxelSize)
{
    auto mockModel = AsMockModel(model);
    if (!mockModel || voxelSize <= 0.0f)
    {
        return;
    }
    // keep the first point within each voxel
    std::map<std::tuple<long long, long long, long long>, Eigen::Vector3f> voxels;
    for (const auto& point : mockModel->points_)
    {
        const auto key = std::make_tuple(static_cast<long long>(std::floor(point.x() / voxelSize)),
                                         static_cast<long long>(std::floor(point.y() / voxelSize)),
                                         static_cast<long long>(std::floor(point.z() / voxelSize)));
        voxels.emplace(key, point);
    }
    std::vector<Eigen::Vector3f> downsampled;
    downsampled.reserve(voxels.size());
    for (const auto& [key, point] : voxels)
    {
        downsampled.push_back(point);
    }
    mockModel->points_ = std::move(downsampled);
}

MOCK_PC_EXPORT void mockpc_voxelize(HsBa::Slicer::IModel* model, float voxelSize)
{
    auto mockModel = AsMockModel(model);
    if (!mockModel || voxelSize <= 0.0f)
    {
        return;
    }
    // snap each point to its voxel center
    for (auto& point : mockModel->points_)
    {
        point = (point.array() / voxelSize).floor() * voxelSize + voxelSize / 2.0f;
    }
}
