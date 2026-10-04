/** @file IglModel.hpp
 * @brief libigl-backed triangle mesh model implementing the IModel interface.
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_IGLMODEL_HPP
#define HSBA_SLICER_IGLMODEL_HPP

#include "base/IModel.hpp"

#include <Eigen/Dense>
#include <boost/functional/hash.hpp>

#include "2D/FloatPolygons.hpp"

namespace HsBa::Slicer
{
/**
 * @class IglModel
 * @brief libigl-backed triangle mesh model implementing the IModel interface.
 *
 * Stores vertices, faces and optional normals as Eigen matrices and provides
 * loading/saving, affine transforms, bounding-box/volume queries, normal
 * computation, boolean operations and primitive/extrusion factories.
 */
class IglModel final : public IModel
{
public:
    /// Construct an empty model.
    IglModel() = default;
    /**
     * @brief Construct a model from a vertex/face mesh.
     * @param vertices Per-vertex positions (N x 3).
     * @param faces Triangle vertex indices (M x 3).
     * @param calcNormals Whether to compute face normals immediately.
     */
    IglModel(const Eigen::MatrixXf& vertices, const Eigen::MatrixXi& faces, bool calcNormals = true);
    /**
     * @brief Construct a model from a vertex/face mesh with explicit normals.
     * @param vertices Per-vertex positions (N x 3).
     * @param faces Triangle vertex indices (M x 3).
     * @param normals Normals to associate with the mesh.
     */
    IglModel(const Eigen::MatrixXf& vertices, const Eigen::MatrixXi& faces, const Eigen::MatrixXf& normals);
    /// Move-construct a model from vertex/face meshes, optionally computing normals.
    IglModel(Eigen::MatrixXf&& vertices, Eigen::MatrixXi&& faces, bool calcNormals = true);
    /// Move-construct a model from vertex/face meshes with explicit normals.
    IglModel(Eigen::MatrixXf&& vertices, Eigen::MatrixXi&& faces, Eigen::MatrixXf&& normals);
    IglModel(const IglModel& o) = default;   ///< Copy constructor.
    IglModel& operator=(const IglModel& o) = default;  ///< Copy assignment.
    IglModel(IglModel&& o) = default;        ///< Move constructor.
    IglModel& operator=(IglModel&& o) = default;       ///< Move assignment.
    ~IglModel() = default;                   ///< Destructor.

    /// Load the model from a file (STL/OBJ/PLY/OFF and other supported mesh formats).
    bool Load(std::string_view fileName) override;
    /// Save the model to a file in the given format.
    bool Save(std::string_view fileName, const ModelFormat format) const override;

    /// Translate the model by the given vector.
    void Translate(const Eigen::Vector3f& translation) override;
    /// Rotate the model by the given quaternion.
    void Rotate(const Eigen::Quaternionf& rotation) override;
    /// Uniformly scale the model by the given factor.
    void Scale(const float scale) override;
    /// Non-uniformly scale the model by the per-axis factors.
    void Scale(const Eigen::Vector3f& scale) override;
    /// Apply an isometry (rigid) transform to the model.
    void Transform(const Eigen::Isometry3f& transform) override;
    /// Apply a 4x4 homogeneous transform to the model.
    void Transform(const Eigen::Matrix4f& transform) override;
    /// Apply an affine transform to the model.
    void Transform(const Eigen::Transform<float, 3, Eigen::Affine>& transform) override;

    /// Get the axis-aligned bounding box of the model.
    void BoundingBox(Eigen::Vector3f& min,
                     Eigen::Vector3f& max) const override;

    /// Get the mesh as a (vertices, faces) pair in libigl style.
    std::pair<Eigen::MatrixXf, Eigen::MatrixXi> TriangleMesh() const override;
    /// Get the signed volume of the model.
    float Volume() const override;

    /// Recompute and store face normals for the current mesh.
    void ComputeNormals();
    /// Compute and return per-vertex normals.
    Eigen::MatrixXf ComputeVertexNormals() const;
    /// Compute and return per-face normals.
    Eigen::MatrixXf ComputeFaceNormals() const;

    friend IglModel Union(const IglModel& left, const IglModel& right);
    friend IglModel Intersection(const IglModel& left, const IglModel& right);
    friend IglModel Difference(const IglModel& left, const IglModel& right);
    friend IglModel Xor(const IglModel& left, const IglModel& right);

    /// Create an axis-aligned box centered at the origin with the given size.
    static IglModel CreateBox(const Eigen::Vector3f& size);
    /// Create a UV-sphere of the given radius with the given subdivision level.
    static IglModel CreateSphere(const float radius, const int subdivisions = 3);
    /// Create a cylinder of the given radius and height with the given segment count.
    static IglModel CreateCylinder(const float radius, const float height, const int segments = 32);
    /// Create a cone of the given radius and height with the given segment count.
    static IglModel CreateCone(const float radius, const float height, const int segments = 32);
    /// Create a torus with the given major/minor radii and segment counts.
    static IglModel CreateTorus(const float majorRadius, const float minorRadius, const int majorSegments = 32,
                                const int minorSegments = 16);

    /// Extrude a single closed polygon along the given direction into a solid.
    static IglModel CreatePrime(const PolygonD& poly, const Eigen::Vector3f& direction);

    /// Extrude a set of polygons along the given direction into a solid.
    static IglModel CreatePrime(const PolygonsD& paths, const Eigen::Vector3f& direction);


private:
    Eigen::MatrixXf vertices_ = Eigen::MatrixXf{};
    Eigen::MatrixXi faces_ = Eigen::MatrixXi{};
    Eigen::MatrixXf normals_ = Eigen::MatrixXf{};
    std::string fileName_;
    friend struct std::hash<IglModel>;
};
}  // namespace HsBa::Slicer

#endif  // HSBA_SLICER_IGLMODEL_HPP