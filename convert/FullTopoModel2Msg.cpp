#include "FullTopoModel2Msg.hpp"

namespace HsBa::Slicer
{

void FullTopoModel2Msg(const FullTopoModel& model, HsbaProto::msg_topo_trimeshes* msg)
{
    if (msg == nullptr)
    {
        return;
    }
    msg->Clear();

    for (const auto& vertex : model.GetVertices())
    {
        HsbaProto::msg_point3* point = msg->add_vetex();
        point->set_x(vertex.vertex.x());
        point->set_y(vertex.vertex.y());
        point->set_z(vertex.vertex.z());
    }

    for (const auto& face : model.GetFaces())
    {
        HsbaProto::msg_topo_trimeshes::msg_topo_trimesh* triangle = msg->add_trimesh();
        triangle->set_p0(static_cast<uint32_t>(face.triangle[0]));
        triangle->set_p1(static_cast<uint32_t>(face.triangle[1]));
        triangle->set_p2(static_cast<uint32_t>(face.triangle[2]));
        if (face.normal != Eigen::Vector3f::Zero())
        {
            HsbaProto::msg_vector3* normal = triangle->mutable_normal();
            normal->set_x(face.normal.x());
            normal->set_y(face.normal.y());
            normal->set_z(face.normal.z());
        }
    }
}

}  // namespace HsBa::Slicer
