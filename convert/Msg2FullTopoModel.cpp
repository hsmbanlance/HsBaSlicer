#include "Msg2FullTopoModel.hpp"

namespace HsBa::Slicer
{

FullTopoModel MsgTopoTrimeshes2FullTopoModel(const HsbaProto::msg_topo_trimeshes& msg, bool use_normals)
{
    std::vector<Eigen::Vector3f> vertices;
    vertices.reserve(msg.vetex_size());
    for (const auto& vertex_msg : msg.vetex())
    {
        vertices.emplace_back(vertex_msg.x(), vertex_msg.y(), vertex_msg.z());
    }

    std::vector<std::array<int, 3>> triangles;
    triangles.reserve(msg.trimesh_size());
    for (const auto& triangle_msg : msg.trimesh())
    {
        int p0 = static_cast<int>(triangle_msg.p0());
        int p1 = static_cast<int>(triangle_msg.p1());
        int p2 = static_cast<int>(triangle_msg.p2());
        if (p0 < 0 || p1 < 0 || p2 < 0 || p0 >= static_cast<int>(vertices.size()) || p1 >= static_cast<int>(vertices.size()) || p2 >= static_cast<int>(vertices.size()))
        {
            continue;
        }
        triangles.push_back({{p0, p1, p2}});
    }

    return FullTopoModel(vertices, triangles, use_normals);
}

}  // namespace HsBa::Slicer
