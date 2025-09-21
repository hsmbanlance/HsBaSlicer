#include "IglModel.hpp"

#include <igl/read_triangle_mesh.h>
#include <igl/writePLY.h>
#include <igl/writeOBJ.h>
#include <igl/writeSTL.h>
#include <igl/writeOFF.h>

#include <igl/per_face_normals.h>
#include <igl/per_vertex_normals.h>
#include <igl/vertex_triangle_adjacency.h>
#include <igl/per_edge_normals.h>
#include <igl/volume.h>

#include <igl/copyleft/cgal/mesh_boolean.h>

#include "base/error.hpp"
#include "base/encoding_convert.hpp"

namespace HsBa::Slicer
{
    IglModel::IglModel(const Eigen::MatrixXf& vertices, const Eigen::MatrixXi& faces, bool calcNormals)
        :vertices_(vertices),faces_(faces)
    {
        if (calcNormals) 
        {
            ComputeNormals();
        }
    }
    IglModel::IglModel(const Eigen::MatrixXf& vertices, const Eigen::MatrixXi& faces, const Eigen::MatrixXf& normals)
        :vertices_(vertices),faces_(faces),normals_(normals)
    {
    }

    IglModel::IglModel(Eigen::MatrixXf&& vertices, Eigen::MatrixXi&& faces, bool calcNormals)
        :vertices_(std::move(vertices)),faces_(std::move(faces))
    {
        if (calcNormals)
        {
            ComputeNormals();
        }
    }

    IglModel::IglModel(Eigen::MatrixXf&& vertices, Eigen::MatrixXi&& faces, Eigen::MatrixXf&& normals)
        :vertices_(std::move(vertices)),faces_(std::move(faces)),normals_(std::move(normals))
    {
    }

    bool IglModel::Load(std::string_view filename)
    {
        fileName_=filename;
        //convert to system coding path
        std::string filename_ansi = utf8_to_local(fileName_);
        return igl::read_triangle_mesh(filename_ansi, vertices_, faces_);
    }

    bool IglModel::Save(std::string_view filename, ModelFormat format) const
    {
        std::string filename_ansi = utf8_to_local(std::string{filename});
        if(IsMeshFormat(format))
        {
            switch (format)
            {
            case ModelFormat::BinarySTL:
                return igl::writeSTL(filename_ansi, vertices_, faces_,igl::FileEncoding::Binary);
            case ModelFormat::ASCIISTL:
                return igl::writeSTL(filename_ansi, vertices_, faces_,igl::FileEncoding::Ascii);
            case ModelFormat::OFF:
                return igl::writeOFF(filename_ansi, vertices_, faces_);
            case ModelFormat::OBJ:
                return igl::writeOBJ(filename_ansi, vertices_, faces_);
            case ModelFormat::ASCIIPLY:
                return igl::writePLY(filename_ansi, vertices_, faces_,igl::FileEncoding::Ascii);
            case ModelFormat::BinaryPLY:
                return igl::writePLY(filename_ansi, vertices_, faces_, igl::FileEncoding::Binary);
            default:
                throw NotSupportedError("Unsupported file format.");
            }
        }
        else
        {
            throw NotSupportedError("Unsupported file format.");
        }
    }

    void IglModel::Translate(const Eigen::Vector3f& translation)
    {
        vertices_ += translation;
    }
    void IglModel::Rotate(const Eigen::Quaternionf& rotation)
    {
        Eigen::Matrix3f rotationMatrix = rotation.toRotationMatrix();
        vertices_ = rotationMatrix * vertices_;
        if(normals_.cols() ==faces_.cols())
        {
            normals_ = rotationMatrix.transpose() * normals_;
        }
    }
    void IglModel::Scale(const float scale)
    {
        vertices_ *= scale;
    }
    void IglModel::Scale(const Eigen::Vector3f& scaleFactors)
    {
        vertices_ *= scaleFactors;
    }
    void IglModel::Transform(const Eigen::Isometry3f& transform)
    {
        vertices_ = (transform.matrix() * vertices_.colwise().homogeneous()).colwise().hnormalized();
        if(normals_.cols() == faces_.cols())
        {
            normals_ = transform.rotation().transpose() * normals_;
        }
    }
    void IglModel::Transform(const Eigen::Matrix4f& transform)
    {
        vertices_ = (transform * vertices_.colwise().homogeneous()).colwise().hnormalized();
        if(normals_.cols() == faces_.cols())
        {
            normals_ = transform.block<3,3>(0,0).transpose() * normals_;
        }
    }
    void IglModel::Transform(const Eigen::Transform<float, 3, Eigen::Affine>& transform)
    {
        vertices_ = (transform.matrix() * vertices_.colwise().homogeneous()).colwise().hnormalized();
        if(normals_.cols() == faces_.cols())
        {
            normals_ = transform.rotation().transpose() * normals_;
        }
    }

    void IglModel::BoundingBox(Eigen::Vector3f& min, Eigen::Vector3f& max) const
    {
        min = vertices_.colwise().minCoeff();
        max = vertices_.colwise().maxCoeff();
    }
    float IglModel::Volume() const
    {
        Eigen::MatrixXf v2(vertices_.rows() + 1, vertices_.cols());
        v2.topRows(vertices_.rows()) = vertices_;
        v2.bottomRows(1).setZero();
        Eigen::MatrixXi t(faces_.rows(), 4);
        t.leftCols(3) = faces_;
        t.rightCols(1).setConstant((int)vertices_.rows());
        Eigen::VectorXf vol;
        igl::volume(v2, t, vol);
        return abs(vol.sum());
    }

    void IglModel::ComputeNormals()
    {
        igl::per_face_normals(vertices_, faces_, normals_);
    }
    Eigen::MatrixXf IglModel::ComputeVertexNormals() const
    {
        Eigen::MatrixXf normals(vertices_.rows(), 3);
        igl::per_vertex_normals(vertices_, faces_, normals);
        return normals;
    }
    Eigen::MatrixXf IglModel::ComputeFaceNormals() const
    {
        Eigen::MatrixXf normals(faces_.rows(), 3);
        igl::per_face_normals(vertices_, faces_, normals);
        return normals;
    }


    std::pair<Eigen::MatrixXf,Eigen::MatrixXi> IglModel::TriangleMesh() const
    {
        Eigen::MatrixXf v=vertices_;
        Eigen::MatrixXi f=faces_;
        return std::make_pair(v,f);
    }
    IglModel Union(const IglModel& left, const IglModel& right)
    {
        Eigen::MatrixXf v;
        Eigen::MatrixXi f;
        igl::copyleft::cgal::mesh_boolean(left.vertices_, left.faces_, right.vertices_, right.faces_, igl::MESH_BOOLEAN_TYPE_UNION, v, f);
        return IglModel(v, f);
    }
    IglModel Intersection(const IglModel& left, const IglModel& right)
    {
        Eigen::MatrixXf v;
        Eigen::MatrixXi f;
        igl::copyleft::cgal::mesh_boolean(left.vertices_, left.faces_, right.vertices_, right.faces_, igl::MESH_BOOLEAN_TYPE_INTERSECT, v, f);
        return IglModel(v, f);
    }
    IglModel Difference(const IglModel& left, const IglModel& right)
    {
        Eigen::MatrixXf v;
        Eigen::MatrixXi f;
        igl::copyleft::cgal::mesh_boolean(left.vertices_, left.faces_, right.vertices_, right.faces_, igl::MESH_BOOLEAN_TYPE_MINUS, v, f);
        return IglModel(v, f);
    }
    IglModel Xor(const IglModel& left, const IglModel& right)
    {
        Eigen::MatrixXf v;
        Eigen::MatrixXi f;
        igl::copyleft::cgal::mesh_boolean(left.vertices_, left.faces_, right.vertices_, right.faces_, igl::MESH_BOOLEAN_TYPE_XOR, v, f);
        return IglModel(v, f);
    }
}// namespace HsBa::Slicer
