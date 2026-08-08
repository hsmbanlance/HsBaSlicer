#include "OpenVdbModel.hpp"
#include "OpenVdbModel_internal.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <utility>

#include <openvdb/io/File.h>

#include "base/error.hpp"
#include "base/ModelFormat.hpp"

namespace HsBa::Slicer
{
namespace
{
static const bool kOpenVdbInitialized = []() {
    openvdb::initialize();
    return true;
}();

bool IsVdbFile(std::string_view fileName)
{
    const std::string lower = [&]() {
        std::string result{fileName};
        std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return result;
    }();

    return lower.ends_with(".vdb");
}

bool IsXYZFormat(std::string_view fileName)
{
    const std::string lower = [&]() {
        std::string result{fileName};
        std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return result;
    }();

    return lower.ends_with(".xyz") || lower.ends_with(".txt");
}

std::vector<Eigen::Vector3f> ReadPointsFromFile(const std::string& fileName)
{
    std::ifstream input(fileName);
    if (!input.is_open())
    {
        throw IOError("Failed to open point cloud file: " + fileName);
    }

    std::vector<Eigen::Vector3f> points;
    std::string line;
    while (std::getline(input, line))
    {
        if (line.empty())
        {
            continue;
        }

        std::istringstream stream(line);
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        if (!(stream >> x >> y >> z))
        {
            continue;
        }

        points.emplace_back(x, y, z);
    }

    if (points.empty())
    {
        throw IOError("No points were found in file: " + fileName);
    }

    return points;
}

bool WritePointsToFile(const std::string& fileName, const std::vector<Eigen::Vector3f>& points)
{
    std::ofstream output(fileName, std::ios::trunc);
    if (!output.is_open())
    {
        return false;
    }

    for (const auto& point : points)
    {
        output << point.x() << ' ' << point.y() << ' ' << point.z() << '\n';
    }
    return true;
}

}  // namespace

using vdb_internal::ToCoord;
using vdb_internal::ToEigen;
using vdb_internal::NormalizePath;

OpenVdbModel::OpenVdbModel() : grid_{openvdb::Vec3fGrid::create(openvdb::Vec3f(0.0f))}
{
    grid_->setName("point_cloud");
    grid_->setTransform(openvdb::math::Transform::createLinearTransform(1.0));
}

OpenVdbModel::OpenVdbModel(const std::string& file_path) : OpenVdbModel()
{
    file_name_ = file_path;
}

OpenVdbModel::OpenVdbModel(const std::pair<Eigen::MatrixXf, Eigen::MatrixXi>& mesh) : OpenVdbModel()
{
    SetFromVertices(mesh.first);
}

OpenVdbModel::OpenVdbModel(std::pair<Eigen::MatrixXf, Eigen::MatrixXi>&& mesh) : OpenVdbModel()
{
    SetFromVertices(mesh.first);
}

OpenVdbModel& OpenVdbModel::operator=(const std::pair<Eigen::MatrixXf, Eigen::MatrixXi>& mesh)
{
    SetFromVertices(mesh.first);
    return *this;
}

OpenVdbModel::operator std::pair<Eigen::MatrixXf, Eigen::MatrixXi>() const
{
    return ToVerticesFaces();
}

bool OpenVdbModel::Load(std::string_view fileName)
{
    file_name_ = std::string{fileName};
    const std::string normalizedPath = NormalizePath(fileName);

    try
    {
        if (IsVdbFile(fileName))
        {
            openvdb::io::File file(normalizedPath);
            file.open();
            openvdb::GridPtrVecPtr grids = file.getGrids();
            file.close();

            if (!grids || grids->empty())
            {
                throw IOError("No OpenVDB grids were found in file: " + normalizedPath);
            }

            if (auto grid = openvdb::gridPtrCast<openvdb::Vec3fGrid>((*grids)[0]))
            {
                grid_ = grid;
            }
            else
            {
                throw IOError("The OpenVDB grid in file does not contain Vec3f values: " + normalizedPath);
            }
        }
        else
        {
            const auto points = ReadPointsFromFile(normalizedPath);
            grid_ = openvdb::Vec3fGrid::create(openvdb::Vec3f(0.0f));
            grid_->setName("point_cloud");
            grid_->setTransform(openvdb::math::Transform::createLinearTransform(1.0));
            for (const auto& point : points)
            {
                AddPoint(point);
            }
        }

        return true;
    }
    catch (const std::exception&)
    {
        Clear();
        return false;
    }
}

bool OpenVdbModel::Save(std::string_view fileName, const ModelFormat format) const
{
    if (!IsPointCloudFormat(format) && !IsXYZFormat(fileName) && !IsVdbFile(fileName))
    {
        throw NotSupportedError("Unsupported point cloud format.");
    }

    const std::string normalizedPath = NormalizePath(fileName);
    if (IsVdbFile(fileName))
    {
        openvdb::io::File file(normalizedPath);
        openvdb::GridPtrVec grids;
        grids.push_back(grid_);
        file.write(grids);
        file.close();
        return true;
    }

    const auto points = Points();
    return WritePointsToFile(normalizedPath, points);
}

void OpenVdbModel::Translate(const Eigen::Vector3f& translation)
{
    for (auto iter = grid_->beginValueOn(); iter; ++iter)
    {
        openvdb::Vec3f value = *iter;
        value += openvdb::Vec3f(translation.x(), translation.y(), translation.z());
        iter.setValue(value);
    }
}

void OpenVdbModel::Rotate(const Eigen::Quaternionf& rotation)
{
    const Eigen::Matrix3f rotationMatrix = rotation.toRotationMatrix();
    for (auto iter = grid_->beginValueOn(); iter; ++iter)
    {
        openvdb::Vec3f value = *iter;
        Eigen::Vector3f point = ToEigen(value);
        point = rotationMatrix * point;
        iter.setValue(openvdb::Vec3f(point.x(), point.y(), point.z()));
    }
}

void OpenVdbModel::Scale(const float scale)
{
    for (auto iter = grid_->beginValueOn(); iter; ++iter)
    {
        openvdb::Vec3f value = *iter;
        value *= scale;
        iter.setValue(value);
    }
}

void OpenVdbModel::Scale(const Eigen::Vector3f& scale)
{
    for (auto iter = grid_->beginValueOn(); iter; ++iter)
    {
        openvdb::Vec3f value = *iter;
        value.x() *= scale.x();
        value.y() *= scale.y();
        value.z() *= scale.z();
        iter.setValue(value);
    }
}

void OpenVdbModel::Transform(const Eigen::Isometry3f& transform)
{
    for (auto iter = grid_->beginValueOn(); iter; ++iter)
    {
        Eigen::Vector3f point = ToEigen(*iter);
        point = transform * point;
        iter.setValue(openvdb::Vec3f(point.x(), point.y(), point.z()));
    }
}

void OpenVdbModel::Transform(const Eigen::Matrix4f& transform)
{
    for (auto iter = grid_->beginValueOn(); iter; ++iter)
    {
        Eigen::Vector4f homogeneous(ToEigen(*iter).x(), ToEigen(*iter).y(), ToEigen(*iter).z(), 1.0f);
        const Eigen::Vector4f transformed = transform * homogeneous;
        const Eigen::Vector3f point = transformed.head<3>();
        iter.setValue(openvdb::Vec3f(point.x(), point.y(), point.z()));
    }
}

void OpenVdbModel::Transform(const Eigen::Transform<float, 3, Eigen::Affine>& transform)
{
    for (auto iter = grid_->beginValueOn(); iter; ++iter)
    {
        Eigen::Vector3f point = ToEigen(*iter);
        point = transform * point;
        iter.setValue(openvdb::Vec3f(point.x(), point.y(), point.z()));
    }
}

void OpenVdbModel::BoundingBox(Eigen::Vector3f& min, Eigen::Vector3f& max) const
{
    if (PointCount() == 0)
    {
        min = Eigen::Vector3f::Zero();
        max = Eigen::Vector3f::Zero();
        return;
    }

    min = ToEigen(*grid_->cbeginValueOn());
    max = ToEigen(*grid_->cbeginValueOn());

    for (auto iter = grid_->cbeginValueOn(); iter; ++iter)
    {
        const Eigen::Vector3f point = ToEigen(*iter);
        min.x() = std::min(min.x(), point.x());
        min.y() = std::min(min.y(), point.y());
        min.z() = std::min(min.z(), point.z());
        max.x() = std::max(max.x(), point.x());
        max.y() = std::max(max.y(), point.y());
        max.z() = std::max(max.z(), point.z());
    }
}

float OpenVdbModel::Volume() const
{
    return 0.0f;
}

std::pair<Eigen::MatrixXf, Eigen::MatrixXi> OpenVdbModel::TriangleMesh() const
{
    return GenerateMesh();
}

void OpenVdbModel::AddPoint(const Eigen::Vector3f& point)
{
    if (!grid_)
    {
        grid_ = openvdb::Vec3fGrid::create(openvdb::Vec3f(0.0f));
        grid_->setName("point_cloud");
        grid_->setTransform(openvdb::math::Transform::createLinearTransform(1.0));
    }

    grid_->tree().setValue(ToCoord(point), openvdb::Vec3f(point.x(), point.y(), point.z()));
}

void OpenVdbModel::AddPoints(const std::vector<Eigen::Vector3f>& points)
{
    for (const auto& point : points)
    {
        AddPoint(point);
    }
}

std::vector<Eigen::Vector3f> OpenVdbModel::Points() const
{
    std::vector<Eigen::Vector3f> points;
    points.reserve(PointCount());
    for (auto iter = grid_->cbeginValueOn(); iter; ++iter)
    {
        points.push_back(ToEigen(*iter));
    }
    return points;
}

std::size_t OpenVdbModel::PointCount() const
{
    if (!grid_)
    {
        return 0;
    }

    return static_cast<std::size_t>(grid_->activeVoxelCount());
}

bool OpenVdbModel::IsEmpty() const
{
    return PointCount() == 0;
}

void OpenVdbModel::SetFromVertices(const Eigen::MatrixXf& vertices)
{
    Clear();
    const auto rows = vertices.rows();
    for (Eigen::Index i = 0; i < rows; ++i)
    {
        AddPoint(Eigen::Vector3f{vertices(i, 0), vertices(i, 1), vertices(i, 2)});
    }
}

Eigen::MatrixXf OpenVdbModel::ToVertices() const
{
    const auto points = Points();
    Eigen::MatrixXf vertices(static_cast<Eigen::Index>(points.size()), 3);
    for (std::size_t i = 0; i < points.size(); ++i)
    {
        vertices.row(static_cast<Eigen::Index>(i)) = points[i].transpose();
    }
    return vertices;
}

std::pair<Eigen::MatrixXf, Eigen::MatrixXi> OpenVdbModel::ToVerticesFaces() const
{
    return GenerateMesh();
}

void OpenVdbModel::Clear()
{
    if (!grid_)
    {
        grid_ = openvdb::Vec3fGrid::create(openvdb::Vec3f(0.0f));
        grid_->setName("point_cloud");
        grid_->setTransform(openvdb::math::Transform::createLinearTransform(1.0));
        return;
    }

    grid_->clear();
    grid_->setTransform(openvdb::math::Transform::createLinearTransform(1.0));
}

std::vector<Eigen::Vector3f> OpenVdbModel::NearestNeighbor(const Eigen::Vector3f& query) const
{
    std::vector<Eigen::Vector3f> result;
    if (PointCount() == 0)
    {
        return result;
    }

    float bestDistance = std::numeric_limits<float>::infinity();
    Eigen::Vector3f bestPoint;
    for (auto iter = grid_->cbeginValueOn(); iter; ++iter)
    {
        const Eigen::Vector3f point = ToEigen(*iter);
        const float distance = (point - query).squaredNorm();
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestPoint = point;
        }
    }

    if (PointCount() > 0)
    {
        result.push_back(bestPoint);
    }
    return result;
}

std::vector<Eigen::Vector3f> OpenVdbModel::KNN(const Eigen::Vector3f& query, std::size_t k) const
{
    if (k == 0)
    {
        return {};
    }

    std::vector<std::pair<float, Eigen::Vector3f>> ranked;
    ranked.reserve(PointCount());
    for (auto iter = grid_->cbeginValueOn(); iter; ++iter)
    {
        const Eigen::Vector3f point = ToEigen(*iter);
        ranked.emplace_back((point - query).squaredNorm(), point);
    }

    std::sort(ranked.begin(), ranked.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.first < rhs.first;
    });

    std::vector<Eigen::Vector3f> result;
    result.reserve(std::min<std::size_t>(k, ranked.size()));
    for (std::size_t i = 0; i < std::min<std::size_t>(k, ranked.size()); ++i)
    {
        result.push_back(ranked[i].second);
    }
    return result;
}

std::vector<Eigen::Vector3f> OpenVdbModel::Filter(const std::function<bool(const Eigen::Vector3f&)>& predicate) const
{
    if (!predicate)
    {
        return {};
    }

    std::vector<Eigen::Vector3f> result;
    for (auto iter = grid_->cbeginValueOn(); iter; ++iter)
    {
        const Eigen::Vector3f point = ToEigen(*iter);
        if (predicate(point))
        {
            result.push_back(point);
        }
    }
    return result;
}

void OpenVdbModel::Voxelize(float voxelSize)
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

    auto newGrid = openvdb::Vec3fGrid::create(openvdb::Vec3f(0.0f));
    newGrid->setName("point_cloud");
    newGrid->setTransform(openvdb::math::Transform::createLinearTransform(static_cast<double>(voxelSize)));

    for (const auto& point : points)
    {
        const openvdb::Coord coord = newGrid->transform().worldToIndexCellCentered(vdb_internal::ToVec3d(point));
        newGrid->tree().setValue(coord, openvdb::Vec3f(point.x(), point.y(), point.z()));
    }

    grid_ = std::move(newGrid);
}

std::vector<Eigen::Vector3f> OpenVdbModel::VoxelCenters(float voxelSize) const
{
    if (voxelSize <= 0.0f)
    {
        throw InvalidArgumentError("voxel size must be positive");
    }

    std::vector<Eigen::Vector3f> centers;
    for (auto iter = grid_->cbeginValueOn(); iter; ++iter)
    {
        const auto coord = iter.getCoord();
        const openvdb::Vec3d center = grid_->transform().indexToWorld(openvdb::Vec3d(
            static_cast<double>(coord.x()) + 0.5,
            static_cast<double>(coord.y()) + 0.5,
            static_cast<double>(coord.z()) + 0.5));
        centers.emplace_back(static_cast<float>(center.x()), static_cast<float>(center.y()), static_cast<float>(center.z()));
    }
    return centers;
}

Eigen::Vector3f OpenVdbModel::Centroid() const
{
    if (IsEmpty())
    {
        return Eigen::Vector3f::Zero();
    }

    Eigen::Vector3f sum = Eigen::Vector3f::Zero();
    std::size_t count = 0;
    for (auto iter = grid_->cbeginValueOn(); iter; ++iter)
    {
        sum += ToEigen(*iter);
        ++count;
    }
    return sum / static_cast<float>(count);
}

void OpenVdbModel::Merge(const OpenVdbModel& other)
{
    const auto otherPoints = other.Points();
    AddPoints(otherPoints);
}

}  // namespace HsBa::Slicer
