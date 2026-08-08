#include "CgalModel.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

#include <CGAL/AABB_face_graph_triangle_primitive.h>
#include <CGAL/AABB_traits_3.h>
#include <CGAL/AABB_tree.h>
#include <CGAL/Boolean_set_operations_2.h>
#include <CGAL/IO/io.h>
#include <CGAL/Polyhedron_3.h>
#include <CGAL/Polyhedron_incremental_builder_3.h>
#include <CGAL/Simple_cartesian.h>
#include <CGAL/Surface_mesh.h>
#include <CGAL/Surface_mesh_shortest_path.h>
#include <CGAL/boost/graph/IO/polygon_mesh_io.h>
#include <CGAL/boost/graph/copy_face_graph.h>
#include <CGAL/boost/graph/generators.h>
#include <CGAL/polygon_mesh_processing.h>

#include <igl/copyleft/cgal/mesh_to_polyhedron.h>
#include <igl/copyleft/cgal/polyhedron_to_mesh.h>

#include "IglModel.hpp"
#include "base/ModelFormat.hpp"
#include "base/encoding_convert.hpp"
#include "base/error.hpp"


namespace HsBa::Slicer
{
CgalModel::CgalModel(const Polyhedron_3& o) : mesh_{o}
{
}

CgalModel::CgalModel(const Eigen::MatrixXf& v, const Eigen::MatrixXi& f)
{
    if (v.rows() == 0 || f.rows() == 0)
    {
        throw RuntimeError("Empty mesh provided to CgalModel constructor");
    }

    // Try direct conversion first
    bool success = igl::copyleft::cgal::mesh_to_polyhedron(v, f, mesh_);

    if (!success)
    {
        // Try to repair the mesh using CGAL's Polygon Mesh Processing
        try
        {
            // Step 1: Create a polygon soup from the input mesh
            std::vector<Point_3> points;
            std::vector<std::vector<std::size_t>> polygons;

            // Convert vertices
            for (int i = 0; i < v.rows(); ++i)
            {
                points.emplace_back(v(i, 0), v(i, 1), v(i, 2));
            }

            // Convert faces
            for (int i = 0; i < f.rows(); ++i)
            {
                std::vector<std::size_t> face;
                for (int j = 0; j < 3; ++j)
                {
                    face.push_back(static_cast<std::size_t>(f(i, j)));
                }
                polygons.push_back(std::move(face));
            }

            // Step 2: Orient the polygon soup consistently
            CGAL::Polygon_mesh_processing::orient_polygon_soup(points, polygons);

            // Step 3: Merge duplicate vertices and build a proper mesh
            try
            {
                CGAL::Polygon_mesh_processing::polygon_soup_to_polygon_mesh(points, polygons, mesh_);
                success = true;  // If no exception thrown, assume success

                // Additional validation and fix face orientation if needed
                if (mesh_.is_valid())
                {
                    // Check volume sign and flip faces if needed
                    double vol = CGAL::Polygon_mesh_processing::volume(mesh_);
                    if (vol < 0.0)
                    {
                        CGAL::Polygon_mesh_processing::reverse_face_orientations(mesh_);
                    }
                }
                else
                {
                    success = false;
                }
            }
            catch (const std::exception& e)
            {
                // CGAL operations may throw std::exception or derived types
                success = false;
            }
        }
        catch (const std::exception& e)
        {
            // Outer exception handler for polygon soup construction
            success = false;
        }

        if (!success)
        {
            throw RuntimeError("mesh_to_polyhedron conversion failed even after repair attempts");
        }
    }
}

bool CgalModel::Load(std::string_view filePath)
{
    filename_ = filePath;
    std::string filepath_ansi = utf8_to_local(filename_);
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
    Affine_3 tran(CGAL::Translation(), Vector_3{translation.x(), translation.y(), translation.z()});
    CGAL::Polygon_mesh_processing::transform(tran, mesh_);
}

void CgalModel::Rotate(const Eigen::Quaternionf& rotation)
{
    Eigen::Matrix3f rotationMatrix = rotation.toRotationMatrix();
    Affine_3 tran(rotationMatrix(0, 0), rotationMatrix(0, 1), rotationMatrix(0, 2), 0, rotationMatrix(1, 0),
                  rotationMatrix(1, 1), rotationMatrix(1, 2), 0, rotationMatrix(2, 0), rotationMatrix(2, 1),
                  rotationMatrix(2, 2), 0);
    CGAL::Polygon_mesh_processing::transform(tran, mesh_);
}

void CgalModel::Scale(const float scale)
{
    Affine_3 tran(CGAL::Scaling(), scale);
    CGAL::Polygon_mesh_processing::transform(tran, mesh_);
}

void CgalModel::Scale(const Eigen::Vector3f& scale)
{
    Affine_3 tran(scale.x(), 0, 0, 0, 0, scale.y(), 0, 0, 0, 0, scale.z(), 0);
    CGAL::Polygon_mesh_processing::transform(tran, mesh_);
}

void CgalModel::Transform(const Eigen::Isometry3f& transform)
{
    Affine_3 tran(transform(0, 0), transform(0, 1), transform(0, 2), transform(0, 3), transform(1, 0), transform(1, 1),
                  transform(1, 2), transform(1, 3), transform(2, 0), transform(2, 1), transform(2, 2), transform(2, 3));
    CGAL::Polygon_mesh_processing::transform(tran, mesh_);
}

void CgalModel::Transform(const Eigen::Matrix4f& transform)
{
    Affine_3 tran(transform(0, 0), transform(0, 1), transform(0, 2), transform(0, 3), transform(1, 0), transform(1, 1),
                  transform(1, 2), transform(1, 3), transform(2, 0), transform(2, 1), transform(2, 2), transform(2, 3));
    CGAL::Polygon_mesh_processing::transform(tran, mesh_);
}

void CgalModel::Transform(const Eigen::Transform<float, 3, Eigen::Affine>& transform)
{
    Affine_3 tran(transform(0, 0), transform(0, 1), transform(0, 2), transform(0, 3), transform(1, 0), transform(1, 1),
                  transform(1, 2), transform(1, 3), transform(2, 0), transform(2, 1), transform(2, 2), transform(2, 3));
    CGAL::Polygon_mesh_processing::transform(tran, mesh_);
}

void CgalModel::BoundingBox(Eigen::Vector3f& min, Eigen::Vector3f& max) const
{
    min = Eigen::Vector3f(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
                          std::numeric_limits<float>::max());
    max = Eigen::Vector3f(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(),
                          std::numeric_limits<float>::lowest());
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
    CgalModel::Nef_Polyheron_3 left_br = CgalModel::Nef_Polyheron_3{left.mesh_};
    CgalModel::Nef_Polyheron_3 right_br = CgalModel::Nef_Polyheron_3{right.mesh_};
    auto r_br = left_br.join(right_br);
    CgalModel::Polyhedron_3 r;
    r_br.convert_to_polyhedron(r);
    return CgalModel(r);
}

CgalModel Intersection(const CgalModel& left, const CgalModel& right)
{
    CgalModel::Nef_Polyheron_3 left_br = CgalModel::Nef_Polyheron_3{left.mesh_};
    CgalModel::Nef_Polyheron_3 right_br = CgalModel::Nef_Polyheron_3{right.mesh_};
    auto r_br = left_br.intersection(right_br);
    CgalModel::Polyhedron_3 r;
    r_br.convert_to_polyhedron(r);
    return CgalModel(r);
}

CgalModel Difference(const CgalModel& left, const CgalModel& right)
{
    CgalModel::Nef_Polyheron_3 left_br = CgalModel::Nef_Polyheron_3{left.mesh_};
    CgalModel::Nef_Polyheron_3 right_br = CgalModel::Nef_Polyheron_3{right.mesh_};
    auto r_br = left_br.difference(right_br);
    CgalModel::Polyhedron_3 r;
    r_br.convert_to_polyhedron(r);
    return CgalModel(r);
}

CgalModel Xor(const CgalModel& left, const CgalModel& right)
{
    CgalModel::Nef_Polyheron_3 left_br = CgalModel::Nef_Polyheron_3{left.mesh_};
    CgalModel::Nef_Polyheron_3 right_br = CgalModel::Nef_Polyheron_3{right.mesh_};
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
    Point_3 max_p(h.x(), h.y(), h.z());
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
            if (i != 0)
                faces.emplace_back(a, c, b);
            if (i != stacks - 1)
                faces.emplace_back(b, c, d);
        }
    }
    Eigen::MatrixXf v(verts.size(), 3);
    Eigen::MatrixXi f(faces.size(), 3);
    for (size_t i = 0; i < verts.size(); ++i)
        v.row((int)i) = verts[i];
    for (size_t i = 0; i < faces.size(); ++i)
        f.row((int)i) = faces[i];
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
    verts.emplace_back(0, 0, -h2);
    int topCenter = (int)verts.size();
    verts.emplace_back(0, 0, h2);
    for (int i = 0; i < seg; ++i)
    {
        int i0 = i * 2;
        int i1 = ((i + 1) % seg) * 2;
        faces.emplace_back(i0, i1, i0 + 1);
        faces.emplace_back(i1, i1 + 1, i0 + 1);
        faces.emplace_back(bottomCenter, i0, i1);
        faces.emplace_back(topCenter, i1 + 1, i0 + 1);
    }
    Eigen::MatrixXf v(verts.size(), 3);
    Eigen::MatrixXi f(faces.size(), 3);
    for (size_t i = 0; i < verts.size(); ++i)
        v.row((int)i) = verts[i];
    for (size_t i = 0; i < faces.size(); ++i)
        f.row((int)i) = faces[i];
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
    verts.emplace_back(0, 0, -h2);
    int apexIndex = (int)verts.size();
    verts.emplace_back(0, 0, h2);
    for (int i = 0; i < seg; ++i)
    {
        int ni = (i + 1) % seg;
        faces.emplace_back(baseCenter, i, ni);
        faces.emplace_back(i, apexIndex, ni);
    }
    Eigen::MatrixXf v(verts.size(), 3);
    Eigen::MatrixXi f(faces.size(), 3);
    for (size_t i = 0; i < verts.size(); ++i)
        v.row((int)i) = verts[i];
    for (size_t i = 0; i < faces.size(); ++i)
        f.row((int)i) = faces[i];
    return CgalModel(v, f);
}

CgalModel CgalModel::CreateTorus(const float majorRadius, const float minorRadius, const int majorSegments,
                                 const int minorSegments)
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
    for (size_t i = 0; i < verts.size(); ++i)
        v.row((int)i) = verts[i];
    for (size_t i = 0; i < faces.size(); ++i)
        f.row((int)i) = faces[i];
    return CgalModel(v, f);
}

// ========== 辅助 Builder 类 ==========

class PrismBuilder : public CGAL::Modifier_base<CgalModel::Polyhedron_3::HalfedgeDS>
{
public:
    using Point_3 = CgalModel::Point_3;

    PrismBuilder(const std::vector<std::array<int, 3>>& bottom_tris, const std::vector<Eigen::Vector2f>& verts_2d,
                 const Eigen::Vector3f& dir)
        : bottom_tris_(bottom_tris), verts_2d_(verts_2d), dir_(dir)
    {
    }

    void operator()(CgalModel::Polyhedron_3::HalfedgeDS& hds) override
    {
        const int n = static_cast<int>(verts_2d_.size());
        const int n_bottom = static_cast<int>(bottom_tris_.size());
        const int n_side = 2 * n;

        CGAL::Polyhedron_incremental_builder_3<CgalModel::Polyhedron_3::HalfedgeDS> builder(hds, true);
        builder.begin_surface(2 * n, 2 * n_bottom + n_side);

        // 底面顶点 (z = 0)
        for (int i = 0; i < n; ++i)
        {
            builder.add_vertex(Point_3(verts_2d_[i].x(), verts_2d_[i].y(), 0.0));
        }

        // 顶面顶点
        for (int i = 0; i < n; ++i)
        {
            builder.add_vertex(Point_3(verts_2d_[i].x() + dir_.x(), verts_2d_[i].y() + dir_.y(), dir_.z()));
        }

        // 底面（法向朝下，-Z方向）
        for (const auto& tri : bottom_tris_)
        {
            builder.begin_facet();
            // Reverse winding for bottom face
            builder.add_vertex_to_facet(tri[0]);
            builder.add_vertex_to_facet(tri[2]);
            builder.add_vertex_to_facet(tri[1]);
            builder.end_facet();
        }

        // 顶面（法向朝上，+Z方向）
        for (const auto& tri : bottom_tris_)
        {
            builder.begin_facet();
            // Keep CCW winding for top face
            builder.add_vertex_to_facet(tri[0] + n);
            builder.add_vertex_to_facet(tri[1] + n);
            builder.add_vertex_to_facet(tri[2] + n);
            builder.end_facet();
        }

        // 侧面（法向朝外）
        for (int i = 0; i < n; ++i)
        {
            int j = (i + 1) % n;
            int v0 = i, v1 = j, v2 = j + n, v3 = i + n;

            // Match IglModel's vertex order exactly
            builder.begin_facet();
            builder.add_vertex_to_facet(v0);
            builder.add_vertex_to_facet(v1);
            builder.add_vertex_to_facet(v2);
            builder.end_facet();

            builder.begin_facet();
            builder.add_vertex_to_facet(v0);
            builder.add_vertex_to_facet(v2);
            builder.add_vertex_to_facet(v3);
            builder.end_facet();
        }

        builder.end_surface();
        if (builder.error())
        {
            throw std::runtime_error("Polyhedron construction failed");
        }
    }

private:
    const std::vector<std::array<int, 3>>& bottom_tris_;
    const std::vector<Eigen::Vector2f>& verts_2d_;
    const Eigen::Vector3f dir_;
};

class MultiPathPrismBuilder : public CGAL::Modifier_base<CgalModel::Polyhedron_3::HalfedgeDS>
{
public:
    using Point_3 = CgalModel::Point_3;

    MultiPathPrismBuilder(const std::vector<std::array<int, 3>>& bottom_tris,
                          const std::vector<std::array<int, 2>>& side_edges,
                          const std::vector<Eigen::Vector2f>& verts_2d, const Eigen::Vector3f& dir)
        : bottom_tris_(bottom_tris), side_edges_(side_edges), verts_2d_(verts_2d), dir_(dir)
    {
    }

    void operator()(CgalModel::Polyhedron_3::HalfedgeDS& hds) override
    {
        const int n = static_cast<int>(verts_2d_.size());
        const int n_bottom = static_cast<int>(bottom_tris_.size());
        const int n_side = 2 * static_cast<int>(side_edges_.size());

        CGAL::Polyhedron_incremental_builder_3<CgalModel::Polyhedron_3::HalfedgeDS> builder(hds, true);
        builder.begin_surface(2 * n, 2 * n_bottom + n_side);

        // 底面顶点
        for (int i = 0; i < n; ++i)
        {
            builder.add_vertex(Point_3(verts_2d_[i].x(), verts_2d_[i].y(), 0.0));
        }

        // 顶面顶点
        for (int i = 0; i < n; ++i)
        {
            builder.add_vertex(Point_3(verts_2d_[i].x() + dir_.x(), verts_2d_[i].y() + dir_.y(), dir_.z()));
        }

        // 底面（法向朝下，-Z方向）
        for (const auto& tri : bottom_tris_)
        {
            builder.begin_facet();
            builder.add_vertex_to_facet(tri[0]);
            builder.add_vertex_to_facet(tri[2]);
            builder.add_vertex_to_facet(tri[1]);
            builder.end_facet();
        }

        // 顶面（法向朝上，+Z方向）
        for (const auto& tri : bottom_tris_)
        {
            builder.begin_facet();
            builder.add_vertex_to_facet(tri[0] + n);
            builder.add_vertex_to_facet(tri[1] + n);
            builder.add_vertex_to_facet(tri[2] + n);
            builder.end_facet();
        }

        // 侧面（法向朝外）
        for (const auto& edge : side_edges_)
        {
            int v0 = edge[0], v1 = edge[1];
            int v2 = v1 + n, v3 = v0 + n;

            // Match IglModel's vertex order exactly
            builder.begin_facet();
            builder.add_vertex_to_facet(v0);
            builder.add_vertex_to_facet(v1);
            builder.add_vertex_to_facet(v2);
            builder.end_facet();

            builder.begin_facet();
            builder.add_vertex_to_facet(v0);
            builder.add_vertex_to_facet(v2);
            builder.add_vertex_to_facet(v3);
            builder.end_facet();
        }

        builder.end_surface();
        if (builder.error())
        {
            throw RuntimeError("Polyhedron construction failed");
        }
    }

private:
    const std::vector<std::array<int, 3>>& bottom_tris_;
    const std::vector<std::array<int, 2>>& side_edges_;
    const std::vector<Eigen::Vector2f>& verts_2d_;
    const Eigen::Vector3f dir_;
};

// ========== 实现 ==========

CgalModel CgalModel::CreatePrime(const PolygonD& poly, const Eigen::Vector3f& direction)
{
    const size_t n = poly.size();
    if (n < 3)
    {
        throw InvalidArgumentError("Polygon must have at least 3 points");
    }

    Clipper2Lib::PathsD paths_in{poly};
    Clipper2Lib::PathsD triangles;

    auto result = Clipper2Lib::Triangulate(paths_in, 0, triangles, true);
    if (result != Clipper2Lib::TriangulateResult::success)
    {
        throw RuntimeError("Triangulation failed");
    }

    auto findIndex = [&](const Clipper2Lib::PointD& p) -> int
    {
        for (int i = 0; i < static_cast<int>(n); ++i)
        {
            if (std::abs(poly[i].x - p.x) < 1e-9 && std::abs(poly[i].y - p.y) < 1e-9)
            {
                return i;
            }
        }
        return -1;
    };

    std::vector<std::array<int, 3>> bottom_tris;
    bottom_tris.reserve(triangles.size());

    for (const auto& tri : triangles)
    {
        if (tri.size() != 3)
            continue;
        std::array<int, 3> idx;
        for (int i = 0; i < 3; ++i)
        {
            idx[i] = findIndex(tri[i]);
            if (idx[i] < 0)
            {
                throw RuntimeError("Triangulation produced unexpected vertex");
            }
        }
        bottom_tris.push_back(idx);
    }

    // Clipper2 Triangulate 对已是三角形的输入不产出三角形，回退直接以原三角形作底面
    if (bottom_tris.empty() && n == 3)
    {
        bottom_tris.push_back({0, 1, 2});
    }

    // Build using IglModel first, then convert to CgalModel
    const Eigen::Vector3f dir(direction.x(), direction.y(), direction.z());

    // Build vertices
    Eigen::MatrixXf V(2 * n, 3);
    for (size_t i = 0; i < n; ++i)
    {
        V.row(i) << static_cast<float>(poly[i].x), static_cast<float>(poly[i].y), 0.0f;
        V.row(i + n) = V.row(i) + dir.transpose();
    }

    // Build faces
    const int n_bottom = static_cast<int>(bottom_tris.size());
    const int n_side = 2 * static_cast<int>(n);
    Eigen::MatrixXi F(2 * n_bottom + n_side, 3);
    int f = 0;

    // Bottom face (reversed winding)
    for (const auto& tri : bottom_tris)
    {
        F.row(f++) << tri[0], tri[2], tri[1];
    }

    // Top face
    for (const auto& tri : bottom_tris)
    {
        F.row(f++) << tri[0] + n, tri[1] + n, tri[2] + n;
    }

    // Side faces
    for (size_t i = 0; i < n; ++i)
    {
        size_t j = (i + 1) % n;
        int v0 = static_cast<int>(i);
        int v1 = static_cast<int>(j);
        int v2 = v1 + static_cast<int>(n);
        int v3 = v0 + static_cast<int>(n);

        F.row(f++) << v0, v1, v2;
        F.row(f++) << v0, v2, v3;
    }

    // Create IglModel and convert to CgalModel
    IglModel igl_model(V, F.topRows(f), false);
    auto [v, fa] = igl_model.TriangleMesh();
    CgalModel model(v, fa);
    // mesh_to_polyhedron 可能将共面三角形合并为多边形面片，导致体积计算丢失部分四面体分量；
    // 强制三角化并修正面朝向，保证体积为正
    CGAL::Polygon_mesh_processing::triangulate_faces(model.mesh_);
    if (CGAL::Polygon_mesh_processing::volume(model.mesh_) < 0.0)
    {
        CGAL::Polygon_mesh_processing::reverse_face_orientations(model.mesh_);
    }
    return model;
}

CgalModel CgalModel::CreatePrime(const PolygonsD& paths, const Eigen::Vector3f& direction)
{
    if (paths.empty())
    {
        throw InvalidArgumentError("Paths must not be empty");
    }

    // 归一化绕序：外轮廓 CCW（笛卡尔坐标下 Area > 0），孔洞 CW（Area < 0）。
    // Clipper2 Triangulate 按此约定识别孔洞；若外轮廓与孔洞同号，
    // 孔洞会被当作独立实体三角化导致体积偏大。
    // 通过几何包含关系判定孔洞，不依赖调用方传入的绕序。
    Clipper2Lib::PathsD norm_paths = paths;
    std::vector<bool> is_hole(norm_paths.size(), false);
    // 通过嵌套深度奇偶判定孔洞：路径 i 的深度 = 严格包含它的其他路径数，深度为奇即孔洞。
    // 包含判定用"多数顶点在内部"而非质心——质心可能落在内层子路径内
    // （如外方框质心恰在内孔中），导致外轮廓被误判为孔洞而整体反转绕序。
    // 注意：不能直接对 double 路径调用 Clipper2Lib::PointInPolygon——其 MSVC 分支按
    // int64 精确算术编写（TriSign 仅有 int64_t 重载，double 被隐式截断），
    // 非整数坐标会被误判共线而返回 IsOn。故按项目惯例先整型化（×integerization）
    // 到 Path64 再做包含判定（int64 精确算术），包含关系在缩放下不变，无需反整型化。
    const Clipper2Lib::Paths64 int_paths = Integerization(norm_paths);
    for (size_t i = 0; i < norm_paths.size(); ++i)
    {
        int depth = 0;
        for (size_t j = 0; j < norm_paths.size(); ++j)
        {
            if (i == j)
                continue;
            int inside = 0;
            for (const auto& pt : int_paths[i])
            {
                if (Clipper2Lib::PointInPolygon(pt, int_paths[j]) == Clipper2Lib::PointInPolygonResult::IsInside)
                {
                    ++inside;
                }
            }
            if (inside * 2 > static_cast<int>(norm_paths[i].size()))
            {
                ++depth;
            }
        }
        is_hole[i] = (depth % 2) == 1;
    }
    for (size_t i = 0; i < norm_paths.size(); ++i)
    {
        const double a = Clipper2Lib::Area(norm_paths[i]);
        if ((is_hole[i] && a > 0.0) || (!is_hole[i] && a < 0.0))
        {
            std::reverse(norm_paths[i].begin(), norm_paths[i].end());
        }
    }

    Clipper2Lib::PathsD triangles;
    auto result = Clipper2Lib::Triangulate(norm_paths, 0, triangles, true);
    if (result != Clipper2Lib::TriangulateResult::success)
    {
        throw RuntimeError("Triangulation failed");
    }

    std::vector<Eigen::Vector2f> unique_verts;
    auto findOrAdd = [&](const Clipper2Lib::PointD& p) -> int
    {
        for (int i = 0; i < static_cast<int>(unique_verts.size()); ++i)
        {
            if (std::abs(unique_verts[i].x() - static_cast<float>(p.x)) < 1e-6f &&
                std::abs(unique_verts[i].y() - static_cast<float>(p.y)) < 1e-6f)
            {
                return i;
            }
        }
        unique_verts.push_back({static_cast<float>(p.x), static_cast<float>(p.y)});
        return static_cast<int>(unique_verts.size()) - 1;
    };

    for (const auto& path : norm_paths)
    {
        for (const auto& pt : path)
        {
            findOrAdd(pt);
        }
    }

    for (const auto& tri : triangles)
    {
        for (const auto& pt : tri)
        {
            findOrAdd(pt);
        }
    }

    const int n = static_cast<int>(unique_verts.size());

    std::vector<std::array<int, 3>> bottom_tris;
    bottom_tris.reserve(triangles.size());

    for (const auto& tri : triangles)
    {
        if (tri.size() != 3)
            continue;
        std::array<int, 3> idx;
        for (int i = 0; i < 3; ++i)
        {
            idx[i] = findOrAdd(tri[i]);
        }
        // 强制底面三角形为 CCW（笛卡尔坐标下有向面积为正），不依赖 Triangulate 的输出绕序
        if (Clipper2Lib::Area(tri) < 0.0)
        {
            std::swap(idx[1], idx[2]);
        }
        bottom_tris.push_back(idx);
    }

    // Clipper2 Triangulate 对三角形路径不产出三角形，追加原始三角形路径补全底面，避免盖面丢失；
    // 外轮廓三角形归一化为 CCW，孔洞三角形归一化为 CW（盖面贡献相消）
    const size_t trianglePathCount = std::count_if(norm_paths.begin(), norm_paths.end(),
                                                   [](const PolygonD& p) { return p.size() == 3; });
    if (bottom_tris.size() < trianglePathCount)
    {
        for (size_t pi = 0; pi < norm_paths.size(); ++pi)
        {
            const auto& path = norm_paths[pi];
            if (path.size() != 3)
                continue;
            std::array<int, 3> idx;
            for (int i = 0; i < 3; ++i)
            {
                idx[i] = findOrAdd(path[i]);
            }
            const double a = Clipper2Lib::Area(path);
            if ((is_hole[pi] && a > 0.0) || (!is_hole[pi] && a < 0.0))
            {
                std::swap(idx[1], idx[2]);
            }
            bottom_tris.push_back(idx);
        }
    }

    // ========== 4. 构建 3D 顶点 ==========
    const Eigen::Vector3f dir(direction.x(), direction.y(), direction.z());
    Eigen::MatrixXf V(2 * n, 3);
    for (int i = 0; i < n; ++i)
    {
        V.row(i) << unique_verts[i].x(), unique_verts[i].y(), 0.0f;
        V.row(i + n) = V.row(i) + dir.transpose();
    }

    // ========== 5. 构建面 ==========
    const int n_bottom = static_cast<int>(bottom_tris.size());

    // 侧面：基于归一化 paths 的每条边（孔洞为 CW，保证侧壁法向指向孔内）
    int n_side_tris = 0;
    for (const auto& path : norm_paths)
    {
        n_side_tris += 2 * static_cast<int>(path.size());
    }

    Eigen::MatrixXi F(2 * n_bottom + n_side_tris, 3);
    int f = 0;

    // 底面：法向朝下（-Z），反转 winding
    for (const auto& tri : bottom_tris)
    {
        F.row(f++) << tri[0], tri[2], tri[1];
    }

    // 顶面：法向朝上（+Z），保持 CCW winding
    for (const auto& tri : bottom_tris)
    {
        F.row(f++) << tri[0] + n, tri[1] + n, tri[2] + n;
    }

    // 侧面：基于原始 paths 的边
    auto findVertIdx = [&](const Clipper2Lib::PointD& p) -> int
    {
        for (int i = 0; i < n; ++i)
        {
            if (std::abs(unique_verts[i].x() - static_cast<float>(p.x)) < 1e-6f &&
                std::abs(unique_verts[i].y() - static_cast<float>(p.y)) < 1e-6f)
            {
                return i;
            }
        }
        throw RuntimeError("Vertex not found");
    };

    for (const auto& path : norm_paths)
    {
        const size_t m = path.size();
        for (size_t i = 0; i < m; ++i)
        {
            size_t j = (i + 1) % m;
            int v0 = findVertIdx(path[i]);
            int v1 = findVertIdx(path[j]);
            int v2 = v1 + n;
            int v3 = v0 + n;

            F.row(f++) << v0, v1, v2;
            F.row(f++) << v0, v2, v3;
        }
    }

    // Create IglModel and convert to CgalModel
    IglModel igl_model(V, F.topRows(f), false);
    auto [v, fa] = igl_model.TriangleMesh();

    if (v.rows() == 0 || fa.rows() == 0)
    {
        throw RuntimeError("IglModel TriangleMesh returned empty result for multi-path");
    }

    CgalModel model(v, fa);
    // 与单多边形重载相同：强制三角化并修正面朝向，保证体积为正
    CGAL::Polygon_mesh_processing::triangulate_faces(model.mesh_);
    if (CGAL::Polygon_mesh_processing::volume(model.mesh_) < 0.0)
    {
        CGAL::Polygon_mesh_processing::reverse_face_orientations(model.mesh_);
    }
    return model;
}

// ========== Surface geodesic / curve / helix operations ==========

namespace detail
{
using SurfaceMesh = CGAL::Surface_mesh<CgalModel::Point_3>;
using ShortestPathTraits = CGAL::Surface_mesh_shortest_path_traits<CgalModel::EpicKernel, SurfaceMesh>;
using ShortestPath = CGAL::Surface_mesh_shortest_path<ShortestPathTraits>;
using AABBPrimitive = CGAL::AABB_face_graph_triangle_primitive<SurfaceMesh>;
using AABBTraits = CGAL::AABB_traits_3<CgalModel::EpicKernel, AABBPrimitive>;
using AABBTree = CGAL::AABB_tree<AABBTraits>;

inline CgalModel::Point_3 ToCgalPoint(const Eigen::Vector3f& p)
{
    return {static_cast<double>(p.x()), static_cast<double>(p.y()), static_cast<double>(p.z())};
}

template <typename PointT>
inline Eigen::Vector3f ToEigen(const PointT& p)
{
    return {static_cast<float>(p.x()), static_cast<float>(p.y()), static_cast<float>(p.z())};
}

/// Convert Polyhedron_3 to a triangulated Surface_mesh
inline SurfaceMesh ToSurfaceMesh(const CgalModel::Polyhedron_3& poly)
{
    SurfaceMesh sm;
    CGAL::copy_face_graph(poly, sm);
    CGAL::Polygon_mesh_processing::triangulate_faces(sm);
    return sm;
}

/// Build AABB tree on a SurfaceMesh
inline AABBTree BuildAABBTree(const SurfaceMesh& sm)
{
    AABBTree tree(sm.faces().begin(), sm.faces().end(), sm);
    tree.accelerate_distance_queries();
    return tree;
}

/// Compute a Face_location (face + barycentric coords) for the closest point on mesh to query
inline ShortestPath::Face_location MakeFaceLocation(const SurfaceMesh& sm, const AABBTree& tree,
                                                    const Eigen::Vector3f& query)
{
    auto pt = ToCgalPoint(query);
    auto projection = tree.closest_point_and_primitive(pt);
    auto closestPt = projection.first;
    auto f = projection.second;  // face_descriptor

    // Get the 3 vertices of the triangular face via member functions
    auto h0 = sm.halfedge(f);
    auto h1 = sm.next(h0);
    auto h2 = sm.next(h1);
    auto va = sm.target(h0);
    auto vb = sm.target(h1);
    auto vc = sm.target(h2);
    const auto& a = sm.point(va);
    const auto& b = sm.point(vb);
    const auto& c = sm.point(vc);

    // Barycentric coordinates via dot products
    auto v0 = c - a;
    auto v1 = b - a;
    auto v2 = closestPt - a;
    double d00 = v0 * v0;
    double d01 = v0 * v1;
    double d11 = v1 * v1;
    double d20 = v2 * v0;
    double d21 = v2 * v1;
    double denom = d00 * d11 - d01 * d01;
    if (std::abs(denom) < 1e-15)
        denom = 1e-15;
    double bw = (d11 * d20 - d01 * d21) / denom;
    double bv = (d00 * d21 - d01 * d20) / denom;
    double bu = 1.0 - bv - bw;

    // Clamp and normalize
    bu = (std::max)(0.0, (std::min)(1.0, bu));
    bv = (std::max)(0.0, (std::min)(1.0, bv));
    bw = (std::max)(0.0, (std::min)(1.0, bw));
    double sum = bu + bv + bw;
    if (sum > 0.0) { bu /= sum; bv /= sum; bw /= sum; }

    // Face_location convention:
    // w0 = source(halfedge(f,sm),sm), w1 = target(halfedge(f,sm),sm), w2 = target(next(halfedge(f,sm),sm),sm)
    auto src0 = sm.source(h0);
    double w0 = 0, w1 = 0, w2 = 0;
    if (src0 == va) { w0 = bu; w1 = bv; w2 = bw; }
    else if (src0 == vb) { w0 = bv; w1 = bw; w2 = bu; }
    else { w0 = bw; w1 = bu; w2 = bv; }

    ShortestPath::Barycentric_coordinates bary;
    bary[0] = w0;
    bary[1] = w1;
    bary[2] = w2;
    return {f, bary};
}
}  // namespace detail

std::vector<Eigen::Vector3f> CgalModel::GeodesicPath(const Eigen::Vector3f& source,
                                                     const Eigen::Vector3f& target) const
{
    auto sm = detail::ToSurfaceMesh(mesh_);
    auto tree = detail::BuildAABBTree(sm);

    detail::ShortestPath shortestPath(sm);
    auto srcLoc = detail::MakeFaceLocation(sm, tree, source);
    shortestPath.add_source_point(srcLoc);

    // Get path to the closest vertex to target
    auto tgtPt = detail::ToCgalPoint(target);
    auto projection = tree.closest_point_and_primitive(tgtPt);
    auto f = projection.second;
    auto h0 = sm.halfedge(f);
    auto v = sm.target(h0);

    std::vector<CgalModel::Point_3> pathPoints;
    shortestPath.shortest_path_points_to_source_points(v, std::back_inserter(pathPoints));

    std::vector<Eigen::Vector3f> result;
    result.reserve(pathPoints.size());
    for (const auto& p : pathPoints)
    {
        result.push_back(detail::ToEigen(p));
    }
    return result;
}

std::vector<float> CgalModel::GeodesicDistance(const Eigen::Vector3f& source) const
{
    auto sm = detail::ToSurfaceMesh(mesh_);
    auto tree = detail::BuildAABBTree(sm);

    detail::ShortestPath shortestPath(sm);
    auto srcLoc = detail::MakeFaceLocation(sm, tree, source);
    shortestPath.add_source_point(srcLoc);

    std::vector<float> distances;
    distances.reserve(sm.number_of_vertices());
    for (const auto& vd : sm.vertices())
    {
        auto res = shortestPath.shortest_distance_to_source_points(vd);
        distances.push_back(static_cast<float>(res.first));
    }
    return distances;
}

Eigen::Vector3f CgalModel::ProjectPointOnSurface(const Eigen::Vector3f& point) const
{
    auto sm = detail::ToSurfaceMesh(mesh_);
    auto tree = detail::BuildAABBTree(sm);
    auto closest = tree.closest_point(detail::ToCgalPoint(point));
    return detail::ToEigen(closest);
}

std::vector<Eigen::Vector3f> CgalModel::SurfaceSpiral(const Eigen::Vector3f& axisOrigin,
                                                      const Eigen::Vector3f& axisDirection, float turns,
                                                      int samplesPerTurn, float startRadius,
                                                      float endRadius) const
{
    if (turns <= 0.0f || samplesPerTurn < 3)
    {
        throw InvalidArgumentError("SurfaceSpiral: turns must be > 0 and samplesPerTurn >= 3");
    }

    auto sm = detail::ToSurfaceMesh(mesh_);
    auto tree = detail::BuildAABBTree(sm);

    Eigen::Vector3f axis = axisDirection.normalized();
    Eigen::Vector3f u = axis.unitOrthogonal();
    Eigen::Vector3f w = axis.cross(u).normalized();

    Eigen::Vector3f bmin, bmax;
    BoundingBox(bmin, bmax);
    float projMin = axis.dot(bmin - axisOrigin);
    float projMax = axis.dot(bmax - axisOrigin);
    float heightRange = projMax - projMin;

    if (endRadius < 0.0f)
    {
        endRadius = (bmax - bmin).norm() * 0.5f;
    }
    if (startRadius <= 0.0f)
    {
        startRadius = endRadius * 0.1f;
    }

    const int totalSamples = static_cast<int>(turns * samplesPerTurn);
    std::vector<Eigen::Vector3f> result;
    result.reserve(totalSamples);

    for (int i = 0; i < totalSamples; ++i)
    {
        float t = static_cast<float>(i) / static_cast<float>(totalSamples - 1);
        float angle = t * turns * 2.0f * std::numbers::pi_v<float>;
        float radius = startRadius + (endRadius - startRadius) * t;
        float height = projMin + heightRange * t;

        Eigen::Vector3f candidate = axisOrigin + axis * height + radius * (u * std::cos(angle) + w * std::sin(angle));
        auto closest = tree.closest_point(detail::ToCgalPoint(candidate));
        result.push_back(detail::ToEigen(closest));
    }
    return result;
}

std::vector<Eigen::Vector3f> CgalModel::SurfaceHelix(const Eigen::Vector3f& axisOrigin,
                                                     const Eigen::Vector3f& axisDirection, float turns, float pitch,
                                                     float radius, int samplesPerTurn) const
{
    if (turns <= 0.0f || samplesPerTurn < 3)
    {
        throw InvalidArgumentError("SurfaceHelix: turns must be > 0 and samplesPerTurn >= 3");
    }

    auto sm = detail::ToSurfaceMesh(mesh_);
    auto tree = detail::BuildAABBTree(sm);

    Eigen::Vector3f axis = axisDirection.normalized();
    Eigen::Vector3f u = axis.unitOrthogonal();
    Eigen::Vector3f w = axis.cross(u).normalized();

    const int totalSamples = static_cast<int>(turns * samplesPerTurn);
    std::vector<Eigen::Vector3f> result;
    result.reserve(totalSamples);

    for (int i = 0; i < totalSamples; ++i)
    {
        float t = static_cast<float>(i) / static_cast<float>(totalSamples - 1);
        float angle = t * turns * 2.0f * std::numbers::pi_v<float>;
        float height = t * turns * pitch;

        Eigen::Vector3f candidate = axisOrigin + axis * height + radius * (u * std::cos(angle) + w * std::sin(angle));
        auto closest = tree.closest_point(detail::ToCgalPoint(candidate));
        result.push_back(detail::ToEigen(closest));
    }
    return result;
}

}  // namespace HsBa::Slicer

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