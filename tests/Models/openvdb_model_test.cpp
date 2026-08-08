#define BOOST_TEST_MODULE OpenVdbModelTests
#include <boost/test/included/unit_test.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>

#include "pointcloud/OpenVdbModel.hpp"

using namespace HsBa::Slicer;

BOOST_AUTO_TEST_CASE(point_cloud_operations)
{
    OpenVdbModel model;
    model.AddPoint(Eigen::Vector3f{0.0f, 0.0f, 0.0f});
    model.AddPoint(Eigen::Vector3f{1.0f, 2.0f, 3.0f});
    model.AddPoint(Eigen::Vector3f{4.0f, 0.5f, 1.0f});

    BOOST_CHECK_EQUAL(model.PointCount(), 3u);

    auto points = model.Points();
    BOOST_CHECK_EQUAL(points.size(), 3u);
    BOOST_CHECK_CLOSE(points[1].x(), 1.0f, 1e-5);
    BOOST_CHECK_CLOSE(points[1].y(), 2.0f, 1e-5);
    BOOST_CHECK_CLOSE(points[1].z(), 3.0f, 1e-5);

    Eigen::Vector3f min, max;
    model.BoundingBox(min, max);
    BOOST_CHECK_CLOSE(min.x(), 0.0f, 1e-5);
    BOOST_CHECK_CLOSE(min.y(), 0.0f, 1e-5);
    BOOST_CHECK_CLOSE(min.z(), 0.0f, 1e-5);
    BOOST_CHECK_CLOSE(max.x(), 4.0f, 1e-5);
    BOOST_CHECK_CLOSE(max.y(), 2.0f, 1e-5);
    BOOST_CHECK_CLOSE(max.z(), 3.0f, 1e-5);

    model.Translate(Eigen::Vector3f{1.0f, -1.0f, 0.0f});
    auto translated = model.Points();
    BOOST_REQUIRE_EQUAL(translated.size(), 3u);
    BOOST_CHECK_CLOSE(translated[0].x(), 1.0f, 1e-5);
    BOOST_CHECK_CLOSE(translated[0].y(), -1.0f, 1e-5);
    BOOST_CHECK_CLOSE(translated[0].z(), 0.0f, 1e-5);
    BOOST_CHECK_CLOSE(translated[1].x(), 2.0f, 1e-5);
    BOOST_CHECK_CLOSE(translated[1].y(), 1.0f, 1e-5);
    BOOST_CHECK_CLOSE(translated[1].z(), 3.0f, 1e-5);
    BOOST_CHECK_CLOSE(translated[2].x(), 5.0f, 1e-5);
    BOOST_CHECK_CLOSE(translated[2].y(), -0.5f, 1e-5);
    BOOST_CHECK_CLOSE(translated[2].z(), 1.0f, 1e-5);

    model.Voxelize(1.0f);
    BOOST_CHECK_EQUAL(model.PointCount(), 3u);
    auto voxelCenters = model.VoxelCenters(1.0f);
    BOOST_CHECK_EQUAL(voxelCenters.size(), model.PointCount());

    // 平移后的点：(1,-1,0) 距离 1.0、(2,1,3) 距离 sqrt(14)≈3.74、(5,-0.5,1) 距离 sqrt(27.25)≈5.22
    auto neighbors = model.RadiusSearch(Eigen::Vector3f{0.0f, 0.0f, 0.0f}, 2.0f);
    BOOST_REQUIRE_EQUAL(neighbors.size(), 1u);
    BOOST_CHECK_CLOSE(neighbors.front().x(), 1.0f, 1e-5);
    BOOST_CHECK_CLOSE(neighbors.front().y(), -1.0f, 1e-5);
    BOOST_CHECK_CLOSE(neighbors.front().z(), 0.0f, 1e-5);
    auto wideNeighbors = model.RadiusSearch(Eigen::Vector3f{0.0f, 0.0f, 0.0f}, 4.0f);
    BOOST_CHECK_EQUAL(wideNeighbors.size(), 2u);

    // 坐标取整后不同、但落入同一体素的点，降采样后必须合并为一个
    // （10.0 与 10.6 取整为 10/11 不会被 AddPoint 合并，但在 voxelSize=2.0 下属于同一体素）
    OpenVdbModel dupModel;
    dupModel.AddPoint(Eigen::Vector3f{0.0f, 0.0f, 0.0f});
    dupModel.AddPoint(Eigen::Vector3f{10.0f, 0.0f, 0.0f});
    dupModel.AddPoint(Eigen::Vector3f{10.6f, 0.0f, 0.0f});
    BOOST_REQUIRE_EQUAL(dupModel.PointCount(), 3u);
    dupModel.Downsample(2.0f);
    BOOST_CHECK_EQUAL(dupModel.PointCount(), 2u);

    const auto tempPath = std::filesystem::temp_directory_path() / "hsba_openvdb_model_test.xyz";
    BOOST_REQUIRE(model.Save(tempPath.string(), ModelFormat::XYZ));

    OpenVdbModel reloaded;
    BOOST_REQUIRE(reloaded.Load(tempPath.string()));
    BOOST_REQUIRE_EQUAL(reloaded.PointCount(), model.PointCount());
    // 往返后点集必须一致（不保证顺序，按坐标排序后逐点比对）
    auto reloadedPoints = reloaded.Points();
    auto savedPoints = model.Points();
    const auto pointLess = [](const Eigen::Vector3f& lhs, const Eigen::Vector3f& rhs) {
        if (lhs.x() != rhs.x())
        {
            return lhs.x() < rhs.x();
        }
        if (lhs.y() != rhs.y())
        {
            return lhs.y() < rhs.y();
        }
        return lhs.z() < rhs.z();
    };
    std::sort(reloadedPoints.begin(), reloadedPoints.end(), pointLess);
    std::sort(savedPoints.begin(), savedPoints.end(), pointLess);
    for (std::size_t i = 0; i < savedPoints.size(); ++i)
    {
        BOOST_CHECK_CLOSE(reloadedPoints[i].x(), savedPoints[i].x(), 1e-4);
        BOOST_CHECK_CLOSE(reloadedPoints[i].y(), savedPoints[i].y(), 1e-4);
        BOOST_CHECK_CLOSE(reloadedPoints[i].z(), savedPoints[i].z(), 1e-4);
    }

    std::filesystem::remove(tempPath);
}

BOOST_AUTO_TEST_CASE(point_cloud_neighbors_and_filter)
{
    OpenVdbModel model;
    model.AddPoint(Eigen::Vector3f{0.0f, 0.0f, 0.0f});
    model.AddPoint(Eigen::Vector3f{1.0f, 0.0f, 0.0f});
    model.AddPoint(Eigen::Vector3f{2.0f, 0.0f, 0.0f});
    model.AddPoint(Eigen::Vector3f{3.0f, 0.0f, 0.0f});

    const auto nearest = model.NearestNeighbor(Eigen::Vector3f{0.9f, 0.0f, 0.0f});
    BOOST_REQUIRE_EQUAL(nearest.size(), 1u);
    BOOST_CHECK_CLOSE(nearest.front().x(), 1.0f, 1e-5f);

    const auto knn = model.KNN(Eigen::Vector3f{0.9f, 0.0f, 0.0f}, 2u);
    BOOST_REQUIRE_EQUAL(knn.size(), 2u);
    BOOST_CHECK_CLOSE(knn[0].x(), 1.0f, 1e-5f);  // 距离 0.1，最近
    BOOST_CHECK_CLOSE(knn[1].x(), 0.0f, 1e-5f);  // 距离 0.9，次近

    // k 大于点数时仅返回全部点
    const auto knnOverflow = model.KNN(Eigen::Vector3f{0.0f, 0.0f, 0.0f}, 100u);
    BOOST_CHECK_EQUAL(knnOverflow.size(), 4u);

    const auto filtered = model.Filter([](const Eigen::Vector3f& point) {
        return point.x() >= 2.0f;
    });
    BOOST_REQUIRE_EQUAL(filtered.size(), 2u);
    BOOST_CHECK_CLOSE(filtered.front().x(), 2.0f, 1e-5f);
    BOOST_CHECK_CLOSE(filtered.back().x(), 3.0f, 1e-5f);

    // 无匹配时返回空集
    const auto filteredNone = model.Filter([](const Eigen::Vector3f& point) {
        return point.x() > 100.0f;
    });
    BOOST_CHECK(filteredNone.empty());
}
