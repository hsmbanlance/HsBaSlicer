#define BOOST_TEST_MODULE UserCustomPointCloudModelTests
#include <boost/test/included/unit_test.hpp>

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "pointcloud/UserCustomPointCloudModel.hpp"

using namespace HsBa::Slicer;

namespace
{
// locate the mock dll by absolute path, relative loading depends on the current working directory
const std::string kMockDllPath = []
{
#ifdef MOCK_POINT_CLOUD_DLL_DIR
    std::filesystem::path dllPath(MOCK_POINT_CLOUD_DLL_DIR);
#ifdef _WIN32
    dllPath /= "mock_point_cloud_dll.dll";
#elif __APPLE__
    dllPath /= "libmock_point_cloud_dll.dylib";
#else
    dllPath /= "libmock_point_cloud_dll.so";
#endif
    return dllPath.string();
#else
    return std::string("mock_point_cloud_dll");
#endif
}();
constexpr std::string_view kMockFunName = "mockpc";
}  // namespace

BOOST_AUTO_TEST_CASE(load_dll_and_add_points)
{
    UserCustomPointCloudModel model;
    // 未加载 dll 前按空点云处理，与 OpenVdbModel 默认构造行为一致
    BOOST_CHECK(model.IsEmpty());
    BOOST_CHECK_EQUAL(model.PointCount(), 0u);

    model.LoadDll(kMockDllPath, kMockFunName);
    model.AddPoint(Eigen::Vector3f{0.0f, 0.0f, 0.0f});
    model.AddPoint(Eigen::Vector3f{1.0f, 2.0f, 3.0f});
    model.AddPoints({Eigen::Vector3f{4.0f, 0.5f, 1.0f}});

    BOOST_CHECK(!model.IsEmpty());
    BOOST_CHECK_EQUAL(model.PointCount(), 3u);

    const auto points = model.Points();
    BOOST_REQUIRE_EQUAL(points.size(), 3u);
    BOOST_CHECK_CLOSE(points[1].x(), 1.0f, 1e-5);
    BOOST_CHECK_CLOSE(points[1].y(), 2.0f, 1e-5);
    BOOST_CHECK_CLOSE(points[1].z(), 3.0f, 1e-5);

    model.Clear();
    BOOST_CHECK(model.IsEmpty());
    BOOST_CHECK_EQUAL(model.PointCount(), 0u);
}

BOOST_AUTO_TEST_CASE(set_from_vertices_and_to_vertices)
{
    UserCustomPointCloudModel model;
    model.LoadDll(kMockDllPath, kMockFunName);

    Eigen::MatrixXf vertices(2, 3);
    vertices << 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f;
    model.SetFromVertices(vertices);
    BOOST_CHECK_EQUAL(model.PointCount(), 2u);

    const auto roundTrip = model.ToVertices();
    BOOST_REQUIRE_EQUAL(roundTrip.rows(), 2);
    BOOST_CHECK_CLOSE(roundTrip(0, 0), 1.0f, 1e-5);
    BOOST_CHECK_CLOSE(roundTrip(0, 2), 3.0f, 1e-5);
    BOOST_CHECK_CLOSE(roundTrip(1, 1), 5.0f, 1e-5);
}

BOOST_AUTO_TEST_CASE(centroid)
{
    UserCustomPointCloudModel model;
    model.LoadDll(kMockDllPath, kMockFunName);
    model.AddPoints({Eigen::Vector3f{0.0f, 0.0f, 0.0f}, Eigen::Vector3f{2.0f, 0.0f, 0.0f},
                     Eigen::Vector3f{0.0f, 4.0f, 0.0f}});

    const auto centroid = model.Centroid();
    BOOST_CHECK_CLOSE(centroid.x(), 2.0f / 3.0f, 1e-4);
    BOOST_CHECK_CLOSE(centroid.y(), 4.0f / 3.0f, 1e-4);
    BOOST_CHECK_SMALL(centroid.z(), 1e-5f);
}

BOOST_AUTO_TEST_CASE(merge_point_clouds)
{
    UserCustomPointCloudModel left;
    UserCustomPointCloudModel right;
    left.LoadDll(kMockDllPath, kMockFunName);
    right.LoadDll(kMockDllPath, kMockFunName);

    left.AddPoints({Eigen::Vector3f{0.0f, 0.0f, 0.0f}, Eigen::Vector3f{1.0f, 0.0f, 0.0f}});
    right.AddPoint(Eigen::Vector3f{2.0f, 0.0f, 0.0f});

    left.Merge(right);
    BOOST_CHECK_EQUAL(left.PointCount(), 3u);
    const auto points = left.Points();
    BOOST_REQUIRE_EQUAL(points.size(), 3u);
    BOOST_CHECK_CLOSE(points[2].x(), 2.0f, 1e-5);
}

BOOST_AUTO_TEST_CASE(downsample_and_voxelize)
{
    UserCustomPointCloudModel model;
    model.LoadDll(kMockDllPath, kMockFunName);
    // 前两点落在同一体素，第三点位于另一体素
    model.AddPoints({Eigen::Vector3f{0.1f, 0.1f, 0.1f}, Eigen::Vector3f{0.2f, 0.2f, 0.2f},
                     Eigen::Vector3f{1.5f, 0.0f, 0.0f}});

    model.Downsample(1.0f);
    BOOST_CHECK_EQUAL(model.PointCount(), 2u);

    model.Voxelize(1.0f);
    const auto points = model.Points();
    BOOST_REQUIRE_EQUAL(points.size(), 2u);
    BOOST_CHECK_CLOSE(points[0].x(), 0.5f, 1e-5);
    BOOST_CHECK_CLOSE(points[0].y(), 0.5f, 1e-5);
    BOOST_CHECK_CLOSE(points[0].z(), 0.5f, 1e-5);
    BOOST_CHECK_CLOSE(points[1].x(), 1.5f, 1e-5);
    BOOST_CHECK_CLOSE(points[1].y(), 0.5f, 1e-5);
}

BOOST_AUTO_TEST_CASE(remove_statistical_outliers)
{
    UserCustomPointCloudModel model;
    model.LoadDll(kMockDllPath, kMockFunName);
    // 5 个密集点 + 1 个远离群点
    model.AddPoints({Eigen::Vector3f{0.0f, 0.0f, 0.0f}, Eigen::Vector3f{0.1f, 0.0f, 0.0f},
                     Eigen::Vector3f{0.0f, 0.1f, 0.0f}, Eigen::Vector3f{0.1f, 0.1f, 0.0f},
                     Eigen::Vector3f{0.05f, 0.05f, 0.0f}, Eigen::Vector3f{100.0f, 100.0f, 100.0f}});

    model.RemoveStatisticalOutliers(3, 1.0f);
    BOOST_CHECK_EQUAL(model.PointCount(), 5u);
    for (const auto& point : model.Points())
    {
        BOOST_CHECK(point.norm() < 1.0f);
    }
}

BOOST_AUTO_TEST_CASE(compute_normals)
{
    UserCustomPointCloudModel model;
    model.LoadDll(kMockDllPath, kMockFunName);
    // 关于原点对称的两点，mock 法向为质心径向方向
    model.AddPoints({Eigen::Vector3f{1.0f, 0.0f, 0.0f}, Eigen::Vector3f{-1.0f, 0.0f, 0.0f}});

    const auto normals = model.ComputeNormals(2);
    BOOST_REQUIRE_EQUAL(normals.rows(), 2);
    BOOST_CHECK_CLOSE(normals(0, 0), 1.0f, 1e-4);
    BOOST_CHECK_SMALL(normals(0, 1), 1e-5f);
    BOOST_CHECK_CLOSE(normals(1, 0), -1.0f, 1e-4);
    BOOST_CHECK_SMALL(normals(1, 2), 1e-5f);
}

BOOST_AUTO_TEST_CASE(imodel_interface)
{
    UserCustomPointCloudModel model;
    model.LoadDll(kMockDllPath, kMockFunName);
    model.AddPoints({Eigen::Vector3f{0.0f, 0.0f, 0.0f}, Eigen::Vector3f{1.0f, 2.0f, 3.0f}});

    Eigen::Vector3f min, max;
    model.BoundingBox(min, max);
    BOOST_CHECK_SMALL(min.x(), 1e-5f);
    BOOST_CHECK_CLOSE(max.x(), 1.0f, 1e-5);
    BOOST_CHECK_CLOSE(max.z(), 3.0f, 1e-5);
    BOOST_CHECK_SMALL(model.Volume(), 1e-5f);

    model.Translate(Eigen::Vector3f{1.0f, -1.0f, 0.0f});
    const auto points = model.Points();
    BOOST_REQUIRE_EQUAL(points.size(), 2u);
    BOOST_CHECK_CLOSE(points[1].x(), 2.0f, 1e-5);
    BOOST_CHECK_CLOSE(points[1].y(), 1.0f, 1e-5);

    const auto [vertices, faces] = model.TriangleMesh();
    BOOST_CHECK_EQUAL(vertices.rows(), 2);
    BOOST_CHECK_EQUAL(faces.rows(), 0);

    BOOST_CHECK(model.Save("unused.xyz", ModelFormat::XYZ));
}

BOOST_AUTO_TEST_CASE(load_from_file)
{
    UserCustomPointCloudModel model;
    model.LoadDll(kMockDllPath, kMockFunName);
    BOOST_CHECK(model.Load("unused.xyz"));
    BOOST_CHECK(model.IsEmpty());
    model.UnloadDll();
}
