/** @file IModel.hpp
 * @brief A header file containing the definition of the IModel interface for the HsBa Slicer project.
 * This file defines the IModel interface, which represents a 3D model in the HsBa Slicer project. The interface provides methods for loading and saving models, applying transformations (translation, rotation, scaling), calculating bounding boxes and volumes, and retrieving triangle mesh data. The IModel interface serves as a base class for different types of 3D models that can be used in the slicing process.
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_IModel_HPP
#define HSBA_SLICER_IModel_HPP

#include <cstdint>
#include <string_view>

#include <Eigen/Core>
#include <Eigen/Geometry>

extern "C"
{
    // defined for C interface

    /** @brief A 2D vector with float components. */
    typedef struct HsBaVector2f
    {
        float x;
        float y;
    } HsBaVector2f_t;
    /** @brief A 3D vector with float components. */
    typedef struct HsBaVector3f
    {
        float x;
        float y;
        float z;
    } HsBaVector3f_t;
    /** @brief A 4D vector with float components. */
    typedef struct HsBaQuaternionf
    {
        float x;
        float y;
        float z;
        float w;
    } HsBaQuaternionf_t;
    /** @brief A 4x4 matrix with float components. */
    typedef struct HsBaMatrix4f
    {
        float m[4][4];
    } HsBaMatrix4f_t;
    /** @brief A 2D polygon with float components. */
    typedef struct HsBaPoly2D
    {
        HsBaVector2f_t* vertices;
        size_t vertexCount;
    } HsBaPoly2D_t;
}

namespace HsBa::Slicer
{
/** @brief An enumeration representing different model formats. */
enum class ModelFormat : uint32_t
{
    // mesh
    /** @brief Unknown PLY format. */
    UnknownPLY = 0,
    /** @brief ASCII PLY format. */
    ASCIIPLY,
    /** @brief Binary PLY format. */
    BinaryPLY,
    /** @brief OBJ format. */
    OBJ,
    /** @brief Unknown STL format. */ 
    UnknownSTL,
    /** @brief ASCII STL format. */
    BinarySTL,
    /** @brief Binary STL format. */
    ASCIISTL,
    /** @brief OFF format. */
    OFF,
    // Brep
    /** @brief VRML format. */
    VRML = 10,
    /** @brief STEP format. */
    STEP,
    /** @brief IGES format. */
    IGES,
    // csg
    //  SolidWorksPart,
    /** @brief SLDPRT is the file extension for SolidWorks part files. */
    SLDPRT = 30,
    // CATIAPart,
    /** @brief CATPART is the file extension for CATIA part files. */
    CATPART,
    // point cloud
    /** @brief XYZ format. */
    XYZ = 40,
    // Unknown
    Unknown = 100
};
/** @brief An interface representing a 3D model in the HsBa Slicer project. */
class IModel
{
public:
    virtual ~IModel() = default;
    /** @brief Load the model from a file. */
    virtual bool Load(std::string_view fileName) = 0;                                  // load the model from a file
    /** @brief Save the model to a file. */
    virtual bool Save(std::string_view fileName, const ModelFormat format) const = 0;  // save the model to a file

    /** @brief Translate the model. */
    virtual void Translate(const Eigen::Vector3f& translation) = 0;  // translate the model
    /** @brief Rotate the model. */
    virtual void Rotate(const Eigen::Quaternionf& rotation) = 0;     // rotate the model
    /** @brief Scale the model. */
    virtual void Scale(const float scale) = 0;
    virtual void Scale(const Eigen::Vector3f& scale) = 0;                                    // scale the model
    /** @brief Transform the model. */
    virtual void Transform(const Eigen::Isometry3f& transform) = 0;                          // transform the model
    virtual void Transform(const Eigen::Matrix4f& transform) = 0;                            // transform the model
    virtual void Transform(const Eigen::Transform<float, 3, Eigen::Affine>& transform) = 0;  // transform the model

    /** @brief Get the axis-aligned bounding box of the model. */
    virtual void BoundingBox(Eigen::Vector3f& min,
                             Eigen::Vector3f& max) const = 0;  // get the AA bounding box of the model
    /** @brief Get the volume of the model. */
    virtual float Volume() const = 0;                          // get the volume of the model
    /** @brief Get the triangle mesh representation of the model. */
    virtual std::pair<Eigen::MatrixXf, Eigen::MatrixXi> TriangleMesh() const = 0;  // get igl style trianglemesh
};
}  // namespace HsBa::Slicer

#include <magic_enum/magic_enum.hpp>

template <>
struct magic_enum::customize::enum_range<HsBa::Slicer::ModelFormat>
{
    static constexpr int min = static_cast<int>(HsBa::Slicer::ModelFormat::UnknownPLY);
    static constexpr int max = static_cast<int>(HsBa::Slicer::ModelFormat::Unknown);
};

#endif  // HSBA_SLICER_IModel_HPP