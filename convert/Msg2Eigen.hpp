#pragma once
#ifndef HSBA_SLICER_MSG2EIGEN_HPP
#define HSBA_SLICER_MSG2EIGEN_HPP

#include <vector>

#include <Eigen/Core>
#include <Eigen/Geometry>

#include "vector.pb.h"
#include "point.pb.h"
#include "transform.pb.h"
#include "path.pb.h"

namespace HsBa::Slicer
{
    void MsgVector3f2Eigen(const HsbaProto::msg_vector3& msg, Eigen::Vector3f& eigen);
    void MsgPoint3f2Eigen(const HsbaProto::msg_point3& msg, Eigen::Vector3f& eigen);
    void MsgVector2f2Eigen(const HsbaProto::msg_vector2& msg, Eigen::Vector2f& eigen);
    void MsgPoint2f2Eigen(const HsbaProto::msg_point2& msg, Eigen::Vector2f& eigen);

    void MsgTransform3f2Eigen(const HsbaProto::msg_transform3& msg, Eigen::Transform<float, 3, Eigen::Affine>& eigen);
    void MsgTransform2f2Eigen(const HsbaProto::msg_transform2& msg, Eigen::Transform<float, 2, Eigen::Affine>& eigen);
    void MsgTransform3f2Eigen(const HsbaProto::msg_transform3& msg, Eigen::Isometry3f& eigen);
    void MsgTransform2f2Eigen(const HsbaProto::msg_transform2& msg, Eigen::Isometry2f& eigen);
    void MsgTransform3f2Eigen(const HsbaProto::msg_transform3& msg, Eigen::Matrix4f& eigen);
    void MsgTransform2f2Eigen(const HsbaProto::msg_transform2& msg, Eigen::Matrix3f& eigen);

    void MsgPath2Eigen(const HsbaProto::msg_path3& msg, std::vector<Eigen::Vector3f>& eigen);
    void MsgPath2Eigen(const HsbaProto::msg_path2& msg, std::vector<Eigen::Vector2f>& eigen);
}// namespace HsBa::Slicer

#endif // HSBA_SLICER_MSG2EIGEN_HPP