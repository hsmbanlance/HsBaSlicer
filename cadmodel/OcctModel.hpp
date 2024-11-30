#pragma once
#ifndef HSBA_SLICER_OCCTMODEL_HPP
#define HSBA_SLICER_OCCTMODEL_HPP

#include <vector>

#include <Standard.hxx>
#include <Standard_Handle.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Compound.hxx>

#include "base/IModel.hpp"

namespace HsBa::Slicer 
{
    class OcctModel : public IModel
    {
    public:
        OcctModel() = default;
        OcctModel(const TopoDS_Shape& shape);
        OcctModel(TopoDS_Shape&& shape);
        OcctModel(const OcctModel& o) = default;
        OcctModel& operator=(const OcctModel& o) = default;
        OcctModel(OcctModel&& o) = default;
        OcctModel& operator=(OcctModel&& o) = default;
        ~OcctModel() = default;
        void AddShape(const OcctModel& o);
        void AddShape(OcctModel&& o);
        void AddShape(const TopoDS_Shape& o);
        void AddShape(TopoDS_Shape&& o);

        bool Load(std::string_view fileName) override; // load the model from a file
        bool Save(std::string_view fileName,const ModelFormat format) const override; // save the model to a file

        void Translate(const Eigen::Vector3f& translation) override; // translate the model
        void Rotate(const Eigen::Quaternionf& rotation) override; // rotate the model
        void Scale(const float scale) override;
        void Scale(const Eigen::Vector3f& scale) override; // scale the model
        void Transform(const Eigen::Isometry3f& transform) override; // transform the model
        void Transform(const Eigen::Matrix4f& transform) override; // transform the model
        void Transform(const Eigen::Transform<float, 3, Eigen::Affine>& transform) override; // transform the model

        void BoundingBox(Eigen::Vector3f& min, Eigen::Vector3f& max) const override; // get the AA bounding box of the model
        
        std::pair<Eigen::MatrixXf,Eigen::MatrixXi> TriangleMesh() const override; //get igl style trianglemesh
        float Volume() const override; //get the volume of the model

        bool UnionAll();

        static OcctModel MakeBox(const Eigen::Vector3f& p1, const Eigen::Vector3f& p2);
        static OcctModel MakeBox(const Eigen::Vector3f& p1, float x, float y, float z);
        static OcctModel MakeSphere(const Eigen::Vector3f& o, float r);
        static OcctModel MakeCone(float r1, float r2, float h);
        static OcctModel MakeTorus(float r1, float r2, float angle);

        friend OcctModel Union(const OcctModel& left, const OcctModel& right);
        friend OcctModel Intersection(const OcctModel& left, const OcctModel& right);
        friend OcctModel Difference(const OcctModel& left, const OcctModel& right);
        friend OcctModel Xor(const OcctModel& left, const OcctModel& right);

        friend OcctModel ThickSolid(const OcctModel& model, float thickness);
        friend OcctModel ThickSolid(const OcctModel& model, const std::vector<std::vector<Eigen::Vector3f>>& faces, float thickness);

        friend struct std::hash<OcctModel>;

    private:
        void ReadStep(const std::string& path);
        void ReadIGES(const std::string& path);
        bool WriteStep(const std::string& path) const;
        bool WriteIGES(const std::string& path) const;
        TopoDS_Shape shape_ = TopoDS_Shape{};
        std::string fileName_;
    };
}// namespace HsBa::Slicer 

template<>
struct std::hash<HsBa::Slicer::OcctModel>{
    std::size_t operator()(const HsBa::Slicer::OcctModel& model) const noexcept;
};
#endif // HSBA_SLICER_OCCTMODEL_HPP