/** @file UserCustomPointCloudModel.hpp
 * @brief User-supplied point cloud model loaded from an external DLL/SO plugin.
 * @author HsBa
 */
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
/**
 * @class IUserCustomPointCloud
 * @brief Abstract provider of C function pointers implementing a custom point cloud kernel.
 *
 * Each getter returns a raw C entry point (create/destroy/add/query/etc.) resolved from a
 * user plugin; the typedefs describe their signatures against the shared HsBaVector3f_t ABI.
 */
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
/**
 * @class UserCustomPointCloudDll
 * @brief Loads a user point cloud plugin DLL/SO and exposes its entry points via IUserCustomPointCloud.
 */
class UserCustomPointCloudDll final : public IUserCustomPointCloud
{
public:
    friend class UserCustomPointCloudModel;
    /// @brief Load the plugin at dllPath and resolve the added-function symbol named addedFunName.
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
    /// @brief Unload the plugin and release resolved symbols.
    ~UserCustomPointCloudDll();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/**
 * @class UserCustomPointCloudModel
 * @brief IModel implementation that delegates point cloud operations to a user plugin DLL/SO.
 *
 * Point-cloud API mirrors OpenVdbModel so plugins and built-in kernels are interchangeable.
 */
class UserCustomPointCloudModel : public IModel
{
public:
    /// @brief Construct an empty model (no plugin loaded yet).
    UserCustomPointCloudModel() = default;
    /// @brief Destroy the model and release the underlying plugin model.
    ~UserCustomPointCloudModel() override;
    /// @brief Load the plugin DLL/SO and its added-function symbol.
    void LoadDll(std::string_view dllPath, std::string_view addedFunName);
    /// @brief Unload the currently loaded plugin.
    void UnloadDll();
    /// @brief Load the model from a file.
    bool Load(std::string_view fileName) override;
    /// @brief Save the model to a file in the given format.
    bool Save(std::string_view fileName, const ModelFormat format) const override;

    /// @brief Translate the model.
    void Translate(const Eigen::Vector3f& translation) override;
    /// @brief Rotate the model.
    void Rotate(const Eigen::Quaternionf& rotation) override;
    /// @brief Uniformly scale the model.
    void Scale(const float scale) override;
    /// @brief Non-uniformly scale the model per axis.
    void Scale(const Eigen::Vector3f& scale) override;
    /// @brief Apply an isometry transform to the model.
    void Transform(const Eigen::Isometry3f& transform) override;
    /// @brief Apply a 4x4 matrix transform to the model.
    void Transform(const Eigen::Matrix4f& transform) override;
    /// @brief Apply an affine transform to the model.
    void Transform(const Eigen::Transform<float, 3, Eigen::Affine>& transform) override;
    /// @brief Get the axis-aligned bounding box of the model.
    void BoundingBox(Eigen::Vector3f& min, Eigen::Vector3f& max) const override;
    /// @brief Get the volume of the model.
    float Volume() const override;
    /// @brief Get the IGL-style triangle mesh of the model.
    std::pair<Eigen::MatrixXf, Eigen::MatrixXi> TriangleMesh() const override;

    // point cloud exits consistent with OpenVdbModel
    /// @brief Add a single point to the cloud.
    void AddPoint(const Eigen::Vector3f& point);
    /// @brief Add multiple points to the cloud.
    void AddPoints(const std::vector<Eigen::Vector3f>& points);
    /// @brief Return all points as a vector.
    std::vector<Eigen::Vector3f> Points() const;
    /// @brief Return the number of points.
    std::size_t PointCount() const;
    /// @brief Whether the cloud contains no points.
    bool IsEmpty() const;
    /// @brief Remove all points.
    void Clear();
    /// @brief Set points from an Nx3 vertex matrix (replaces current content).
    void SetFromVertices(const Eigen::MatrixXf& vertices);
    /// @brief Get all points as an Nx3 matrix.
    Eigen::MatrixXf ToVertices() const;
    /// @brief Voxelize the point cloud at the given voxel size.
    void Voxelize(float voxelSize);
    /// @brief Downsample the cloud using voxel grid filtering.
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
