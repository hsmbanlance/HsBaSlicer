#include "CgalModel.hpp"

#include <cmath>
#include <vector>
#include <numbers>

#include <CGAL/IO/io.h>
#include <CGAL/Polyhedron_3.h>
#include <CGAL/boost/graph/IO/polygon_mesh_io.h>
#include <CGAL/boost/graph/generators.h>
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
        bool ok = CGAL::IO::read_polygon_mesh(filepath_ansi, mesh_);
        if (ok)
        {
            // ensure faces are triangulated after load to keep downstream code safe
            CGAL::Polygon_mesh_processing::triangulate_faces(mesh_);
        }
        return ok;
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
        // ensure triangular faces before converting to igl mesh
        Polyhedron_3 tmp = mesh_;
        CGAL::Polygon_mesh_processing::triangulate_faces(tmp);
        igl::copyleft::cgal::polyhedron_to_mesh(tmp, vertices, faces);
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
    
    CgalModel CgalModel::CreateBox(const Eigen::Vector3f& size)
    {
        // Use CGAL generator to build a valid hexahedron (closed, oriented)
        const Eigen::Vector3f h = size * 0.5f;
        Point_3 min_p(-h.x(), -h.y(), -h.z());
        Point_3 max_p( h.x(),  h.y(),  h.z());
        // Iso_cuboid_3 describes the box corners
        EpicKernel::Iso_cuboid_3 cub(min_p, max_p);
        Polyhedron_3 poly;
        CGAL::make_hexahedron(cub, poly);
        // ensure faces are triangulated: libigl's polyhedron_to_mesh expects triangular faces
        CGAL::Polygon_mesh_processing::triangulate_faces(poly);
        return CgalModel(poly);
    }

    CgalModel CgalModel::CreateSphere(const float radius, const int subdivisions)
    {
        const int stacks = std::max(4, 2 * subdivisions + 6);
        const int slices = std::max(8, 8 * subdivisions + 8);
        std::vector<Eigen::Vector3f> verts;
        std::vector<Eigen::Vector3i> faces;
        for (int i = 0; i <= stacks; ++i)
        {
            float v = (float)i / (float)stacks;
            float theta = v * std::numbers::pi_v<float>;
            for (int j = 0; j < slices; ++j)
            {
                float u = (float)j / (float)slices;
                float phi = u * 2.0f * std::numbers::pi_v<float>;
                float x = radius * std::sin(theta) * std::cos(phi);
                float y = radius * std::sin(theta) * std::sin(phi);
                float z = radius * std::cos(theta);
                verts.emplace_back(x, y, z);
            }
        }
        for (int i = 0; i < stacks; ++i)
        {
            for (int j = 0; j < slices; ++j)
            {
                int next = (j + 1) % slices;
                int a = i * slices + j;
                int b = i * slices + next;
                int c = (i + 1) * slices + j;
                int d = (i + 1) * slices + next;
                if (i != 0) faces.emplace_back(a, c, b);
                if (i != stacks - 1) faces.emplace_back(b, c, d);
            }
        }
        Eigen::MatrixXf v(verts.size(), 3);
        Eigen::MatrixXi f(faces.size(), 3);
        for (size_t i = 0; i < verts.size(); ++i) v.row((int)i) = verts[i];
        for (size_t i = 0; i < faces.size(); ++i) f.row((int)i) = faces[i];
        return CgalModel(v, f);
    }

    CgalModel CgalModel::CreateCylinder(const float radius, const float height, const int segments)
    {
        const int seg = std::max(3, segments);
        const float h2 = height * 0.5f;
        std::vector<Eigen::Vector3f> verts;
        std::vector<Eigen::Vector3i> faces;
        for (int i = 0; i < seg; ++i)
        {
            float a = (float)i / seg * 2.0f * std::numbers::pi_v<float>;
            float x = radius * std::cos(a);
            float y = radius * std::sin(a);
            verts.emplace_back(x, y, -h2);
            verts.emplace_back(x, y, h2);
        }
        int bottomCenter = (int)verts.size();
        verts.emplace_back(0,0,-h2);
        int topCenter = (int)verts.size();
        verts.emplace_back(0,0,h2);
        for (int i = 0; i < seg; ++i)
        {
            int i0 = i * 2;
            int i1 = ((i + 1) % seg) * 2;
            faces.emplace_back(i0, i1, i0+1);
            faces.emplace_back(i1, i1+1, i0+1);
            faces.emplace_back(bottomCenter, i0, i1);
            faces.emplace_back(topCenter, i1+1, i0+1);
        }
        Eigen::MatrixXf v(verts.size(), 3);
        Eigen::MatrixXi f(faces.size(), 3);
        for (size_t i = 0; i < verts.size(); ++i) v.row((int)i) = verts[i];
        for (size_t i = 0; i < faces.size(); ++i) f.row((int)i) = faces[i];
        return CgalModel(v, f);
    }

    CgalModel CgalModel::CreateCone(const float radius, const float height, const int segments)
    {
        const int seg = std::max(3, segments);
        const float h2 = height * 0.5f;
        std::vector<Eigen::Vector3f> verts;
        std::vector<Eigen::Vector3i> faces;
        for (int i = 0; i < seg; ++i)
        {
            float a = (float)i / seg * 2.0f * std::numbers::pi_v<float>;
            float x = radius * std::cos(a);
            float y = radius * std::sin(a);
            verts.emplace_back(x, y, -h2);
        }
        int baseCenter = (int)verts.size();
        verts.emplace_back(0,0,-h2);
        int apexIndex = (int)verts.size();
        verts.emplace_back(0,0,h2);
        for (int i = 0; i < seg; ++i)
        {
            int ni = (i + 1) % seg;
            faces.emplace_back(baseCenter, i, ni);
            faces.emplace_back(i, apexIndex, ni);
        }
        Eigen::MatrixXf v(verts.size(), 3);
        Eigen::MatrixXi f(faces.size(), 3);
        for (size_t i = 0; i < verts.size(); ++i) v.row((int)i) = verts[i];
        for (size_t i = 0; i < faces.size(); ++i) f.row((int)i) = faces[i];
        return CgalModel(v, f);
    }

    CgalModel CgalModel::CreateTorus(const float majorRadius, const float minorRadius, const int majorSegments, const int minorSegments)
    {
        const int R = std::max(3, majorSegments);
        const int r = std::max(3, minorSegments);
        std::vector<Eigen::Vector3f> verts;
        std::vector<Eigen::Vector3i> faces;
        for (int i = 0; i < R; ++i)
        {
            float u = (float)i / R * 2.0f * std::numbers::pi_v<float>;
            for (int j = 0; j < r; ++j)
            {
                float v = (float)j / r * 2.0f * std::numbers::pi_v<float>;
                float x = (majorRadius + minorRadius * std::cos(v)) * std::cos(u);
                float y = (majorRadius + minorRadius * std::cos(v)) * std::sin(u);
                float z = minorRadius * std::sin(v);
                verts.emplace_back(x, y, z);
            }
        }
        for (int i = 0; i < R; ++i)
        {
            for (int j = 0; j < r; ++j)
            {
                int ni = (i + 1) % R;
                int nj = (j + 1) % r;
                int a = i * r + j;
                int b = ni * r + j;
                int c = i * r + nj;
                int d = ni * r + nj;
                faces.emplace_back(a, b, c);
                faces.emplace_back(b, d, c);
            }
        }
        Eigen::MatrixXf v(verts.size(), 3);
        Eigen::MatrixXi f(faces.size(), 3);
        for (size_t i = 0; i < verts.size(); ++i) v.row((int)i) = verts[i];
        for (size_t i = 0; i < faces.size(); ++i) f.row((int)i) = faces[i];
        return CgalModel(v, f);
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