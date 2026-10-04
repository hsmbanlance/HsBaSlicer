/** @file CgalModel.hpp
 * @brief CGAL-backed polyhedral mesh model implementing the IModel interface.
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_CGAL_MODEL_HPP
#define HSBA_SLICER_CGAL_MODEL_HPP

#include <Eigen/Dense>
#include <string>
#include <string_view>
#include <vector>

#include <CGAL/Aff_transformation_3.h>
#include <CGAL/Cartesian_converter.h>
#include <CGAL/Exact_integer.h>
#include <CGAL/Nef_polyhedron_3.h>
#include <CGAL/Polygon_mesh_processing/corefinement.h>
#include <CGAL/Polyhedron_3.h>

#include "2D/FloatPolygons.hpp"
#include "base/IModel.hpp"

namespace HsBa::Slicer
{
/**
 * @class CgalModel
 * @brief CGAL-backed polyhedral mesh model implementing the IModel interface.
 *
 * Wraps a CGAL Polyhedron_3 and provides loading/saving, affine transforms,
 * bounding-box/volume queries, Nef-based boolean operations, primitive and
 * extrusion factories, plus surface geodesic/curve/helix operations.
 */
class CgalModel final : public IModel
{
public:
    using EpicKernel = CGAL::Exact_predicates_inexact_constructions_kernel;  ///< CGAL exact-predicates inexact-constructions kernel.
    using Point_3 = typename EpicKernel::Point_3;                            ///< CGAL 3D point type.
    using Vector_3 = typename EpicKernel::Vector_3;                          ///< CGAL 3D vector type.
    using Affine_3 = CGAL::Aff_transformation_3<EpicKernel>;                 ///< CGAL 3D affine transformation.
    using Polyhedron_3 = CGAL::Polyhedron_3<EpicKernel>;                     ///< CGAL polyhedron surface mesh.
    using Nef_Polyheron_3 = CGAL::Nef_polyhedron_3<EpicKernel>;              ///< CGAL Nef polyhedron for boolean ops.
    CgalModel() = default;                    ///< Construct an empty model.
    ~CgalModel() = default;                   ///< Destructor.
    CgalModel(const CgalModel&) = default;    ///< Copy constructor.
    CgalModel(CgalModel&&) = default;         ///< Move constructor.
    CgalModel& operator=(const CgalModel&) = default;  ///< Copy assignment.
    CgalModel& operator=(CgalModel&&) = default;       ///< Move assignment.
    /// Construct a model from an existing CGAL polyhedron.
    CgalModel(const Polyhedron_3& o);
    /// Construct a model from a vertex/face mesh.
    CgalModel(const Eigen::MatrixXf& v, const Eigen::MatrixXi& f);

    /// Load the model from a file (STL/OFF and other supported mesh formats).
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

    /// Get the signed volume of the model.
    float Volume() const override;

    /// Get the mesh as a (vertices, faces) pair in libigl style.
    std::pair<Eigen::MatrixXf, Eigen::MatrixXi> TriangleMesh() const override;

    friend CgalModel Union(const CgalModel& left, const CgalModel& right);
    friend CgalModel Intersection(const CgalModel& left, const CgalModel& right);
    friend CgalModel Difference(const CgalModel& left, const CgalModel& right);
    friend CgalModel Xor(const CgalModel& left, const CgalModel& right);

    /// Create an axis-aligned box centered at the origin with the given size.
    static CgalModel CreateBox(const Eigen::Vector3f& size);
    /// Create a UV-sphere of the given radius with the given subdivision level.
    static CgalModel CreateSphere(const float radius, const int subdivisions = 3);
    /// Create a cylinder of the given radius and height with the given segment count.
    static CgalModel CreateCylinder(const float radius, const float height, const int segments = 32);
    /// Create a cone of the given radius and height with the given segment count.
    static CgalModel CreateCone(const float radius, const float height, const int segments = 32);
    /// Create a torus with the given major/minor radii and segment counts.
    static CgalModel CreateTorus(const float majorRadius, const float minorRadius, const int majorSegments = 32,
                                 const int minorSegments = 16);
    /// Extrude a single closed polygon along the given direction into a solid.
    static CgalModel CreatePrime(const PolygonD& paths, const Eigen::Vector3f& direction);
    /// Extrude a set of polygons along the given direction into a solid.
    static CgalModel CreatePrime(const PolygonsD& paths, const Eigen::Vector3f& direction);

    // ========== Surface geodesic / curve / helix operations ==========

    /** @brief Compute the shortest geodesic path on the surface between two points.
     * @param source The starting point on the surface.
     * @param target The ending point on the surface.
     * @return A sequence of 3D points forming the geodesic path on the surface.
     */
    std::vector<Eigen::Vector3f> GeodesicPath(const Eigen::Vector3f& source, const Eigen::Vector3f& target) const;

    /** @brief Compute geodesic distances from a source point to all mesh vertices.
     * @param source The source point on the surface.
     * @return A vector of geodesic distances, one per mesh vertex (same order as TriangleMesh vertices).
     */
    std::vector<float> GeodesicDistance(const Eigen::Vector3f& source) const;

    /** @brief Project a 3D point onto the closest position on the mesh surface.
     * @param point The query point in 3D space.
     * @return The closest point on the mesh surface.
     */
    Eigen::Vector3f ProjectPointOnSurface(const Eigen::Vector3f& point) const;

    /** @brief Generate a spiral path on the mesh surface around a given axis.
     * @param axisOrigin The origin of the spiral axis.
     * @param axisDirection The direction of the spiral axis (normalized internally).
     * @param turns Number of full turns of the spiral.
     * @param samplesPerTurn Number of sample points per full turn.
     * @param startRadius Starting radius from the axis.
     * @param endRadius Ending radius from the axis.
     * @return A sequence of 3D points forming the spiral on the surface.
     */
    std::vector<Eigen::Vector3f> SurfaceSpiral(const Eigen::Vector3f& axisOrigin, const Eigen::Vector3f& axisDirection,
                                               float turns, int samplesPerTurn = 64, float startRadius = 0.0f,
                                               float endRadius = -1.0f) const;

    /** @brief Generate a helix path on the mesh surface around a given axis.
     * @param axisOrigin The origin of the helix axis.
     * @param axisDirection The direction of the helix axis (normalized internally).
     * @param turns Number of full turns of the helix.
     * @param pitch Axial distance per full turn.
     * @param radius Radius of the helix from the axis.
     * @param samplesPerTurn Number of sample points per full turn.
     * @return A sequence of 3D points forming the helix on the surface.
     */
    std::vector<Eigen::Vector3f> SurfaceHelix(const Eigen::Vector3f& axisOrigin, const Eigen::Vector3f& axisDirection,
                                              float turns, float pitch, float radius, int samplesPerTurn = 64) const;

private:
    CGAL::Polyhedron_3<EpicKernel> mesh_;
    std::string filename_;
    friend struct std::hash<CgalModel>;
};

}  // namespace HsBa::Slicer

template <>
struct std::hash<HsBa::Slicer::CgalModel>
{
    std::size_t operator()(const HsBa::Slicer::CgalModel& cgalmodel);
};

#endif  // !HSBA_SLICER_CGAL_MODEL_HPP
