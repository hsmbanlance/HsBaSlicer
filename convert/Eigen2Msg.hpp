/** @file Eigen2Msg.hpp
 * @brief Converters from Eigen geometric types to HsbaProto protobuf messages.
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_EIGEN2MSG_HPP
#define HSBA_SLICER_EIGEN2MSG_HPP

#include <vector>

#include <Eigen/Core>
#include <Eigen/Geometry>

#include "path.pb.h"
#include "point.pb.h"
#include "transform.pb.h"
#include "vector.pb.h"

namespace HsBa::Slicer
{
/// @brief Convert a 3D vector to a msg_vector3.
void EigenVector3f2Msg(const Eigen::Vector3f& eigen, HsbaProto::msg_vector3* msg);
/// @brief Convert a 3D vector to a msg_point3.
void EigenVector3f2Msg(const Eigen::Vector3f& eigen, HsbaProto::msg_point3* msg);
/// @brief Convert a 2D vector to a msg_vector2.
void EigenVector2f2Msg(const Eigen::Vector2f& eigen, HsbaProto::msg_vector2* msg);
/// @brief Convert a 2D vector to a msg_point2.
void EigenVector2f2Msg(const Eigen::Vector2f& eigen, HsbaProto::msg_point2* msg);
/// @brief Convert a quaternion to a msg_quaternion.
void EigenQuaternionf2Msg(const Eigen::Quaternionf& eigen, HsbaProto::msg_quaternion* msg);

/// @brief Convert a 3D affine transform to a msg_transform3.
void EigenTransform3f2Msg(const Eigen::Transform<float, 3, Eigen::Affine>& eigen, HsbaProto::msg_transform3* msg);
/// @brief Convert a 2D affine transform to a msg_transform2.
void EigenTransform2f2Msg(const Eigen::Transform<float, 2, Eigen::Affine>& eigen, HsbaProto::msg_transform2* msg);
/// @brief Convert a 3D isometry to a msg_transform3.
void EigenIsometric3f2Msg(const Eigen::Isometry3f& eigen, HsbaProto::msg_transform3* msg);
/// @brief Convert a 2D isometry to a msg_transform2.
void EigenIsometric2f2Msg(const Eigen::Isometry2f& eigen, HsbaProto::msg_transform2* msg);
/// @brief Convert a 4x4 matrix to a msg_transform3.
void EigenMatrix3f2Msg(const Eigen::Matrix4f& eigen, HsbaProto::msg_transform3* msg);
/// @brief Convert a 3x3 matrix to a msg_transform2.
void EigenMatrix2f2Msg(const Eigen::Matrix3f& eigen, HsbaProto::msg_transform2* msg);

/// @brief Convert a 3D point path to a msg_path3.
void EigenPath2Msg(const std::vector<Eigen::Vector3f>& eigen, HsbaProto::msg_path3* msg);
/// @brief Convert a 2D point path to a msg_path2.
void EigenPath2Msg(const std::vector<Eigen::Vector2f>& eigen, HsbaProto::msg_path2* msg);
}  // namespace HsBa::Slicer

#endif  // HSBA_SLICER_EIGEN2MSG_HPP