#pragma once
#ifndef HSBA_SLICER_CGAL_MODEL_HPP
#define HSBA_SLICER_CGAL_MODEL_HPP

#include <string_view>
#include <Eigen/Dense>
#include <string>

#include <CGAL/Polygon_mesh_processing/corefinement.h>
#include <CGAL/Exact_integer.h>
#include <CGAL/Cartesian_converter.h>
#include <CGAL/Polyhedron_3.h>
#include <CGAL/Aff_transformation_3.h>
#include <CGAL/Nef_polyhedron_3.h>

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

        float Volume() const override;
        
        std::pair<Eigen::MatrixXf,Eigen::MatrixXi> TriangleMesh() const override; //get igl style trianglemesh

        friend CgalModel Union(const CgalModel& left, const CgalModel& right);
        friend CgalModel Intersection(const CgalModel& left, const CgalModel& right);
        friend CgalModel Difference(const CgalModel& left, const CgalModel& right);
        friend CgalModel Xor(const CgalModel& left, const CgalModel& right);

    private:
        CGAL::Polyhedron_3<EpicKernel> mesh_;
        std::string filename_;
        friend struct std::hash<CgalModel>;
    };

} // namespace HsBa::Slicer

template<>
struct std::hash<HsBa::Slicer::CgalModel>
{
    std::size_t operator()(const HsBa::Slicer::CgalModel& cgalmodel);
};

#endif // !HSBA_SLICER_CGAL_MODEL_HPP

