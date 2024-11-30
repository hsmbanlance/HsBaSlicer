#pragma once
#ifndef HSBA_SLICER_EIGEN2MSG_HPP
#define HSBA_SLICER_EIGEN2MSG_HPP

#include <vector>

#include <Eigen/Core>
#include <Eigen/Geometry>

#include "vector.pb.h"
#include "point.pb.h"
#include "transform.pb.h"
#include "path.pb.h"

namespace HsBa::Slicer
{
    void EigenVector3f2Msg(const Eigen::Vector3f& eigen, HsbaProto::msg_vector3* msg);
    void EigenVector3f2Msg(const Eigen::Vector3f& eigen, HsbaProto::msg_point3* msg);
    void EigenVector2f2Msg(const Eigen::Vector2f& eigen, HsbaProto::msg_vector2* msg);
    void EigenVector2f2Msg(const Eigen::Vector2f& eigen, HsbaProto::msg_point2* msg);
    void EigenQuaternionf2Msg(const Eigen::Quaternionf& eigen, HsbaProto::msg_quaternion* msg);

    void EigenTransform3f2Msg(const Eigen::Transform<float, 3, Eigen::Affine>& eigen, HsbaProto::msg_transform3* msg);
    void EigenTransform2f2Msg(const Eigen::Transform<float, 2, Eigen::Affine>& eigen, HsbaProto::msg_transform2* msg);
    void EigenIsometric3f2Msg(const Eigen::Isometry3f& eigen, HsbaProto::msg_transform3* msg);
    void EigenIsometric2f2Msg(const Eigen::Isometry2f& eigen, HsbaProto::msg_transform2* msg);
    void EigenMatrix3f2Msg(const Eigen::Matrix4f& eigen, HsbaProto::msg_transform3* msg);
    void EigenMatrix2f2Msg(const Eigen::Matrix3f& eigen, HsbaProto::msg_transform2* msg);

    void EigenPath2Msg(const std::vector<Eigen::Vector3f>& eigen, HsbaProto::msg_path3* msg);
    void EigenPath2Msg(const std::vector<Eigen::Vector2f>& eigen, HsbaProto::msg_path2* msg);
}// namespace HsBa::Slicer

#endif // HSBA_SLICER_EIGEN2MSG_HPP