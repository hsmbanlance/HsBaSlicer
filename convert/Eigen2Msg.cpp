#include "Eigen2Msg.hpp"

namespace HsBa::Slicer
{
    void EigenVector3f2Msg(const Eigen::Vector3f& eigen, HsbaProto::msg_vector3* msg)
    {
        msg->set_x(eigen.x());
        msg->set_y(eigen.y());
        msg->set_z(eigen.z());
    }
    void EigenVector3f2Msg(const Eigen::Vector3f& eigen, HsbaProto::msg_point3* msg)
    {
        msg->set_x(eigen.x());
        msg->set_y(eigen.y());
        msg->set_z(eigen.z());
    }
    void EigenVector2f2Msg(const Eigen::Vector2f& eigen, HsbaProto::msg_vector2* msg)
    {
        msg->set_x(eigen.x());
        msg->set_y(eigen.y());
    }
    void EigenVector2f2Msg(const Eigen::Vector2f& eigen, HsbaProto::msg_point2* msg)
    {
        msg->set_x(eigen.x());
        msg->set_y(eigen.y());
    }
    void EigenQuaternionf2Msg(const Eigen::Quaternionf& eigen, HsbaProto::msg_quaternion* msg)
    {
        msg->set_x(eigen.x());
        msg->set_y(eigen.y());
        msg->set_z(eigen.z());
        msg->set_w(eigen.w());
    }

    void EigenTransform3f2Msg(const Eigen::Transform<float, 3, Eigen::Affine>& eigen, HsbaProto::msg_transform3* msg)
    {
        msg->add_matrix(eigen(0, 0));
        msg->add_matrix(eigen(0, 1));
        msg->add_matrix(eigen(0, 2));
        msg->add_matrix(eigen(0, 3));
        msg->add_matrix(eigen(1, 0));
        msg->add_matrix(eigen(1, 1));
        msg->add_matrix(eigen(1, 2));
        msg->add_matrix(eigen(1, 3));
        msg->add_matrix(eigen(2, 0));
        msg->add_matrix(eigen(2, 1));
        msg->add_matrix(eigen(2, 2));
        msg->add_matrix(eigen(2, 3));
        msg->add_matrix(eigen(3, 0));
        msg->add_matrix(eigen(3, 1));
        msg->add_matrix(eigen(3, 2));
        msg->add_matrix(eigen(3, 3));
    }
    void EigenTransform2f2Msg(const Eigen::Transform<float, 2, Eigen::Affine>& eigen, HsbaProto::msg_transform2* msg)
    {
        msg->add_matrix(eigen(0, 0));
        msg->add_matrix(eigen(0, 1));
        msg->add_matrix(eigen(0, 2));
        msg->add_matrix(eigen(1, 0));
        msg->add_matrix(eigen(1, 1));
        msg->add_matrix(eigen(1, 2));
        msg->add_matrix(eigen(2, 0));
        msg->add_matrix(eigen(2, 1));
        msg->add_matrix(eigen(2, 2));
    }
    void EigenIsometric3f2Msg(const Eigen::Isometry3f& eigen, HsbaProto::msg_transform3* msg)
    {
        msg->add_matrix(eigen(0, 0));
        msg->add_matrix(eigen(0, 1));
        msg->add_matrix(eigen(0, 2));
        msg->add_matrix(eigen(0, 3));
        msg->add_matrix(eigen(1, 0));
        msg->add_matrix(eigen(1, 1));
        msg->add_matrix(eigen(1, 2));
        msg->add_matrix(eigen(1, 3));
        msg->add_matrix(eigen(2, 0));
        msg->add_matrix(eigen(2, 1));
        msg->add_matrix(eigen(2, 2));
        msg->add_matrix(eigen(2, 3));
        msg->add_matrix(eigen(3, 0));
        msg->add_matrix(eigen(3, 1));
        msg->add_matrix(eigen(3, 2));
        msg->add_matrix(eigen(3, 3));
    }
    void EigenIsometric2f2Msg(const Eigen::Isometry2f& eigen, HsbaProto::msg_transform2* msg)
    {
        msg->add_matrix(eigen(0, 0));
        msg->add_matrix(eigen(0, 1));
        msg->add_matrix(eigen(0, 2));
        msg->add_matrix(eigen(1, 0));
        msg->add_matrix(eigen(1, 1));
        msg->add_matrix(eigen(1, 2));
        msg->add_matrix(eigen(2, 0));
        msg->add_matrix(eigen(2, 1));
        msg->add_matrix(eigen(2, 2));
    }
    void EigenMatrix3f2Msg(const Eigen::Matrix4f& eigen, HsbaProto::msg_transform3* msg)
    {
        msg->add_matrix(eigen(0, 0));
        msg->add_matrix(eigen(0, 1));
        msg->add_matrix(eigen(0, 2));
        msg->add_matrix(eigen(0, 3));
        msg->add_matrix(eigen(1, 0));
        msg->add_matrix(eigen(1, 1));
        msg->add_matrix(eigen(1, 2));
        msg->add_matrix(eigen(1, 3));
        msg->add_matrix(eigen(2, 0));
        msg->add_matrix(eigen(2, 1));
        msg->add_matrix(eigen(2, 2));
        msg->add_matrix(eigen(2, 3));
        msg->add_matrix(eigen(3, 0));
        msg->add_matrix(eigen(3, 1));
        msg->add_matrix(eigen(3, 2));
        msg->add_matrix(eigen(3, 3));
    }
    void EigenMatrix2f2Msg(const Eigen::Matrix3f& eigen, HsbaProto::msg_transform2* msg)
    {
        msg->add_matrix(eigen(0, 0));
        msg->add_matrix(eigen(0, 1));
        msg->add_matrix(eigen(0, 2));
        msg->add_matrix(eigen(1, 0));
        msg->add_matrix(eigen(1, 1));
        msg->add_matrix(eigen(1, 2));
        msg->add_matrix(eigen(2, 0));
        msg->add_matrix(eigen(2, 1));
        msg->add_matrix(eigen(2, 2));
    }

    void EigenPath2Msg(const std::vector<Eigen::Vector3f>& eigen, HsbaProto::msg_path3* msg)
    {
        for (const auto& eigen_point : eigen)
        {
            HsbaProto::msg_point3* point = msg->add_point();
            EigenVector3f2Msg(eigen_point, point);
        }
    }
    void EigenPath2Msg(const std::vector<Eigen::Vector2f>& eigen, HsbaProto::msg_path2* msg)
    {
        for (const auto& eigen_point : eigen)
        {
            HsbaProto::msg_point2* point = msg->add_point();
            EigenVector2f2Msg(eigen_point, point);
        }
    }
}// namespace HsBa::Slicer