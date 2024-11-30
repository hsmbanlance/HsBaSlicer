#include "CgalModel.hpp"

#include <CGAL/IO/io.h>
#include <CGAL/Polyhedron_3.h>
#include <CGAL/boost/graph/IO/polygon_mesh_io.h>
#include <CGAL/polygon_mesh_processing.h>
#include <CGAL/Boolean_set_operations_2.h>

#include <igl/copyleft/cgal/polyhedron_to_mesh.h>
#include <igl/copyleft/cgal/mesh_to_polyhedron.h>

#include "base/encoding_convert.hpp"
#include "base/error.hpp"


namespace HsBa::Slicer
{
    CgalModel::CgalModel(const Polyhedron_3& o)
        :mesh_{ o } {}

    CgalModel::CgalModel(const Eigen::MatrixXf& v, const Eigen::MatrixXi& f)
    {
        igl::copyleft::cgal::mesh_to_polyhedron(v, f, mesh_);
    }

    bool CgalModel::Load(std::string_view filePath)
    {
        filename_=filePath;
        std::string filepath_ansi=utf8_to_local(filename_);
        return CGAL::IO::read_polygon_mesh(filepath_ansi, mesh_);
    }
    bool CgalModel::Save(std::string_view fileName, const ModelFormat format) const
    {
        std::string filepath_ansi = utf8_to_local(std::string{fileName});
        if (IsMeshFormat(format))
        {
            switch (format)
            {
            case ModelFormat::BinarySTL:
                return CGAL::IO::write_STL(filepath_ansi, mesh_, CGAL::parameters::use_binary_mode(true));
            case ModelFormat::ASCIISTL:
                return CGAL::IO::write_STL(filepath_ansi, mesh_, CGAL::parameters::use_binary_mode(false));
            case ModelFormat::BinaryPLY:
                return CGAL::IO::write_PLY(filepath_ansi, mesh_, CGAL::parameters::use_binary_mode(true));
            case ModelFormat::ASCIIPLY:
                return CGAL::IO::write_PLY(filepath_ansi, mesh_, CGAL::parameters::use_binary_mode(false));
            case ModelFormat::OBJ:
                return CGAL::IO::write_OBJ(filepath_ansi, mesh_);
            case ModelFormat::OFF:
                return CGAL::IO::write_OFF(filepath_ansi, mesh_);
            default:
                throw NotSupportedError("Unsupported file format.");
            }
        }
        throw NotSupportedError("Unsupported file format.");
    }
    void CgalModel::Translate(const Eigen::Vector3f& translation)
    {
        Affine_3 tran(CGAL::Translation(), Vector_3{ translation.x(),translation.y(),translation.z() });
        CGAL::Polygon_mesh_processing::transform(tran, mesh_);
    }

    void CgalModel::Rotate(const Eigen::Quaternionf& rotation)
    {
        Eigen::Matrix3f rotationMatrix = rotation.toRotationMatrix();
        Affine_3 tran(rotationMatrix(0, 0), rotationMatrix(0, 1), rotationMatrix(0, 2),0,
            rotationMatrix(1, 0), rotationMatrix(1, 1), rotationMatrix(1, 2),0,
            rotationMatrix(2, 0), rotationMatrix(2, 1), rotationMatrix(2, 2),0
        );
        CGAL::Polygon_mesh_processing::transform(tran, mesh_);
    }

    void CgalModel::Scale(const float scale)
    {
        Affine_3 tran(CGAL::Scaling(), scale);
        CGAL::Polygon_mesh_processing::transform(tran, mesh_);
    }

    void CgalModel::Scale(const Eigen::Vector3f& scale)
    {
        Affine_3 tran(scale.x(),0,0,0,
            0,scale.y(),0,0,
            0,0,scale.z(),0
        );
        CGAL::Polygon_mesh_processing::transform(tran, mesh_);
    }

    void CgalModel::Transform(const Eigen::Isometry3f& transform)
    {
        Affine_3 tran(transform(0,0),transform(0,1),transform(0,2),transform(0,3),
            transform(1,0),transform(1,1),transform(1,2),transform(1,3),
            transform(2,0),transform(2,1),transform(2,2),transform(2,3));
        CGAL::Polygon_mesh_processing::transform(tran, mesh_);
    }

    void CgalModel::Transform(const Eigen::Matrix4f& transform)
    {
        Affine_3 tran(transform(0,0),transform(0,1),transform(0,2),transform(0,3),
            transform(1,0),transform(1,1),transform(1,2),transform(1,3),
            transform(2,0),transform(2,1),transform(2,2),transform(2,3)
        );
        CGAL::Polygon_mesh_processing::transform(tran, mesh_);
    }

    void CgalModel::Transform(const Eigen::Transform<float, 3, Eigen::Affine>& transform)
    {
        Affine_3 tran(transform(0,0),transform(0,1),transform(0,2),transform(0,3),
            transform(1,0),transform(1,1),transform(1,2),transform(1,3),
            transform(2,0),transform(2,1),transform(2,2),transform(2,3)
        );
        CGAL::Polygon_mesh_processing::transform(tran, mesh_);
    }

    void CgalModel::BoundingBox(Eigen::Vector3f& min, Eigen::Vector3f& max) const
    {
        min = Eigen::Vector3f(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
        max = Eigen::Vector3f(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest());
        for (const auto& vertex : mesh_.points())
        {
            min.x() = std::min(min.x(), static_cast<float>(vertex.x()));
            min.y() = std::min(min.y(), static_cast<float>(vertex.y()));
            min.z() = std::min(min.z(), static_cast<float>(vertex.z()));
            max.x() = std::max(max.x(), static_cast<float>(vertex.x()));
            max.y() = std::max(max.y(), static_cast<float>(vertex.y()));
            max.z() = std::max(max.z(), static_cast<float>(vertex.z()));
        }
    }

    float CgalModel::Volume() const
    {
        return static_cast<float>(CGAL::Polygon_mesh_processing::volume(mesh_));
    }

    std::pair<Eigen::MatrixXf, Eigen::MatrixXi> CgalModel::TriangleMesh() const
    {
        Eigen::MatrixXf vertices;
        Eigen::MatrixXi faces;
        igl::copyleft::cgal::polyhedron_to_mesh(mesh_, vertices, faces);
        return std::make_pair(vertices, faces);
    }
    CgalModel Union(const CgalModel& left, const CgalModel& right)
    {
        CgalModel::Nef_Polyheron_3 left_br = CgalModel::Nef_Polyheron_3{ left.mesh_ };
        CgalModel::Nef_Polyheron_3 right_br = CgalModel::Nef_Polyheron_3{ right.mesh_ };
        auto r_br = left_br.join(right_br);
        CgalModel::Polyhedron_3 r;
        r_br.convert_to_polyhedron(r);
        return CgalModel(r);
    }

    CgalModel Intersection(const CgalModel& left, const CgalModel& right)
    {
        CgalModel::Nef_Polyheron_3 left_br = CgalModel::Nef_Polyheron_3{ left.mesh_ };
        CgalModel::Nef_Polyheron_3 right_br = CgalModel::Nef_Polyheron_3{ right.mesh_ };
        auto r_br = left_br.intersection(right_br);
        CgalModel::Polyhedron_3 r;
        r_br.convert_to_polyhedron(r);
        return CgalModel(r);
    }

    CgalModel Difference(const CgalModel& left, const CgalModel& right)
    {
        CgalModel::Nef_Polyheron_3 left_br = CgalModel::Nef_Polyheron_3{ left.mesh_ };
        CgalModel::Nef_Polyheron_3 right_br = CgalModel::Nef_Polyheron_3{ right.mesh_ };
        auto r_br = left_br.difference(right_br);
        CgalModel::Polyhedron_3 r;
        r_br.convert_to_polyhedron(r);
        return CgalModel(r);
    }

    CgalModel Xor(const CgalModel& left, const CgalModel& right)
    {
        CgalModel::Nef_Polyheron_3 left_br = CgalModel::Nef_Polyheron_3{ left.mesh_ };
        CgalModel::Nef_Polyheron_3 right_br = CgalModel::Nef_Polyheron_3{ right.mesh_ };
        auto r_br = left_br.symmetric_difference(right_br);
        CgalModel::Polyhedron_3 r;
        r_br.convert_to_polyhedron(r);
        return CgalModel(r);
    }
}// namespace HsBa::Slicer

std::size_t std::hash<HsBa::Slicer::CgalModel>::operator()(const HsBa::Slicer::CgalModel& cgalmodel)
{
    std::size_t hash = 0;
    for (const auto& p : cgalmodel.mesh_.points())
    {
        boost::hash_combine(hash, p);
    }
    for (const auto& e : cgalmodel.mesh_.edges())
    {
        boost::hash_combine(hash, e.face());
    }
    return hash;
}