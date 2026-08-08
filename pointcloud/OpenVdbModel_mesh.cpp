/// @file OpenVdbModel_mesh.cpp
/// @brief Mesh generation from point cloud via OpenVDB level set reconstruction.
/// Separated to avoid exceeding MSVC object file section limits (/bigobj).

#include "OpenVdbModel.hpp"
#include "OpenVdbModel_internal.h"

#include <cmath>
#include <utility>
#include <vector>

#include <openvdb/tools/ParticlesToLevelSet.h>
#include <openvdb/tools/VolumeToMesh.h>
#include <openvdb/tools/LevelSetFilter.h>

#include "base/error.hpp"

namespace HsBa::Slicer
{

std::pair<Eigen::MatrixXf, Eigen::MatrixXi> OpenVdbModel::GenerateMesh(float voxelSize, float particleRadius) const
{
    const auto points = Points();
    if (points.empty())
    {
        return {Eigen::MatrixXf(0, 3), Eigen::MatrixXi(0, 3)};
    }

    // Auto-estimate voxel size from bounding box
    Eigen::Vector3f bbMin, bbMax;
    BoundingBox(bbMin, bbMax);
    float maxExtent = (bbMax - bbMin).maxCoeff();
    if (maxExtent < 1e-6f)
    {
        maxExtent = 1.0f;  // Degenerate case: all points coincide
    }

    if (voxelSize <= 0.0f)
    {
        voxelSize = maxExtent / 100.0f;
    }
    if (particleRadius <= 0.0f)
    {
        // Estimate radius from point density: average spacing ~ extent / cbrt(count)
        const float avgSpacing = maxExtent / std::cbrt(static_cast<float>(points.size()));
        particleRadius = avgSpacing * 1.5f;
    }

    // Create particle adapter
    vdb_internal::LevelSetParticleAdapter adapter(points);
    adapter.radius_ = static_cast<openvdb::Real>(particleRadius);

    // Create SDF grid and rasterize particles as spheres
    auto sdfGrid = openvdb::FloatGrid::create(/*background=*/particleRadius * 3.0f);
    sdfGrid->setTransform(openvdb::math::Transform::createLinearTransform(static_cast<double>(voxelSize)));
    sdfGrid->setGridClass(openvdb::GRID_LEVEL_SET);

    openvdb::tools::particlesToSdf(adapter, *sdfGrid, static_cast<openvdb::Real>(particleRadius));

    // Smooth the level set to reduce noise
    openvdb::tools::LevelSetFilter<openvdb::FloatGrid> filter(*sdfGrid);
    filter.mean(2);
    filter.gaussian(1);

    // Extract mesh via marching cubes
    std::vector<openvdb::Vec3s> meshPoints;
    std::vector<openvdb::Vec3I> triangles;
    std::vector<openvdb::Vec4I> quads;
    openvdb::tools::volumeToMesh(*sdfGrid, meshPoints, triangles, quads, 0.0);

    // Triangulate quads
    const std::size_t totalFaces = triangles.size() + quads.size() * 2;
    if (totalFaces == 0)
    {
        // Fallback: return raw points without faces
        Eigen::MatrixXf vertices(static_cast<Eigen::Index>(points.size()), 3);
        for (std::size_t i = 0; i < points.size(); ++i)
        {
            vertices.row(static_cast<Eigen::Index>(i)) = points[i].transpose();
        }
        return {vertices, Eigen::MatrixXi(0, 3)};
    }

    // Convert vertices
    Eigen::MatrixXf vertices(static_cast<Eigen::Index>(meshPoints.size()), 3);
    for (std::size_t i = 0; i < meshPoints.size(); ++i)
    {
        vertices(static_cast<Eigen::Index>(i), 0) = meshPoints[i].x();
        vertices(static_cast<Eigen::Index>(i), 1) = meshPoints[i].y();
        vertices(static_cast<Eigen::Index>(i), 2) = meshPoints[i].z();
    }

    // Convert faces (triangles + triangulated quads)
    Eigen::MatrixXi faces(static_cast<Eigen::Index>(totalFaces), 3);
    Eigen::Index faceIdx = 0;
    for (const auto& tri : triangles)
    {
        faces(faceIdx, 0) = static_cast<int>(tri[0]);
        faces(faceIdx, 1) = static_cast<int>(tri[1]);
        faces(faceIdx, 2) = static_cast<int>(tri[2]);
        ++faceIdx;
    }
    for (const auto& quad : quads)
    {
        // Split quad into two triangles: (0,1,2) and (0,2,3)
        faces(faceIdx, 0) = static_cast<int>(quad[0]);
        faces(faceIdx, 1) = static_cast<int>(quad[1]);
        faces(faceIdx, 2) = static_cast<int>(quad[2]);
        ++faceIdx;
        faces(faceIdx, 0) = static_cast<int>(quad[0]);
        faces(faceIdx, 1) = static_cast<int>(quad[2]);
        faces(faceIdx, 2) = static_cast<int>(quad[3]);
        ++faceIdx;
    }

    return {vertices, faces};
}

}  // namespace HsBa::Slicer
