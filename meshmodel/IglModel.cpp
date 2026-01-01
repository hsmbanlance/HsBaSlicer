#include "IglModel.hpp"

#include <cmath>
#include <numbers>

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
#include <cmath>
#include <algorithm>

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
        auto is_valid_mesh = [](const Eigen::MatrixXf& V, const Eigen::MatrixXi& F)->bool{
            if (V.rows() == 0 || F.rows() == 0) return false;
            if (V.cols() < 3) return false;
            if (F.cols() < 3) return false;
            // finite check
            for (int r = 0; r < V.rows(); ++r)
            {
                for (int c = 0; c < V.cols(); ++c)
                {
                    float val = V(r,c);
                    if (!std::isfinite(val)) return false;
                }
            }
            // indices bounds
            int nv = V.rows();
            for (int r = 0; r < F.rows(); ++r)
            {
                for (int c = 0; c < F.cols(); ++c)
                {
                    int idx = F(r,c);
                    if (idx < 0 || idx >= nv) return false;
                }
            }
            return true;
        };

        Eigen::MatrixXf v;
        Eigen::MatrixXi f;
        if (!is_valid_mesh(left.vertices_, left.faces_) || !is_valid_mesh(right.vertices_, right.faces_))
        {
            return IglModel(Eigen::MatrixXf(), Eigen::MatrixXi());
        }

        igl::copyleft::cgal::mesh_boolean(left.vertices_, left.faces_, right.vertices_, right.faces_, igl::MESH_BOOLEAN_TYPE_UNION, v, f);
        
        if (v.rows() == 0 || f.rows() == 0) return IglModel(Eigen::MatrixXf(), Eigen::MatrixXi());
        return IglModel(v, f);
    }
    IglModel Intersection(const IglModel& left, const IglModel& right)
    {
        auto is_valid_mesh = [](const Eigen::MatrixXf& V, const Eigen::MatrixXi& F)->bool{
            if (V.rows() == 0 || F.rows() == 0) return false;
            if (V.cols() < 3) return false;
            if (F.cols() < 3) return false;
            for (int r = 0; r < V.rows(); ++r)
                for (int c = 0; c < V.cols(); ++c)
                    if (!std::isfinite(V(r,c))) return false;
            int nv = V.rows();
            for (int r = 0; r < F.rows(); ++r)
                for (int c = 0; c < F.cols(); ++c)
                    if (F(r,c) < 0 || F(r,c) >= nv) return false;
            return true;
        };
        Eigen::MatrixXf v;
        Eigen::MatrixXi f;
        if (!is_valid_mesh(left.vertices_, left.faces_) || !is_valid_mesh(right.vertices_, right.faces_))
            return IglModel(Eigen::MatrixXf(), Eigen::MatrixXi());

        igl::copyleft::cgal::mesh_boolean(left.vertices_, left.faces_, right.vertices_, right.faces_, igl::MESH_BOOLEAN_TYPE_INTERSECT, v, f);
        
        if (v.rows() == 0 || f.rows() == 0) return IglModel(Eigen::MatrixXf(), Eigen::MatrixXi());
        return IglModel(v, f);
    }
    IglModel Difference(const IglModel& left, const IglModel& right)
    {
        auto is_valid_mesh = [](const Eigen::MatrixXf& V, const Eigen::MatrixXi& F)->bool{
            if (V.rows() == 0 || F.rows() == 0) return false;
            if (V.cols() < 3) return false;
            if (F.cols() < 3) return false;
            for (int r = 0; r < V.rows(); ++r)
                for (int c = 0; c < V.cols(); ++c)
                    if (!std::isfinite(V(r,c))) return false;
            int nv = V.rows();
            for (int r = 0; r < F.rows(); ++r)
                for (int c = 0; c < F.cols(); ++c)
                    if (F(r,c) < 0 || F(r,c) >= nv) return false;
            return true;
        };
        Eigen::MatrixXf v;
        Eigen::MatrixXi f;
        if (!is_valid_mesh(left.vertices_, left.faces_) || !is_valid_mesh(right.vertices_, right.faces_))
            return IglModel(Eigen::MatrixXf(), Eigen::MatrixXi());

        igl::copyleft::cgal::mesh_boolean(left.vertices_, left.faces_, right.vertices_, right.faces_, igl::MESH_BOOLEAN_TYPE_MINUS, v, f);
        
        if (v.rows() == 0 || f.rows() == 0) return IglModel(Eigen::MatrixXf(), Eigen::MatrixXi());
        return IglModel(v, f);
    }
    IglModel Xor(const IglModel& left, const IglModel& right)
    {
        auto is_valid_mesh = [](const Eigen::MatrixXf& V, const Eigen::MatrixXi& F)->bool{
            if (V.rows() == 0 || F.rows() == 0) return false;
            if (V.cols() < 3) return false;
            if (F.cols() < 3) return false;
            for (int r = 0; r < V.rows(); ++r)
                for (int c = 0; c < V.cols(); ++c)
                    if (!std::isfinite(V(r,c))) return false;
            int nv = V.rows();
            for (int r = 0; r < F.rows(); ++r)
                for (int c = 0; c < F.cols(); ++c)
                    if (F(r,c) < 0 || F(r,c) >= nv) return false;
            return true;
        };
        Eigen::MatrixXf v;
        Eigen::MatrixXi f;
        if (!is_valid_mesh(left.vertices_, left.faces_) || !is_valid_mesh(right.vertices_, right.faces_))
            return IglModel(Eigen::MatrixXf(), Eigen::MatrixXi());

        igl::copyleft::cgal::mesh_boolean(left.vertices_, left.faces_, right.vertices_, right.faces_, igl::MESH_BOOLEAN_TYPE_XOR, v, f);

        if (v.rows() == 0 || f.rows() == 0) return IglModel(Eigen::MatrixXf(), Eigen::MatrixXi());
        return IglModel(v, f);
    }

        IglModel IglModel::CreateBox(const Eigen::Vector3f& size)
    {
        const Eigen::Vector3f h = size * 0.5f;
        std::vector<Eigen::Vector3f> verts{
            {-h.x(), -h.y(), -h.z()},
            { h.x(), -h.y(), -h.z()},
            { h.x(),  h.y(), -h.z()},
            {-h.x(),  h.y(), -h.z()},
            {-h.x(), -h.y(),  h.z()},
            { h.x(), -h.y(),  h.z()},
            { h.x(),  h.y(),  h.z()},
            {-h.x(),  h.y(),  h.z()}
        };
        std::vector<Eigen::Vector3i> faces{
            {0,1,2},{0,2,3}, // bottom
            {4,6,5},{4,7,6}, // top
            {0,4,5},{0,5,1}, // -y
            {1,5,6},{1,6,2}, // +x
            {2,6,7},{2,7,3}, // +y
            {3,7,4},{3,4,0}  // -x
        };
        Eigen::MatrixXf v(verts.size(), 3);
        Eigen::MatrixXi f(faces.size(), 3);
        for (size_t i = 0; i < verts.size(); ++i) v.row((int)i) = verts[i];
        for (size_t i = 0; i < faces.size(); ++i) f.row((int)i) = faces[i];
        return IglModel(std::move(v), std::move(f), true);
    }

    IglModel IglModel::CreateSphere(const float radius, const int subdivisions)
    {
        const int stacks = std::max(4, 2 * subdivisions + 6);
        const int slices = std::max(8, 8 * subdivisions + 8);
        std::vector<Eigen::Vector3f> verts;
        std::vector<Eigen::Vector3i> faces;
        for (int i = 0; i <= stacks; ++i)
        {
            float v = (float)i / (float)stacks;
            float theta = v * std::numbers::pi_v<float>; // 0..pi
            for (int j = 0; j < slices; ++j)
            {
                float u = (float)j / (float)slices;
                float phi = u * 2.0f * std::numbers::pi_v<float>; // 0..2pi
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
        return IglModel(std::move(v), std::move(f), true);
    }

    IglModel IglModel::CreateCylinder(const float radius, const float height, const int segments)
    {
        const int seg = std::max(3, segments);
        const float h2 = height * 0.5f;
        std::vector<Eigen::Vector3f> verts;
        std::vector<Eigen::Vector3i> faces;
        // ring bottom and top
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
        // sides
        for (int i = 0; i < seg; ++i)
        {
            int i0 = i * 2;
            int i1 = ((i + 1) % seg) * 2;
            // quad (i0 top/bottom) -> two triangles
            faces.emplace_back(i0, i1, i0+1);
            faces.emplace_back(i1, i1+1, i0+1);
            // bottom cap
            faces.emplace_back(bottomCenter, i0, i1);
            // top cap
            faces.emplace_back(topCenter, i1+1, i0+1);
        }
        Eigen::MatrixXf v(verts.size(), 3);
        Eigen::MatrixXi f(faces.size(), 3);
        for (size_t i = 0; i < verts.size(); ++i) v.row((int)i) = verts[i];
        for (size_t i = 0; i < faces.size(); ++i) f.row((int)i) = faces[i];
        return IglModel(std::move(v), std::move(f), true);
    }

    IglModel IglModel::CreateCone(const float radius, const float height, const int segments)
    {
        const int seg = std::max(3, segments);
        const float h2 = height * 0.5f;
        std::vector<Eigen::Vector3f> verts;
        std::vector<Eigen::Vector3i> faces;
        // base ring
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
        return IglModel(std::move(v), std::move(f), true);
    }

    IglModel IglModel::CreateTorus(const float majorRadius, const float minorRadius, const int majorSegments, const int minorSegments)
    {
        const int R = std::max(3, majorSegments);
        const int r = std::max(3, minorSegments);
        std::vector<Eigen::Vector3f> verts;
        std::vector<Eigen::Vector3i> faces;
        for (int i = 0; i < R; ++i)
        {
            float u = (float)i / R * 2.0f * std::numbers::pi_v<float>;
            Eigen::Vector3f center(majorRadius * std::cos(u), majorRadius * std::sin(u), 0);
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
        return IglModel(std::move(v), std::move(f), true);
    }
}// namespace HsBa::Slicer
