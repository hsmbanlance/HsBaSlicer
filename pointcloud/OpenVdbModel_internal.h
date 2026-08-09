#pragma once
#ifndef HSBA_SLICER_OPENVDB_MODEL_INTERNAL_H
#define HSBA_SLICER_OPENVDB_MODEL_INTERNAL_H

/// @file OpenVdbModel_internal.h
/// @brief Shared helpers for OpenVdbModel implementation files (not part of public API).

#include <cmath>
#include <string>
#include <string_view>
#include <vector>

#include <Eigen/Core>
#include <openvdb/openvdb.h>

#include "base/encoding_convert.hpp"

namespace HsBa::Slicer
{
namespace vdb_internal
{

inline std::string NormalizePath(std::string_view fileName)
{
    if (fileName.empty())
    {
        return {};
    }
    return utf8_to_local(std::string{fileName});
}

inline openvdb::Coord ToCoord(const Eigen::Vector3f& point)
{
    return openvdb::Coord{static_cast<int>(std::llround(point.x())),
                          static_cast<int>(std::llround(point.y())),
                          static_cast<int>(std::llround(point.z()))};
}

inline Eigen::Vector3f ToEigen(const openvdb::Vec3f& value)
{
    return Eigen::Vector3f{value.x(), value.y(), value.z()};
}

inline openvdb::Vec3d ToVec3d(const Eigen::Vector3f& point)
{
    return openvdb::Vec3d{static_cast<double>(point.x()), static_cast<double>(point.y()), static_cast<double>(point.z())};
}

/// @brief Particle adapter for OpenVDB PointIndexGrid.
struct PointArrayAdapter
{
    explicit PointArrayAdapter(const std::vector<Eigen::Vector3f>& pts) : points(pts) {}

    using PosType = openvdb::Vec3d;
    std::size_t size() const { return points.size(); }
    void getPos(std::size_t n, PosType& xyz) const
    {
        const auto& p = points[n];
        xyz = PosType{static_cast<double>(p.x()), static_cast<double>(p.y()), static_cast<double>(p.z())};
    }

    const std::vector<Eigen::Vector3f>& points;
};

/// @brief Particle adapter for OpenVDB ParticlesToLevelSet.
struct LevelSetParticleAdapter
{
    using PosType = openvdb::Vec3R;

    explicit LevelSetParticleAdapter(const std::vector<Eigen::Vector3f>& pts) : points_(pts) {}

    std::size_t size() const { return points_.size(); }

    void getPos(std::size_t n, openvdb::Vec3R& xyz) const
    {
        const auto& p = points_[n];
        xyz = openvdb::Vec3R{static_cast<double>(p.x()), static_cast<double>(p.y()), static_cast<double>(p.z())};
    }

    void getPosRad(std::size_t n, openvdb::Vec3R& xyz, openvdb::Real& radius) const
    {
        getPos(n, xyz);
        radius = radius_;
    }

    void getPosRadVel(std::size_t n, openvdb::Vec3R& xyz, openvdb::Real& radius, openvdb::Vec3R& vel) const
    {
        getPosRad(n, xyz, radius);
        vel = openvdb::Vec3R{0.0, 0.0, 0.0};
    }

    const std::vector<Eigen::Vector3f>& points_;
    openvdb::Real radius_ = 1.0;
};

}  // namespace vdb_internal
}  // namespace HsBa::Slicer

#endif  // HSBA_SLICER_OPENVDB_MODEL_INTERNAL_H
