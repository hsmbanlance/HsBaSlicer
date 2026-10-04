/** @file Msg2Eigen.hpp
 * @brief Converters from HsbaProto protobuf messages to Eigen geometric types.
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_MSG2EIGEN_HPP
#define HSBA_SLICER_MSG2EIGEN_HPP

#include <vector>

#include <Eigen/Core>
#include <Eigen/Geometry>

#include "path.pb.h"
#include "point.pb.h"
#include "transform.pb.h"
#include "vector.pb.h"

namespace HsBa::Slicer
{
/// @brief Convert a msg_vector3 to a 3D vector.
void MsgVector3f2Eigen(const HsbaProto::msg_vector3& msg, Eigen::Vector3f& eigen);
/// @brief Convert a msg_point3 to a 3D vector.
void MsgPoint3f2Eigen(const HsbaProto::msg_point3& msg, Eigen::Vector3f& eigen);
/// @brief Convert a msg_vector2 to a 2D vector.
void MsgVector2f2Eigen(const HsbaProto::msg_vector2& msg, Eigen::Vector2f& eigen);
/// @brief Convert a msg_point2 to a 2D vector.
void MsgPoint2f2Eigen(const HsbaProto::msg_point2& msg, Eigen::Vector2f& eigen);

/// @brief Convert a msg_transform3 to a 3D affine transform.
void MsgTransform3f2Eigen(const HsbaProto::msg_transform3& msg, Eigen::Transform<float, 3, Eigen::Affine>& eigen);
/// @brief Convert a msg_transform2 to a 2D affine transform.
void MsgTransform2f2Eigen(const HsbaProto::msg_transform2& msg, Eigen::Transform<float, 2, Eigen::Affine>& eigen);
/// @brief Convert a msg_transform3 to a 3D isometry.
void MsgTransform3f2Eigen(const HsbaProto::msg_transform3& msg, Eigen::Isometry3f& eigen);
/// @brief Convert a msg_transform2 to a 2D isometry.
void MsgTransform2f2Eigen(const HsbaProto::msg_transform2& msg, Eigen::Isometry2f& eigen);
/// @brief Convert a msg_transform3 to a 4x4 matrix.
void MsgTransform3f2Eigen(const HsbaProto::msg_transform3& msg, Eigen::Matrix4f& eigen);
/// @brief Convert a msg_transform2 to a 3x3 matrix.
void MsgTransform2f2Eigen(const HsbaProto::msg_transform2& msg, Eigen::Matrix3f& eigen);

/// @brief Convert a msg_path3 to a 3D point path.
void MsgPath2Eigen(const HsbaProto::msg_path3& msg, std::vector<Eigen::Vector3f>& eigen);
/// @brief Convert a msg_path2 to a 2D point path.
void MsgPath2Eigen(const HsbaProto::msg_path2& msg, std::vector<Eigen::Vector2f>& eigen);
}  // namespace HsBa::Slicer

#endif  // HSBA_SLICER_MSG2EIGEN_HPP