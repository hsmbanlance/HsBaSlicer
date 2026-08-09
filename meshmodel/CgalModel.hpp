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
class CgalModel final : public IModel
{
public:
    using EpicKernel = CGAL::Exact_predicates_inexact_constructions_kernel;
    using Point_3 = typename EpicKernel::Point_3;
    using Vector_3 = typename EpicKernel::Vector_3;
    using Affine_3 = CGAL::Aff_transformation_3<EpicKernel>;
    using Polyhedron_3 = CGAL::Polyhedron_3<EpicKernel>;
    using Nef_Polyheron_3 = CGAL::Nef_polyhedron_3<EpicKernel>;
    CgalModel() = default;
    ~CgalModel() = default;
    CgalModel(const CgalModel&) = default;
    CgalModel(CgalModel&&) = default;
    CgalModel& operator=(const CgalModel&) = default;
    CgalModel& operator=(CgalModel&&) = default;
    CgalModel(const Polyhedron_3& o);
    CgalModel(const Eigen::MatrixXf& v, const Eigen::MatrixXi& f);

    bool Load(std::string_view fileName) override;                                  // load the model from a file
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

    float Volume() const override;

    std::pair<Eigen::MatrixXf, Eigen::MatrixXi> TriangleMesh() const override;  // get igl style trianglemesh

    friend CgalModel Union(const CgalModel& left, const CgalModel& right);
    friend CgalModel Intersection(const CgalModel& left, const CgalModel& right);
    friend CgalModel Difference(const CgalModel& left, const CgalModel& right);
    friend CgalModel Xor(const CgalModel& left, const CgalModel& right);

    static CgalModel CreateBox(const Eigen::Vector3f& size);
    static CgalModel CreateSphere(const float radius, const int subdivisions = 3);
    static CgalModel CreateCylinder(const float radius, const float height, const int segments = 32);
    static CgalModel CreateCone(const float radius, const float height, const int segments = 32);
    static CgalModel CreateTorus(const float majorRadius, const float minorRadius, const int majorSegments = 32,
                                 const int minorSegments = 16);
    static CgalModel CreatePrime(const PolygonD& paths, const Eigen::Vector3f& direction);
    static CgalModel CreatePrime(const PolygonsD& paths, const Eigen::Vector3f& direction);

    // ========== Surface geodesic / curve / helix operations ==========

    /** @brief Compute the shortest geodesic path on the surface between two points.
     * @param source The starting point on the surface.
     * @param target The ending point on the surface.
     * @return A sequence of 3D points forming the geodesic path on the surface.
     */
    std::vector<Eigen::Vector3f> GeodesicPath(const Eigen::Vector3f& source,
                                              const Eigen::Vector3f& target) const;

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
    std::vector<Eigen::Vector3f> SurfaceSpiral(const Eigen::Vector3f& axisOrigin,
                                               const Eigen::Vector3f& axisDirection, float turns,
                                               int samplesPerTurn = 64, float startRadius = 0.0f,
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
    std::vector<Eigen::Vector3f> SurfaceHelix(const Eigen::Vector3f& axisOrigin,
                                              const Eigen::Vector3f& axisDirection, float turns, float pitch,
                                              float radius, int samplesPerTurn = 64) const;

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
