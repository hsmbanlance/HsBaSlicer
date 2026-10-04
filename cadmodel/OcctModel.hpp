/** @file OcctModel.hpp
 * @brief OpenCascade (OCCT) BRep CAD model implementing the IModel interface.
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_OCCTMODEL_HPP
#define HSBA_SLICER_OCCTMODEL_HPP

#include <vector>

#include <Standard.hxx>
#include <Standard_Handle.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Shape.hxx>

#include "2D/FloatPolygons.hpp"
#include "OcctTypeAliases.hpp"
#include "base/IModel.hpp"

namespace HsBa::Slicer
{
/**
 * @class OcctModel
 * @brief OpenCascade (OCCT) BRep CAD model implementing the IModel interface.
 *
 * Wraps a TopoDS_Shape and provides loading/saving of STEP/IGES/VRML/BRep
 * files, affine transforms, bounding-box/volume/tessellation queries, boolean
 * operations, thick-solid (shell) operations and primitive/extrusion factories.
 */
class OcctModel : public IModel
{
public:
    /// Construct an empty model.
    OcctModel() = default;
    /// Construct a model from an OCCT shape (copy).
    OcctModel(const TopoDS_Shape& shape);
    /// Construct a model from an OCCT shape (move).
    OcctModel(TopoDS_Shape&& shape);
    OcctModel(const OcctModel& o) = default;   ///< Copy constructor.
    OcctModel& operator=(const OcctModel& o) = default;  ///< Copy assignment.
    OcctModel(OcctModel&& o) = default;        ///< Move constructor.
    OcctModel& operator=(OcctModel&& o) = default;       ///< Move assignment.
    ~OcctModel() = default;                   ///< Destructor.

    /// Add another model's shape into this model's compound.
    void AddShape(const OcctModel& o);
    /// Add another model's shape (moved) into this model's compound.
    void AddShape(OcctModel&& o);
    /// Add a raw OCCT shape into this model's compound.
    void AddShape(const TopoDS_Shape& o);
    /// Add a raw OCCT shape (moved) into this model's compound.
    void AddShape(TopoDS_Shape&& o);

    /// Load the model from a CAD file (STEP/IGES/VRML/BRep).
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

    /// Tessellate the shape and return a (vertices, faces) pair in libigl style.
    std::pair<Eigen::MatrixXf, Eigen::MatrixXi> TriangleMesh() const override;
    /// Get the signed volume of the model.
    float Volume() const override;

    /// Fuse all sub-shapes in the compound into a single shape.
    bool UnionAll();

    friend OcctModel Union(const OcctModel& left, const OcctModel& right);
    friend OcctModel Intersection(const OcctModel& left, const OcctModel& right);
    friend OcctModel Difference(const OcctModel& left, const OcctModel& right);
    friend OcctModel Xor(const OcctModel& left, const OcctModel& right);

    friend OcctModel ThickSolid(const OcctModel& model, float thickness);
    friend OcctModel ThickSolid(const OcctModel& model, const std::vector<std::vector<Eigen::Vector3f>>& faces,
                                float thickness);

    /// Create an axis-aligned box centered at the origin with the given size.
    static OcctModel CreateBox(const Eigen::Vector3f& size);
    /// Create a sphere of the given radius.
    static OcctModel CreateSphere(const float radius, const int subdivisions = 3);
    /// Create a cylinder of the given radius and height with the given segment count.
    static OcctModel CreateCylinder(const float radius, const float height, const int segments = 32);
    /// Create a cone of the given radius and height with the given segment count.
    static OcctModel CreateCone(const float radius, const float height, const int segments = 32);
    /// Create a torus with the given major/minor radii and segment counts.
    static OcctModel CreateTorus(const float majorRadius, const float minorRadius, const int majorSegments = 32,
                                 const int minorSegments = 16);
    /// Extrude a single closed polygon along the given direction into a solid.
    static OcctModel CreatePrime(const PolygonD& poly, const Eigen::Vector3f& direction);
    /// Extrude a set of polygons along the given direction into a solid.
    static OcctModel CreatePrime(const PolygonsD& paths, const Eigen::Vector3f& direction);

    friend struct std::hash<OcctModel>;

private:
    void ReadStep(const std::string& path);
    void ReadIGES(const std::string& path);
    void ReadVRML(const std::string& path);
    void ReadBRep(const std::string& path);
    bool WriteStep(const std::string& path) const;
    bool WriteIGES(const std::string& path) const;
    bool WriteVRML(const std::string& path) const;
    bool WriteBRep(const std::string& path) const;
    TopoDS_Shape shape_ = TopoDS_Shape{};
    std::string fileName_;
};
}  // namespace HsBa::Slicer

template <>
struct std::hash<HsBa::Slicer::OcctModel>
{
    std::size_t operator()(const HsBa::Slicer::OcctModel& model) const noexcept;
};
#endif  // HSBA_SLICER_OCCTMODEL_HPP