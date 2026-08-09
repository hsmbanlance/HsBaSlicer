/// @file OpenVdbModel_analysis.cpp
/// @brief Point cloud analysis operations (spatial queries, downsampling, normals).
/// Separated to avoid exceeding MSVC object file section limits (/bigobj).

#include "OpenVdbModel.hpp"
#include "OpenVdbModel_internal.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <set>
#include <string>
#include <vector>

#include <Eigen/Eigenvalues>

#include <openvdb/tools/PointIndexGrid.h>

#include "base/error.hpp"

namespace HsBa::Slicer
{

using vdb_internal::ToEigen;
using vdb_internal::ToVec3d;
using vdb_internal::PointArrayAdapter;

std::vector<Eigen::Vector3f> OpenVdbModel::RadiusSearch(const Eigen::Vector3f& center, float radius) const
{
    if (radius <= 0.0f)
    {
        throw InvalidArgumentError("radius must be positive");
    }

    const auto points = Points();
    if (points.empty())
    {
        return {};
    }

    PointArrayAdapter adapter(points);
    const double voxelSize = std::max(1e-3, static_cast<double>(radius) * 0.25);
    auto indexGrid = openvdb::tools::createPointIndexGrid<openvdb::tools::PointIndexGrid>(adapter, voxelSize);

    std::vector<Eigen::Vector3f> result;
    openvdb::tree::ValueAccessor<const openvdb::tools::PointIndexTree> acc(indexGrid->tree());
    openvdb::tools::PointIndexIterator<> iter;
    iter.worldSpaceSearchAndUpdate(ToVec3d(center), static_cast<double>(radius), acc, adapter, indexGrid->transform(), true);

    while (iter.test())
    {
        const auto pointIndex = *iter;
        if (pointIndex < points.size())
        {
            result.push_back(points[static_cast<std::size_t>(pointIndex)]);
        }
        if (!iter.next())
        {
            break;
        }
    }

    return result;
}

void OpenVdbModel::Downsample(float voxelSize)
{
    if (voxelSize <= 0.0f)
    {
        throw InvalidArgumentError("voxel size must be positive");
    }

    const auto points = Points();
    if (points.empty())
    {
        return;
    }

    std::vector<Eigen::Vector3f> uniquePoints;
    std::set<std::string> visited;
    PointArrayAdapter adapter(points);
    auto indexGrid = openvdb::tools::createPointIndexGrid<openvdb::tools::PointIndexGrid>(adapter, static_cast<double>(voxelSize));
    for (const auto& point : points)
    {
        const openvdb::Coord coord = indexGrid->transform().worldToIndexCellCentered(ToVec3d(point));
        const auto key = std::to_string(coord.x()) + ":" + std::to_string(coord.y()) + ":" + std::to_string(coord.z());
        if (visited.insert(key).second)
        {
            uniquePoints.push_back(point);
        }
    }

    Clear();
    AddPoints(uniquePoints);
}

void OpenVdbModel::RemoveStatisticalOutliers(std::size_t k, float multiplier)
{
    if (k == 0)
    {
        throw InvalidArgumentError("k must be positive");
    }

    const auto points = Points();
    const auto n = points.size();
    if (n == 0)
    {
        return;
    }

    // Compute mean distance to k nearest neighbors for each point
    std::vector<float> meanDistances(n, 0.0f);
    for (std::size_t i = 0; i < n; ++i)
    {
        auto neighbors = KNN(points[i], k + 1);  // +1 because the point itself is included
        float distSum = 0.0f;
        std::size_t count = 0;
        for (const auto& neighbor : neighbors)
        {
            const float dist = (neighbor - points[i]).norm();
            if (dist > 1e-8f)  // skip self
            {
                distSum += dist;
                ++count;
                if (count >= k)
                {
                    break;
                }
            }
        }
        meanDistances[i] = (count > 0) ? (distSum / static_cast<float>(count)) : 0.0f;
    }

    // Compute global mean and standard deviation
    const float globalMean = std::accumulate(meanDistances.begin(), meanDistances.end(), 0.0f) / static_cast<float>(n);
    float variance = 0.0f;
    for (const float d : meanDistances)
    {
        variance += (d - globalMean) * (d - globalMean);
    }
    variance /= static_cast<float>(n);
    const float stdDev = std::sqrt(variance);

    const float threshold = globalMean + multiplier * stdDev;

    // Keep only inliers
    std::vector<Eigen::Vector3f> inliers;
    inliers.reserve(n);
    for (std::size_t i = 0; i < n; ++i)
    {
        if (meanDistances[i] <= threshold)
        {
            inliers.push_back(points[i]);
        }
    }

    Clear();
    AddPoints(inliers);
}

Eigen::MatrixXf OpenVdbModel::ComputeNormals(std::size_t k) const
{
    const auto points = Points();
    const auto n = points.size();
    Eigen::MatrixXf normals(static_cast<Eigen::Index>(n), 3);

    if (n == 0)
    {
        return normals;
    }

    for (std::size_t i = 0; i < n; ++i)
    {
        auto neighbors = KNN(points[i], k + 1);

        // Compute centroid of neighbors
        Eigen::Vector3f centroid = Eigen::Vector3f::Zero();
        std::size_t count = 0;
        for (const auto& neighbor : neighbors)
        {
            centroid += neighbor;
            ++count;
        }
        if (count > 0)
        {
            centroid /= static_cast<float>(count);
        }

        // Build covariance matrix
        Eigen::Matrix3f covariance = Eigen::Matrix3f::Zero();
        for (const auto& neighbor : neighbors)
        {
            const Eigen::Vector3f d = neighbor - centroid;
            covariance += d * d.transpose();
        }
        covariance /= static_cast<float>(std::max<std::size_t>(count, 1));

        // Eigenvector corresponding to smallest eigenvalue is the normal
        Eigen::SelfAdjointEigenSolver<Eigen::Matrix3f> solver(covariance);
        if (solver.info() == Eigen::Success)
        {
            normals.row(static_cast<Eigen::Index>(i)) = solver.eigenvectors().col(0).normalized().transpose();
        }
        else
        {
            normals.row(static_cast<Eigen::Index>(i)) = Eigen::Vector3f::UnitZ().transpose();
        }
    }

    return normals;
}

}  // namespace HsBa::Slicer
