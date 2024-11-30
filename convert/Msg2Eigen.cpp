#include "Msg2Eigen.hpp"

#include "base/error.hpp"

namespace HsBa::Slicer
{
    void MsgVector3f2Eigen(const HsbaProto::msg_vector3& msg, Eigen::Vector3f& eigen)
    {
        eigen << msg.x(), msg.y(), msg.z();
    }
    void MsgPoint3f2Eigen(const HsbaProto::msg_point3& msg, Eigen::Vector3f& eigen)
    {
        eigen << msg.x(), msg.y(), msg.z();
    }
    void MsgVector2f2Eigen(const HsbaProto::msg_vector2& msg, Eigen::Vector2f& eigen)
    {
        eigen << msg.x(), msg.y();
    }
    void MsgPoint2f2Eigen(const HsbaProto::msg_point2& msg, Eigen::Vector2f& eigen)
    {
        eigen << msg.x(), msg.y();
    }

    void MsgTransform3f2Eigen(const HsbaProto::msg_transform3& msg, Eigen::Transform<float, 3, Eigen::Affine>& eigen)
    {
        if(msg.matrix_size() != 16)
        {
            throw InvalidArgumentError("MsgTransform3f2Eigen: matrix_size != 16");
        }
        Eigen::Matrix<float, 4, 4> mat;
        for(int i = 0; i < 16; i++)
        {
            mat(i / 4, i % 4) = msg.matrix(i);
        }
        eigen = Eigen::Transform<float, 3, Eigen::Affine>(mat);
    }
    void MsgTransform2f2Eigen(const HsbaProto::msg_transform2& msg, Eigen::Transform<float, 2, Eigen::Affine>& eigen)
    {
        if(msg.matrix_size() != 9)
        {
            throw InvalidArgumentError("MsgTransform2f2Eigen: matrix_size != 9");
        }
        Eigen::Matrix<float, 3, 3> mat;
        for(int i = 0; i < 9; i++)
        {
            mat(i / 3, i % 3) = msg.matrix(i);
        }
        eigen = Eigen::Transform<float, 2, Eigen::Affine>(mat);
    }
    void MsgTransform3f2Eigen(const HsbaProto::msg_transform3& msg, Eigen::Isometry3f& eigen)
    {
        if(msg.matrix_size() != 16)
        {
            throw InvalidArgumentError("MsgTransform3f2Eigen: matrix_size != 16");
        }
        Eigen::Matrix<float, 4, 4> mat;
        for(int i = 0; i < 16; i++)
        {
            mat(i / 4, i % 4) = msg.matrix(i);
        }
        eigen = Eigen::Isometry3f(mat);
    }
    void MsgTransform2f2Eigen(const HsbaProto::msg_transform2& msg, Eigen::Isometry2f& eigen)
    {
        if(msg.matrix_size() != 9)
        {
            throw InvalidArgumentError("MsgTransform2f2Eigen: matrix_size != 9");
        }
        Eigen::Matrix<float, 3, 3> mat;
        for(int i = 0; i < 9; i++)
        {
            mat(i / 3, i % 3) = msg.matrix(i);
        }
        eigen = Eigen::Isometry2f(mat);
    }
    void MsgTransform3f2Eigen(const HsbaProto::msg_transform3& msg, Eigen::Matrix4f& eigen)
    {
        if(msg.matrix_size() != 16)
        {
            throw InvalidArgumentError("MsgTransform3f2Eigen: matrix_size != 16");
        }
        for(int i = 0; i < 16; i++)
        {
            eigen(i / 4, i % 4) = msg.matrix(i);
        }
    }
    void MsgTransform2f2Eigen(const HsbaProto::msg_transform2& msg, Eigen::Matrix3f& eigen)
    {
        if(msg.matrix_size() != 9)
        {
            throw InvalidArgumentError("MsgTransform2f2Eigen: matrix_size != 9");
        }
        for(int i = 0; i < 9; i++)
        {
            eigen(i / 3, i % 3) = msg.matrix(i);
        }
    }

    void MsgPath2Eigen(const HsbaProto::msg_path3& msg, std::vector<Eigen::Vector3f>& eigen)
    {
        eigen.resize(msg.point_size());
        for(int i = 0; i < msg.point_size(); i++)
        {
            MsgPoint3f2Eigen(msg.point(i), eigen[i]);
        }
    }
    void MsgPath2Eigen(const HsbaProto::msg_path2& msg, std::vector<Eigen::Vector2f>& eigen)
    {
        eigen.resize(msg.point_size());
        for(int i = 0; i < msg.point_size(); i++)
        {
            MsgPoint2f2Eigen(msg.point(i), eigen[i]);
        }
    }
}// namespace HsBa::Slicer