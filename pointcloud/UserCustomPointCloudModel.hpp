#pragma once
#ifndef HSBA_NO_DLL_LOADER

#ifndef HSBA_SLICER_USER_CUSTOM_POINT_CLOUD_MODEL_HPP
#define HSBA_SLICER_USER_CUSTOM_POINT_CLOUD_MODEL_HPP

#include <memory>
#include <vector>

#include "base/IModel.hpp"

namespace HsBa::Slicer
{
class UserCustomPointCloudModel;
class IUserCustomPointCloud
{
public:
    typedef IModel* (*CreateModelFunc)();
    typedef void (*DestroyModelFunc)(IModel*);
    typedef void (*AddPointFunc)(IModel*, HsBaVector3f_t point);
    typedef void (*AddPointsFunc)(IModel*, const HsBaVector3f_t* points, size_t count);
    typedef void (*GetPointsFunc)(const IModel*, HsBaVector3f_t* outPoints);
    typedef size_t (*PointCountFunc)(const IModel*);
    typedef bool (*IsEmptyFunc)(const IModel*);
    typedef void (*ClearFunc)(IModel*);
    typedef HsBaVector3f_t (*CentroidFunc)(const IModel*);
    typedef void (*MergeFunc)(IModel*, const IModel*);
    typedef void (*RemoveStatisticalOutliersFunc)(IModel*, size_t k, float multiplier);
    typedef void (*ComputeNormalsFunc)(const IModel*, size_t k, HsBaVector3f_t* outNormals);
    typedef void (*DownsampleFunc)(IModel*, float voxelSize);
    typedef void (*VoxelizeFunc)(IModel*, float voxelSize);
    virtual CreateModelFunc GetCreateModelFunc() const = 0;
    virtual DestroyModelFunc GetDestroyModelFunc() const = 0;
    virtual AddPointFunc GetAddPointFunc() const = 0;
    virtual AddPointsFunc GetAddPointsFunc() const = 0;
    virtual GetPointsFunc GetGetPointsFunc() const = 0;
    virtual PointCountFunc GetPointCountFunc() const = 0;
    virtual IsEmptyFunc GetIsEmptyFunc() const = 0;
    virtual ClearFunc GetClearFunc() const = 0;
    virtual CentroidFunc GetCentroidFunc() const = 0;
    virtual MergeFunc GetMergeFunc() const = 0;
    virtual RemoveStatisticalOutliersFunc GetRemoveStatisticalOutliersFunc() const = 0;
    virtual ComputeNormalsFunc GetComputeNormalsFunc() const = 0;
    virtual DownsampleFunc GetDownsampleFunc() const = 0;
    virtual VoxelizeFunc GetVoxelizeFunc() const = 0;
};
class UserCustomPointCloudDll final : public IUserCustomPointCloud
{
public:
    friend class UserCustomPointCloudModel;
    UserCustomPointCloudDll(std::string_view dllPath, std::string_view addedFunName);
    CreateModelFunc GetCreateModelFunc() const override;
    DestroyModelFunc GetDestroyModelFunc() const override;
    AddPointFunc GetAddPointFunc() const override;
    AddPointsFunc GetAddPointsFunc() const override;
    GetPointsFunc GetGetPointsFunc() const override;
    PointCountFunc GetPointCountFunc() const override;
    IsEmptyFunc GetIsEmptyFunc() const override;
    ClearFunc GetClearFunc() const override;
    CentroidFunc GetCentroidFunc() const override;
    MergeFunc GetMergeFunc() const override;
    RemoveStatisticalOutliersFunc GetRemoveStatisticalOutliersFunc() const override;
    ComputeNormalsFunc GetComputeNormalsFunc() const override;
    DownsampleFunc GetDownsampleFunc() const override;
    VoxelizeFunc GetVoxelizeFunc() const override;
    ~UserCustomPointCloudDll();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

class UserCustomPointCloudModel : public IModel
{
public:
    UserCustomPointCloudModel() = default;
    ~UserCustomPointCloudModel() override;
    void LoadDll(std::string_view dllPath, std::string_view addedFunName);  // load the dll
    void UnloadDll();                                                       // unload the dll
    bool Load(std::string_view fileName) override;                          // load the model from a file
    bool Save(std::string_view fileName, const ModelFormat format) const override;  // save the model to a file

    void Translate(const Eigen::Vector3f& translation) override;  // translate the model
    void Rotate(const Eigen::Quaternionf& rotation) override;     // rotate the model
    void Scale(const float scale) override;
    void Scale(const Eigen::Vector3f& scale) override;                                    // scale the model
    void Transform(const Eigen::Isometry3f& transform) override;                          // transform the model
    void Transform(const Eigen::Matrix4f& transform) override;                            // transform the model
    void Transform(const Eigen::Transform<float, 3, Eigen::Affine>& transform) override;  // transform the model
    void BoundingBox(Eigen::Vector3f& min,
                     Eigen::Vector3f& max) const override;  // get the AA bounding box of the model
    float Volume() const override;                          // get the volume of the model
    std::pair<Eigen::MatrixXf, Eigen::MatrixXi> TriangleMesh() const override;  // get igl style trianglemesh

    // point cloud exits consistent with OpenVdbModel
    void AddPoint(const Eigen::Vector3f& point);
    void AddPoints(const std::vector<Eigen::Vector3f>& points);
    std::vector<Eigen::Vector3f> Points() const;
    std::size_t PointCount() const;
    bool IsEmpty() const;
    void Clear();
    /// @brief Set points from an Nx3 vertex matrix (replaces current content).
    void SetFromVertices(const Eigen::MatrixXf& vertices);
    /// @brief Get all points as an Nx3 matrix.
    Eigen::MatrixXf ToVertices() const;
    void Voxelize(float voxelSize);
    void Downsample(float voxelSize);
    /// @brief Compute the centroid (center of mass) of the point cloud.
    Eigen::Vector3f Centroid() const;
    /// @brief Merge another point cloud into this one.
    void Merge(const UserCustomPointCloudModel& other);
    /// @brief Remove statistical outliers.
    /// @param k Number of neighbors for mean distance estimation.
    /// @param multiplier Standard deviation multiplier threshold.
    void RemoveStatisticalOutliers(std::size_t k, float multiplier = 1.0f);
    /// @brief Estimate per-point normals.
    /// @param k Number of neighbors used for normal estimation.
    /// @return Nx3 matrix of unit normals corresponding to Points() order.
    Eigen::MatrixXf ComputeNormals(std::size_t k = 12) const;

private:
    void EnsureModel();
    std::shared_ptr<UserCustomPointCloudDll> dll_;
    IModel* model_ = nullptr;
};
}  // namespace HsBa::Slicer

#endif  // !HSBA_SLICER_USER_CUSTOM_POINT_CLOUD_MODEL_HPP

#endif  // !HSBA_NO_DLL_LOADER
