/** @file IglModel.cpp
 * @brief Implementation of the libigl-backed triangle mesh model.
 * @author HsBa
 */
#include "IglModel.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <numeric>

#include <igl/read_triangle_mesh.h>
#include <igl/writeOBJ.h>
#include <igl/writeOFF.h>
#include <igl/writePLY.h>
#include <igl/writeSTL.h>

#include <igl/per_edge_normals.h>
#include <igl/per_face_normals.h>
#include <igl/per_vertex_normals.h>
#include <igl/vertex_triangle_adjacency.h>
#include <igl/volume.h>

#ifdef USE_CGAL
#include "CgalModel.hpp"
#include <igl/copyleft/cgal/mesh_boolean.h>
#endif

#include "base/ModelFormat.hpp"
#include "base/encoding_convert.hpp"
#include "base/error.hpp"
#include <algorithm>
#include <cmath>

namespace HsBa::Slicer
{
IglModel::IglModel(const Eigen::MatrixXf& vertices, const Eigen::MatrixXi& faces, bool calcNormals)
    : vertices_(vertices), faces_(faces)
{
    if (calcNormals)
    {
        ComputeNormals();
    }
}
IglModel::IglModel(const Eigen::MatrixXf& vertices, const Eigen::MatrixXi& faces, const Eigen::MatrixXf& normals)
    : vertices_(vertices), faces_(faces), normals_(normals)
{
}

IglModel::IglModel(Eigen::MatrixXf&& vertices, Eigen::MatrixXi&& faces, bool calcNormals)
    : vertices_(std::move(vertices)), faces_(std::move(faces))
{
    if (calcNormals)
    {
        ComputeNormals();
    }
}

IglModel::IglModel(Eigen::MatrixXf&& vertices, Eigen::MatrixXi&& faces, Eigen::MatrixXf&& normals)
    : vertices_(std::move(vertices)), faces_(std::move(faces)), normals_(std::move(normals))
{
}

bool IglModel::Load(std::string_view filename)
{
    fileName_ = filename;
    // convert to system coding path
    std::string filename_ansi = utf8_to_local(fileName_);
    return igl::read_triangle_mesh(filename_ansi, vertices_, faces_);
}

bool IglModel::Save(std::string_view filename, ModelFormat format) const
{
    std::string filename_ansi = utf8_to_local(std::string{filename});
    if (IsMeshFormat(format))
    {
        switch (format)
        {
        case ModelFormat::BinarySTL:
            return igl::writeSTL(filename_ansi, vertices_, faces_, igl::FileEncoding::Binary);
        case ModelFormat::ASCIISTL:
            return igl::writeSTL(filename_ansi, vertices_, faces_, igl::FileEncoding::Ascii);
        case ModelFormat::OFF:
            return igl::writeOFF(filename_ansi, vertices_, faces_);
        case ModelFormat::OBJ:
            return igl::writeOBJ(filename_ansi, vertices_, faces_);
        case ModelFormat::ASCIIPLY:
            return igl::writePLY(filename_ansi, vertices_, faces_, igl::FileEncoding::Ascii);
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
    // Vertices are stored row-wise (N×3): MatrixXf += Vector3f has a size mismatch
    // and is undefined behavior (out-of-bounds read) in Release; use rowwise broadcast to add per row
    vertices_.rowwise() += translation.transpose();
}
void IglModel::Rotate(const Eigen::Quaternionf& rotation)
{
    Eigen::Matrix3f rotationMatrix = rotation.toRotationMatrix();
    // With row storage, p' = R·p is equivalent to right-multiplying the row vector by R^T;
    // the rotation matrix is 3×3, so the previous rotationMatrix * vertices_ (3×3 times N×3) was an invalid size
    vertices_ = vertices_ * rotationMatrix.transpose();
    if (normals_.cols() == faces_.cols())
    {
        normals_ = normals_ * rotationMatrix.transpose();
    }
}
void IglModel::Scale(const float scale)
{
    vertices_ *= scale;
}
void IglModel::Scale(const Eigen::Vector3f& scaleFactors)
{
    vertices_ = vertices_.array().rowwise() * scaleFactors.transpose().array();
}
void IglModel::Transform(const Eigen::Isometry3f& transform)
{
    // Row-storage homogeneous transform: p'^T = p_h^T · M^T (p_h is the homogeneous row vector with a trailing 1)
    vertices_ = (vertices_.rowwise().homogeneous() * transform.matrix().transpose()).rowwise().hnormalized();
    if (normals_.cols() == faces_.cols())
    {
        normals_ = normals_ * transform.rotation().transpose();
    }
}
void IglModel::Transform(const Eigen::Matrix4f& transform)
{
    vertices_ = (vertices_.rowwise().homogeneous() * transform.transpose()).rowwise().hnormalized();
    if (normals_.cols() == faces_.cols())
    {
        normals_ = normals_ * transform.block<3, 3>(0, 0).transpose();
    }
}
void IglModel::Transform(const Eigen::Transform<float, 3, Eigen::Affine>& transform)
{
    vertices_ = (vertices_.rowwise().homogeneous() * transform.matrix().transpose()).rowwise().hnormalized();
    if (normals_.cols() == faces_.cols())
    {
        normals_ = normals_ * transform.rotation().transpose();
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


std::pair<Eigen::MatrixXf, Eigen::MatrixXi> IglModel::TriangleMesh() const
{
    Eigen::MatrixXf v = vertices_;
    Eigen::MatrixXi f = faces_;
    return std::make_pair(v, f);
}

#ifdef USE_CGAL
namespace
{
// igl::copyleft::cgal::mesh_boolean relies on the CGAL exact geometry kernel; float vertices may
// produce an inconsistent winding-number field and fail silently (returning false with an empty mesh),
// so they must first be promoted to double; if it still fails, fall back to CgalModel's Nef polyhedron
// boolean (same CGAL kernel, verified reliable in tests)
bool IglMeshBooleanImpl(const Eigen::MatrixXf& va, const Eigen::MatrixXi& fa, const Eigen::MatrixXf& vb,
                        const Eigen::MatrixXi& fb, igl::MeshBooleanType type, Eigen::MatrixXf& v_out,
                        Eigen::MatrixXi& f_out)
{
    Eigen::MatrixXd da = va.cast<double>();
    Eigen::MatrixXd db = vb.cast<double>();
    Eigen::MatrixXd vc;
    Eigen::MatrixXi fc;
    const bool ok = igl::copyleft::cgal::mesh_boolean(da, fa, db, fb, type, vc, fc);
    if (ok && fc.rows() > 0)
    {
        v_out = vc.cast<float>();
        f_out = fc;
        return true;
    }

    // Fallback: Nef boolean via CgalModel
    CgalModel ca(va, fa);
    CgalModel cb(vb, fb);
    CgalModel rc = ca;
    switch (type)
    {
    case igl::MESH_BOOLEAN_TYPE_UNION:
        rc = Union(ca, cb);
        break;
    case igl::MESH_BOOLEAN_TYPE_INTERSECT:
        rc = Intersection(ca, cb);
        break;
    case igl::MESH_BOOLEAN_TYPE_MINUS:
        rc = Difference(ca, cb);
        break;
    case igl::MESH_BOOLEAN_TYPE_XOR:
        rc = Xor(ca, cb);
        break;
    default:
        return false;
    }
    auto [rv, rf] = rc.TriangleMesh();
    if (rf.rows() == 0)
        return false;
    v_out = rv;
    f_out = rf;
    return true;
}
}  // namespace

IglModel Union(const IglModel& left, const IglModel& right)
{
    auto is_valid_mesh = [](const Eigen::MatrixXf& V, const Eigen::MatrixXi& F) -> bool
    {
        if (V.rows() == 0 || F.rows() == 0)
            return false;
        if (V.cols() < 3)
            return false;
        if (F.cols() < 3)
            return false;
        // finite check
        for (int r = 0; r < V.rows(); ++r)
        {
            for (int c = 0; c < V.cols(); ++c)
            {
                float val = V(r, c);
                if (!std::isfinite(val))
                    return false;
            }
        }
        // indices bounds
        int nv = V.rows();
        for (int r = 0; r < F.rows(); ++r)
        {
            for (int c = 0; c < F.cols(); ++c)
            {
                int idx = F(r, c);
                if (idx < 0 || idx >= nv)
                    return false;
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

    IglMeshBooleanImpl(left.vertices_, left.faces_, right.vertices_, right.faces_, igl::MESH_BOOLEAN_TYPE_UNION, v, f);

    if (v.rows() == 0 || f.rows() == 0)
        return IglModel(Eigen::MatrixXf(), Eigen::MatrixXi());
    return IglModel(v, f);
}
IglModel Intersection(const IglModel& left, const IglModel& right)
{
    auto is_valid_mesh = [](const Eigen::MatrixXf& V, const Eigen::MatrixXi& F) -> bool
    {
        if (V.rows() == 0 || F.rows() == 0)
            return false;
        if (V.cols() < 3)
            return false;
        if (F.cols() < 3)
            return false;
        for (int r = 0; r < V.rows(); ++r)
            for (int c = 0; c < V.cols(); ++c)
                if (!std::isfinite(V(r, c)))
                    return false;
        int nv = V.rows();
        for (int r = 0; r < F.rows(); ++r)
            for (int c = 0; c < F.cols(); ++c)
                if (F(r, c) < 0 || F(r, c) >= nv)
                    return false;
        return true;
    };
    Eigen::MatrixXf v;
    Eigen::MatrixXi f;
    if (!is_valid_mesh(left.vertices_, left.faces_) || !is_valid_mesh(right.vertices_, right.faces_))
        return IglModel(Eigen::MatrixXf(), Eigen::MatrixXi());

    IglMeshBooleanImpl(left.vertices_, left.faces_, right.vertices_, right.faces_, igl::MESH_BOOLEAN_TYPE_INTERSECT, v,
                       f);

    if (v.rows() == 0 || f.rows() == 0)
        return IglModel(Eigen::MatrixXf(), Eigen::MatrixXi());
    return IglModel(v, f);
}
IglModel Difference(const IglModel& left, const IglModel& right)
{
    auto is_valid_mesh = [](const Eigen::MatrixXf& V, const Eigen::MatrixXi& F) -> bool
    {
        if (V.rows() == 0 || F.rows() == 0)
            return false;
        if (V.cols() < 3)
            return false;
        if (F.cols() < 3)
            return false;
        for (int r = 0; r < V.rows(); ++r)
            for (int c = 0; c < V.cols(); ++c)
                if (!std::isfinite(V(r, c)))
                    return false;
        int nv = V.rows();
        for (int r = 0; r < F.rows(); ++r)
            for (int c = 0; c < F.cols(); ++c)
                if (F(r, c) < 0 || F(r, c) >= nv)
                    return false;
        return true;
    };
    Eigen::MatrixXf v;
    Eigen::MatrixXi f;
    if (!is_valid_mesh(left.vertices_, left.faces_) || !is_valid_mesh(right.vertices_, right.faces_))
        return IglModel(Eigen::MatrixXf(), Eigen::MatrixXi());

    IglMeshBooleanImpl(left.vertices_, left.faces_, right.vertices_, right.faces_, igl::MESH_BOOLEAN_TYPE_MINUS, v, f);

    if (v.rows() == 0 || f.rows() == 0)
        return IglModel(Eigen::MatrixXf(), Eigen::MatrixXi());
    return IglModel(v, f);
}
IglModel Xor(const IglModel& left, const IglModel& right)
{
    auto is_valid_mesh = [](const Eigen::MatrixXf& V, const Eigen::MatrixXi& F) -> bool
    {
        if (V.rows() == 0 || F.rows() == 0)
            return false;
        if (V.cols() < 3)
            return false;
        if (F.cols() < 3)
            return false;
        for (int r = 0; r < V.rows(); ++r)
            for (int c = 0; c < V.cols(); ++c)
                if (!std::isfinite(V(r, c)))
                    return false;
        int nv = V.rows();
        for (int r = 0; r < F.rows(); ++r)
            for (int c = 0; c < F.cols(); ++c)
                if (F(r, c) < 0 || F(r, c) >= nv)
                    return false;
        return true;
    };
    Eigen::MatrixXf v;
    Eigen::MatrixXi f;
    if (!is_valid_mesh(left.vertices_, left.faces_) || !is_valid_mesh(right.vertices_, right.faces_))
        return IglModel(Eigen::MatrixXf(), Eigen::MatrixXi());

    IglMeshBooleanImpl(left.vertices_, left.faces_, right.vertices_, right.faces_, igl::MESH_BOOLEAN_TYPE_XOR, v, f);

    if (v.rows() == 0 || f.rows() == 0)
        return IglModel(Eigen::MatrixXf(), Eigen::MatrixXi());
    return IglModel(v, f);
}
#else
IglModel Union(const IglModel& left, const IglModel& right)
{
    throw NotSupportedError("Boolean operations are not supported without CGAL.");
}
IglModel Intersection(const IglModel& left, const IglModel& right)
{
    throw NotSupportedError("Boolean operations are not supported without CGAL.");
}
IglModel Difference(const IglModel& left, const IglModel& right)
{
    throw NotSupportedError("Boolean operations are not supported without CGAL.");
}
IglModel Xor(const IglModel& left, const IglModel& right)
{
    throw NotSupportedError("Boolean operations are not supported without CGAL.");
}
#endif

IglModel IglModel::CreateBox(const Eigen::Vector3f& size)
{
    const Eigen::Vector3f h = size * 0.5f;
    std::vector<Eigen::Vector3f> verts{{-h.x(), -h.y(), -h.z()}, {h.x(), -h.y(), -h.z()}, {h.x(), h.y(), -h.z()},
                                       {-h.x(), h.y(), -h.z()},  {-h.x(), -h.y(), h.z()}, {h.x(), -h.y(), h.z()},
                                       {h.x(), h.y(), h.z()},    {-h.x(), h.y(), h.z()}};
    // Winding must orient normals outward: winding-number-based booleans such as mesh_boolean
    // rely on a consistent outward normal orientation (when all normals previously pointed inward, the
    // winding number was -1 and boolean classification was wrong)
    std::vector<Eigen::Vector3i> faces{
        {0, 2, 1}, {0, 3, 2},  // bottom
        {4, 5, 6}, {4, 6, 7},  // top
        {0, 5, 4}, {0, 1, 5},  // -y
        {1, 2, 6}, {1, 6, 5},  // +x
        {2, 3, 7}, {2, 7, 6},  // +y
        {3, 0, 4}, {3, 4, 7}   // -x
    };
    Eigen::MatrixXf v(verts.size(), 3);
    Eigen::MatrixXi f(faces.size(), 3);
    for (size_t i = 0; i < verts.size(); ++i)
        v.row((int)i) = verts[i];
    for (size_t i = 0; i < faces.size(); ++i)
        f.row((int)i) = faces[i];
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
        float theta = v * std::numbers::pi_v<float>;  // 0..pi
        for (int j = 0; j < slices; ++j)
        {
            float u = (float)j / (float)slices;
            float phi = u * 2.0f * std::numbers::pi_v<float>;  // 0..2pi
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
    verts.emplace_back(0, 0, -h2);
    int topCenter = (int)verts.size();
    verts.emplace_back(0, 0, h2);
    // sides
    for (int i = 0; i < seg; ++i)
    {
        int i0 = i * 2;
        int i1 = ((i + 1) % seg) * 2;
        // quad -> two triangles; when the ring is CCW (angle increasing), the following winding orients normals outward
        faces.emplace_back(i0, i1, i0 + 1);
        faces.emplace_back(i1, i1 + 1, i0 + 1);
        // bottom cap (normal points down)
        faces.emplace_back(bottomCenter, i1, i0);
        // top cap (normal points up)
        faces.emplace_back(topCenter, i0 + 1, i1 + 1);
    }
    Eigen::MatrixXf v(verts.size(), 3);
    Eigen::MatrixXi f(faces.size(), 3);
    for (size_t i = 0; i < verts.size(); ++i)
        v.row((int)i) = verts[i];
    for (size_t i = 0; i < faces.size(); ++i)
        f.row((int)i) = faces[i];
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
    return IglModel(std::move(v), std::move(f), true);
}

IglModel IglModel::CreateTorus(const float majorRadius, const float minorRadius, const int majorSegments,
                               const int minorSegments)
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
    for (size_t i = 0; i < verts.size(); ++i)
        v.row((int)i) = verts[i];
    for (size_t i = 0; i < faces.size(); ++i)
        f.row((int)i) = faces[i];
    return IglModel(std::move(v), std::move(f), true);
}

IglModel IglModel::CreatePrime(const PolygonD& poly, const Eigen::Vector3f& direction)
{
    const size_t n = poly.size();
    if (n < 3)
    {
        throw InvalidArgumentError("Polygon must have at least 3 points");
    }

    const Eigen::Vector3f dir(direction.x(), direction.y(), direction.z());

    // Normalize winding: cap/side construction assumes the base polygon is CCW (Clipper2 Area > 0 in
    // Cartesian coordinates); if the input is CW, reverse it to avoid inward-facing cap and side normals
    PolygonD ccw_poly = poly;
    if (Clipper2Lib::Area(ccw_poly) < 0.0)
    {
        std::reverse(ccw_poly.begin(), ccw_poly.end());
    }

    // ========== 1. Triangulate the base face (Clipper2) ==========
    Clipper2Lib::PathsD paths_in{ccw_poly};
    Clipper2Lib::PathsD triangles;

    auto result = Clipper2Lib::Triangulate(paths_in, 0, triangles, true);
    if (result != Clipper2Lib::TriangulateResult::success)
    {
        throw RuntimeError("Triangulation failed");
    }

    // Map the triangulation result back to original vertex indices
    auto findIndex = [&](const Clipper2Lib::PointD& p) -> int
    {
        for (int i = 0; i < static_cast<int>(n); ++i)
        {
            if (std::abs(ccw_poly[i].x - p.x) < 1e-9 && std::abs(ccw_poly[i].y - p.y) < 1e-9)
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
        // Force base triangles to CCW (positive signed area in Cartesian coordinates), not relying on Triangulate's output winding
        if (Clipper2Lib::Area(tri) < 0.0)
        {
            std::swap(idx[1], idx[2]);
        }
        bottom_tris.push_back(idx);
    }

    // Clipper2 Triangulate produces no triangles for already-triangular input; fall back to using the
    // original triangle as the base face (ccw_poly is already normalized to CCW, so {0,1,2} winding is CCW)
    if (bottom_tris.empty() && n == 3)
    {
        bottom_tris.push_back({0, 1, 2});
    }

    // ========== 2. Build 3D vertices ==========
    Eigen::MatrixXf V(2 * n, 3);
    for (size_t i = 0; i < n; ++i)
    {
        V.row(i) << static_cast<float>(ccw_poly[i].x), static_cast<float>(ccw_poly[i].y), 0.0f;
        V.row(i + n) = V.row(i) + dir.transpose();
    }

    // ========== 3. Build faces ==========
    const int n_bottom = static_cast<int>(bottom_tris.size());
    const int n_side = 2 * static_cast<int>(n);
    Eigen::MatrixXi F(2 * n_bottom + n_side, 3);
    int f = 0;

    // Base face: normal points down (-Z), reverse winding
    for (const auto& tri : bottom_tris)
    {
        F.row(f++) << tri[0], tri[2], tri[1];
    }

    // Top face: normal points up (+Z), keep CCW winding
    for (const auto& tri : bottom_tris)
    {
        F.row(f++) << tri[0] + n, tri[1] + n, tri[2] + n;
    }

    // Side faces: based on original polygon edges
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

    IglModel model;
    model.vertices_ = V;
    model.faces_ = F.topRows(f);  // Only use the actually filled rows
    return model;
}

IglModel IglModel::CreatePrime(const PolygonsD& paths, const Eigen::Vector3f& direction)
{
    if (paths.empty())
    {
        throw InvalidArgumentError("Paths must not be empty");
    }

    const Eigen::Vector3f dir(direction.x(), direction.y(), direction.z());

    // Normalize winding: outer contours CCW (Area > 0 in Cartesian coordinates), holes CW (Area < 0).
    // Clipper2 Triangulate identifies holes by this convention (its y-down convention is "outer clockwise,
    // inner counter-clockwise", corresponding to Cartesian signed area outer>0 / hole<0);
    // if outer contours and holes share the same sign, holes are triangulated as independent solids,
    // inflating the volume. Holes are determined by geometric containment, not relying on the caller's winding.
    Clipper2Lib::PathsD norm_paths = paths;
    std::vector<bool> is_hole(norm_paths.size(), false);
    // Determine holes by nesting-depth parity: the depth of path i = the number of other paths strictly
    // containing it; an odd depth means a hole. Containment uses "majority of vertices inside" rather than the
    // centroid - the centroid may fall inside an inner sub-path (e.g. an outer box's centroid lands exactly in
    // an inner hole), causing an outer contour to be misclassified as a hole and its winding fully reversed.
    // Note: Clipper2Lib::PointInPolygon cannot be called directly on double paths - its MSVC branch is written
    // for int64 exact arithmetic (TriSign only has int64_t overloads, double is implicitly truncated), so
    // non-integer coordinates are misjudged as collinear and return IsOn. Following project convention, first
    // integerize (x integerization) to Path64, then test containment (int64 exact arithmetic); containment is
    // invariant under scaling, so no de-integerization is needed.
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

    // ========== 1. Triangulate the base face (Clipper2) ==========
    Clipper2Lib::PathsD triangles;
    auto result = Clipper2Lib::Triangulate(norm_paths, 0, triangles, true);
    if (result != Clipper2Lib::TriangulateResult::success)
    {
        throw RuntimeError("Triangulation failed");
    }

    // ========== 2. Collect all unique vertices ==========
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

    // First collect vertices of all original paths (ensuring side faces can be linked correctly)
    for (const auto& path : paths)
    {
        for (const auto& pt : path)
        {
            findOrAdd(pt);
        }
    }

    // Then collect new vertices possibly introduced by triangulation (simple polygons usually have none)
    for (const auto& tri : triangles)
    {
        for (const auto& pt : tri)
        {
            findOrAdd(pt);
        }
    }

    const int n = static_cast<int>(unique_verts.size());

    // ========== 3. Build base-triangle indices ==========
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
        // Force base triangles to CCW (positive signed area in Cartesian coordinates), not relying on Triangulate's output winding;
        // hole regions are not triangulated (already passed in per convention), so all triangles belong to solid regions
        if (Clipper2Lib::Area(tri) < 0.0)
        {
            std::swap(idx[1], idx[2]);
        }
        bottom_tris.push_back(idx);
    }

    // Clipper2 Triangulate produces no triangles for triangular paths; append the original triangle paths to complete the base face, avoiding lost caps;
    // outer-contour triangles are normalized to CCW, hole triangles to CW (cap contributions cancel)
    const size_t trianglePathCount =
        std::count_if(paths.begin(), paths.end(), [](const PolygonD& p) { return p.size() == 3; });
    if (bottom_tris.size() < trianglePathCount)
    {
        for (size_t pi = 0; pi < paths.size(); ++pi)
        {
            const auto& path = paths[pi];
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

    // ========== 4. Build 3D vertices ==========
    Eigen::MatrixXf V(2 * n, 3);
    for (int i = 0; i < n; ++i)
    {
        V.row(i) << unique_verts[i].x(), unique_verts[i].y(), 0.0f;
        V.row(i + n) = V.row(i) + dir.transpose();
    }

    // ========== 5. Build faces ==========
    const int n_bottom = static_cast<int>(bottom_tris.size());

    // Side faces: per edge of the normalized paths (holes are CW, ensuring wall normals point into the hole)
    int n_side_tris = 0;
    for (const auto& path : norm_paths)
    {
        n_side_tris += 2 * static_cast<int>(path.size());
    }

    Eigen::MatrixXi F(2 * n_bottom + n_side_tris, 3);
    int f = 0;

    // Base face: normal points down (-Z), reverse winding
    for (const auto& tri : bottom_tris)
    {
        F.row(f++) << tri[0], tri[2], tri[1];
    }

    // Top face: normal points up (+Z), keep CCW winding
    for (const auto& tri : bottom_tris)
    {
        F.row(f++) << tri[0] + n, tri[1] + n, tri[2] + n;
    }

    // Side faces: based on edges of the original paths
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

    IglModel model;
    model.vertices_ = V;
    model.faces_ = F.topRows(f);
    return model;
}

}  // namespace HsBa::Slicer
