#pragma once
#ifndef HSBA_SLICER_OPENVDB_MODEL_HPP
#define HSBA_SLICER_OPENVDB_MODEL_HPP

#include <functional>
#include <utility>
#include <vector>

#include <Eigen/Core>
#include <openvdb/openvdb.h>

#include "base/IModel.hpp"

namespace HsBa::Slicer
{
    class OpenVdbModel : public IModel
    {
    public:
        OpenVdbModel();
        explicit OpenVdbModel(const std::string& file_path);
        /// @brief Construct from IGL-style vertices/faces pair (faces are ignored for point cloud).
        explicit OpenVdbModel(const std::pair<Eigen::MatrixXf, Eigen::MatrixXi>& mesh);
        /// @brief Construct from IGL-style vertices/faces pair (move version).
        explicit OpenVdbModel(std::pair<Eigen::MatrixXf, Eigen::MatrixXi>&& mesh);
        ~OpenVdbModel() override = default;

        /// @brief Assign from IGL-style vertices/faces pair (faces are ignored).
        OpenVdbModel& operator=(const std::pair<Eigen::MatrixXf, Eigen::MatrixXi>& mesh);
        /// @brief Convert to IGL-style vertices/faces pair.
        explicit operator std::pair<Eigen::MatrixXf, Eigen::MatrixXi>() const;

        bool Load(std::string_view fileName) override;
        bool Save(std::string_view fileName, const ModelFormat format) const override;

        void Translate(const Eigen::Vector3f& translation) override;
        void Rotate(const Eigen::Quaternionf& rotation) override;
        void Scale(const float scale) override;
        void Scale(const Eigen::Vector3f& scale) override;
        void Transform(const Eigen::Isometry3f& transform) override;
        void Transform(const Eigen::Matrix4f& transform) override;
        void Transform(const Eigen::Transform<float, 3, Eigen::Affine>& transform) override;
        void BoundingBox(Eigen::Vector3f& min, Eigen::Vector3f& max) const override;
        float Volume() const override;
        std::pair<Eigen::MatrixXf, Eigen::MatrixXi> TriangleMesh() const override;

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
        /// @brief Convert to IGL-style pair (vertices Nx3, faces Mx3) via OpenVDB meshing.
        std::pair<Eigen::MatrixXf, Eigen::MatrixXi> ToVerticesFaces() const;
        /// @brief Generate triangle mesh from point cloud using OpenVDB level set reconstruction.
        /// @param voxelSize Voxel size for the level set grid (0 = auto estimate).
        /// @param particleRadius Particle radius for sphere rasterization (0 = auto estimate).
        /// @return IGL-style pair (vertices Nx3, faces Mx3 triangles).
        std::pair<Eigen::MatrixXf, Eigen::MatrixXi> GenerateMesh(float voxelSize = 0.0f, float particleRadius = 0.0f) const;

        void Voxelize(float voxelSize);
        std::vector<Eigen::Vector3f> NearestNeighbor(const Eigen::Vector3f& query) const;
        std::vector<Eigen::Vector3f> KNN(const Eigen::Vector3f& query, std::size_t k) const;
        std::vector<Eigen::Vector3f> Filter(const std::function<bool(const Eigen::Vector3f&)>& predicate) const;
        std::vector<Eigen::Vector3f> RadiusSearch(const Eigen::Vector3f& center, float radius) const;
        void Downsample(float voxelSize);
        std::vector<Eigen::Vector3f> VoxelCenters(float voxelSize) const;

        /// @brief Compute the centroid (center of mass) of the point cloud.
        Eigen::Vector3f Centroid() const;
        /// @brief Merge another point cloud into this one.
        void Merge(const OpenVdbModel& other);
        /// @brief Remove statistical outliers.
        /// @param k Number of neighbors for mean distance estimation.
        /// @param multiplier Standard deviation multiplier threshold.
        void RemoveStatisticalOutliers(std::size_t k, float multiplier = 1.0f);
        /// @brief Estimate per-point normals using KNN covariance analysis.
        /// @param k Number of neighbors used for normal estimation.
        /// @return Nx3 matrix of unit normals corresponding to Points() order.
        Eigen::MatrixXf ComputeNormals(std::size_t k = 12) const;

    private:
        openvdb::Vec3fGrid::Ptr grid_;
        std::string file_name_;
    };
}  // namespace HsBa::Slicer

#endif  // HSBA_SLICER_OPENVDB_MODEL_HPP