#pragma once
#ifndef HSBA_SLICER_IGLMODEL_HPP
#define HSBA_SLICER_IGLMODEL_HPP

#include "base/IModel.hpp"

#include <boost/functional/hash.hpp>
#include <Eigen/Dense>

namespace HsBa::Slicer
{
    class IglModel final : public IModel
    {
    public:
        IglModel() = default;
        IglModel(const Eigen::MatrixXf& vertices, const Eigen::MatrixXi& faces, bool calcNormals = true);
        IglModel(const Eigen::MatrixXf& vertices, const Eigen::MatrixXi& faces, const Eigen::MatrixXf& normals);
        IglModel(Eigen::MatrixXf&& vertices, Eigen::MatrixXi&& faces, bool calcNormals = true);
        IglModel(Eigen::MatrixXf&& vertices, Eigen::MatrixXi&& faces, Eigen::MatrixXf&& normals);
        IglModel(const IglModel& o) = default;
        IglModel& operator=(const IglModel& o) = default;
        IglModel(IglModel&& o) = default;
        IglModel& operator=(IglModel&& o) = default;
        ~IglModel() = default;
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

        void ComputeNormals();//compute face normals
        Eigen::MatrixXf ComputeVertexNormals() const;//compute vertex normals
        Eigen::MatrixXf ComputeFaceNormals() const;//compute face normals

        friend IglModel Union(const IglModel& left, const IglModel& right);
        friend IglModel Intersection(const IglModel& left, const IglModel& right);
        friend IglModel Difference(const IglModel& left, const IglModel& right);
        friend IglModel Xor(const IglModel& left, const IglModel& right);

    private:
        Eigen::MatrixXf vertices_ = Eigen::MatrixXf{};
        Eigen::MatrixXi faces_ = Eigen::MatrixXi{};
        Eigen::MatrixXf normals_ = Eigen::MatrixXf{};
        std::string fileName_;
        friend struct std::hash<IglModel>;
    };
}// namespace HsBa::Slicer

#if !eigen_std_hash //eigen_std_hash is defined in CMakeLists.txt for eigen matrix std::hash define in eigen3
template<>
    struct std::hash<Eigen::MatrixXf>
    {
        std::size_t operator()(const Eigen::MatrixXf& matrix) const noexcept;
    };

template<>
    struct ::std::hash<Eigen::MatrixXi>
    {
        std::size_t operator()(const Eigen::MatrixXi& matrix) const noexcept;
    };
#endif//eigen_std_hash

template<>
    struct std::hash<HsBa::Slicer::IglModel>
    {
        std::size_t operator()(const HsBa::Slicer::IglModel& model) const noexcept;
    };

#endif // HSBA_SLICER_IGLMODEL_HPP