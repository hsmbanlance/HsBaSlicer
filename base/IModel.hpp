#pragma once
#ifndef HSBA_SLICER_IModel_HPP
#define HSBA_SLICER_IModel_HPP

#include <string_view>
#include <cstdint>

#include <Eigen/Core>
#include <Eigen/Geometry>

extern "C"
{
    // defined for C interface

    typedef struct HsBaVector2f
    {
        float x;
        float y;
	} HsBaVector2f_t;
    typedef struct HsBaVector3f
    {
        float x;
        float y;
        float z;
	} HsBaVector3f_t;
    typedef struct HsBaQuaternionf
    {
        float x;
        float y;
        float z;
        float w;
	} HsBaQuaternionf_t;
    typedef struct HsBaMatrix4f
    {
        float m[4][4];
	} HsBaMatrix4f_t;
    typedef struct HsBaPoly2D
    {
		HsBaVector2f_t* vertices;
		size_t vertexCount;
    } HsBaPoly2D_t;
}

namespace HsBa::Slicer
{
    enum class ModelFormat : uint32_t
    {
        //mesh
        UnknownPLY,
        ASCIIPLY,
        BinaryPLY,
        OBJ,
        UnknownSTL,
        BinarySTL,
        ASCIISTL,
        OFF,
        //Brep
        VRML,
        STEP,
        IGES,
        //csg

        //point cloud
        XYZ,
        //Unknown
        Unknown
    };
    class IModel
    {
    public:
        virtual ~IModel() = default;
        
        virtual bool Load(std::string_view fileName) = 0; // load the model from a file
        virtual bool Save(std::string_view fileName,const ModelFormat format) const = 0; // save the model to a file

        virtual void Translate(const Eigen::Vector3f& translation) = 0; // translate the model
        virtual void Rotate(const Eigen::Quaternionf& rotation) = 0; // rotate the model
        virtual void Scale(const float scale) = 0;
        virtual void Scale(const Eigen::Vector3f& scale) = 0; // scale the model
        virtual void Transform(const Eigen::Isometry3f& transform) = 0; // transform the model
        virtual void Transform(const Eigen::Matrix4f& transform) = 0; // transform the model
        virtual void Transform(const Eigen::Transform<float, 3, Eigen::Affine>& transform) = 0; // transform the model

        virtual void BoundingBox(Eigen::Vector3f& min, Eigen::Vector3f& max) const = 0; // get the AA bounding box of the model
        virtual float Volume() const = 0; // get the volume of the model
        
        virtual std::pair<Eigen::MatrixXf,Eigen::MatrixXi> TriangleMesh() const = 0; //get igl style trianglemesh
    };
}// namespace HsBa::Slicer

#endif // HSBA_SLICER_IModel_HPP