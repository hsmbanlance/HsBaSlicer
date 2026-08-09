#define BOOST_TEST_MODULE UserCustomCADModelTests
#include <boost/test/included/unit_test.hpp>

#include <filesystem>
#include <string>
#include <vector>

#include "base/error.hpp"
#include "cadmodel/UserCustomCADModel.hpp"

using namespace HsBa::Slicer;

namespace
{
// locate the mock dll by absolute path, relative loading depends on the current working directory
const std::string kMockDllPath = []
{
#ifdef MOCK_CAD_DLL_DIR
    std::filesystem::path dllPath(MOCK_CAD_DLL_DIR);
#ifdef NDEBUG
#ifdef _WIN32
    dllPath /= "mock_cad_dll.dll";
#elif __APPLE__
    dllPath /= "libmock_cad_dll.dylib";
#else
    dllPath /= "libmock_cad_dll.so";
#endif
#else
#ifdef _WIN32
    dllPath /= "mock_cad_dlld.dll";
#elif __APPLE__
    dllPath /= "libmock_cad_dlld.dylib";
#else
    dllPath /= "libmock_cad_dlld.so";
#endif  // debug build
#endif  // NDEBUG
    return dllPath.string();
#else
    return std::string("mock_cad_dll");
#endif
}();
constexpr std::string_view kMockFunName = "mockcad";
}  // namespace

BOOST_AUTO_TEST_CASE(load_dll_and_load_model)
{
    UserCustomCADModel model;
    // before the dll is loaded the model behaves as empty
    BOOST_CHECK(!model.Save("unused.stp", ModelFormat::UnknownPLY));
    BOOST_CHECK_THROW(model.Load("unused.stp"), RuntimeError);

    model.LoadDll(kMockDllPath, kMockFunName);
    BOOST_CHECK(model.Load("mock.stp"));

    // the fixed stub values prove the virtual calls cross the dll boundary
    Eigen::Vector3f min, max;
    model.BoundingBox(min, max);
    BOOST_CHECK_SMALL(min.x(), 1e-5f);
    BOOST_CHECK_SMALL(min.y(), 1e-5f);
    BOOST_CHECK_SMALL(min.z(), 1e-5f);
    BOOST_CHECK_CLOSE(max.x(), 1.0f, 1e-4);
    BOOST_CHECK_CLOSE(max.y(), 1.0f, 1e-4);
    BOOST_CHECK_CLOSE(max.z(), 1.0f, 1e-4);
    BOOST_CHECK_CLOSE(model.Volume(), 1.0f, 1e-4);

    const auto [vertices, faces] = model.TriangleMesh();
    BOOST_CHECK_EQUAL(vertices.rows(), 0);
    BOOST_CHECK_EQUAL(faces.rows(), 0);
    BOOST_CHECK(model.Save("unused.stp", ModelFormat::UnknownPLY));
}

BOOST_AUTO_TEST_CASE(transforms_are_forwarded)
{
    UserCustomCADModel model;
    model.LoadDll(kMockDllPath, kMockFunName);
    BOOST_CHECK(model.Load("mock.stp"));

    // only the call plumbing is verified here, results belong to the user dll
    BOOST_CHECK_NO_THROW(model.Translate(Eigen::Vector3f{1.0f, 2.0f, 3.0f}));
    BOOST_CHECK_NO_THROW(model.Rotate(Eigen::Quaternionf::Identity()));
    BOOST_CHECK_NO_THROW(model.Scale(2.0f));
    BOOST_CHECK_NO_THROW(model.Scale(Eigen::Vector3f{1.0f, 2.0f, 3.0f}));
    BOOST_CHECK_NO_THROW(model.Transform(Eigen::Isometry3f::Identity()));
    BOOST_CHECK_NO_THROW(model.Transform(Eigen::Matrix4f::Identity()));
    BOOST_CHECK_NO_THROW(model.Transform(Eigen::Transform<float, 3, Eigen::Affine>::Identity()));
}

BOOST_AUTO_TEST_CASE(factory_functions)
{
    UserCustomCADDll dll(kMockDllPath, kMockFunName);

    const auto createBox = dll.GetCreateBoxFunc();
    const auto createSphere = dll.GetCreateSphereFunc();
    const auto createCylinder = dll.GetCreateCylinderFunc();
    const auto setThickness = dll.GetSetThicknessFunc();
    const auto destroyModel = dll.GetDestroyModelFunc();
    BOOST_REQUIRE(createBox != nullptr);
    BOOST_REQUIRE(createSphere != nullptr);
    BOOST_REQUIRE(createCylinder != nullptr);
    BOOST_REQUIRE(setThickness != nullptr);
    BOOST_REQUIRE(destroyModel != nullptr);

    auto* box = createBox(2.0f, 3.0f, 4.0f);
    auto* sphere = createSphere(2.0f, 16);
    auto* cylinder = createCylinder(1.0f, 5.0f, 8);
    BOOST_CHECK(box != nullptr);
    BOOST_CHECK(sphere != nullptr);
    BOOST_CHECK(cylinder != nullptr);

    // the call must reach the dll without crashing
    BOOST_CHECK_NO_THROW(setThickness(box, 0.5f));

    destroyModel(box);
    destroyModel(sphere);
    destroyModel(cylinder);
}

BOOST_AUTO_TEST_CASE(create_prism_and_prism_ex)
{
    UserCustomCADDll dll(kMockDllPath, kMockFunName);
    const auto createPrism = dll.GetCreatePrismFunc();
    const auto createPrismEx = dll.GetCreatePrismExFunc();
    const auto destroyModel = dll.GetDestroyModelFunc();
    BOOST_REQUIRE(createPrism != nullptr);
    BOOST_REQUIRE(createPrismEx != nullptr);
    BOOST_REQUIRE(destroyModel != nullptr);

    // the polygon buffers must be readable across the dll boundary
    std::vector<HsBaVector2f_t> square{{0.0f, 0.0f}, {2.0f, 0.0f}, {2.0f, 2.0f}, {0.0f, 2.0f}};
    HsBaPoly2D_t polygon{square.data(), square.size()};
    HsBaVector3f_t direction{0.0f, 0.0f, 3.0f};

    auto* prism = createPrism(polygon, direction);
    BOOST_REQUIRE(prism != nullptr);
    destroyModel(prism);

    std::vector<HsBaVector2f_t> unitSquare{{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};
    std::vector<HsBaPoly2D_t> polygons{{square.data(), square.size()}, {unitSquare.data(), unitSquare.size()}};
    HsBaPolys2D_t polys{polygons.data(), polygons.size()};

    auto* prismEx = createPrismEx(polys, direction);
    BOOST_REQUIRE(prismEx != nullptr);
    destroyModel(prismEx);

    // invalid input is rejected by the dll
    HsBaPoly2D_t emptyPolygon{nullptr, 0};
    BOOST_CHECK(createPrism(emptyPolygon, direction) == nullptr);
}

BOOST_AUTO_TEST_CASE(boolean_operation)
{
    UserCustomCADModel left;
    UserCustomCADModel right;
    left.LoadDll(kMockDllPath, kMockFunName);
    right.LoadDll(kMockDllPath, kMockFunName);
    BOOST_CHECK(left.Load("mock.stp"));
    BOOST_CHECK(right.Load("mock.stp"));

    BOOST_CHECK_NO_THROW(left.BooleanOperation(right, "union"));
    BOOST_CHECK_NO_THROW(left.BooleanOperation(right, "intersection"));
    BOOST_CHECK_NO_THROW(left.BooleanOperation(right, "difference"));

    // unknown operation fails inside the dll and the wrapper throws
    BOOST_CHECK_THROW(left.BooleanOperation(right, "xor"), RuntimeError);
}

BOOST_AUTO_TEST_CASE(error_cases)
{
    UserCustomCADModel model;
    // operations without a loaded model throw
    BOOST_CHECK_THROW(model.Translate(Eigen::Vector3f::Zero()), RuntimeError);
    BOOST_CHECK_THROW(model.Volume(), RuntimeError);

    model.LoadDll(kMockDllPath, kMockFunName);
    BOOST_CHECK(model.Load("mock.stp"));

    // a default constructed model does not share the dll
    UserCustomCADModel other;
    BOOST_CHECK_THROW(model.BooleanOperation(other, "union"), RuntimeError);

    // same dll but the other model was never created, the dll reports failure
    UserCustomCADModel unloaded;
    unloaded.LoadDll(kMockDllPath, kMockFunName);
    BOOST_CHECK_THROW(model.BooleanOperation(unloaded, "union"), RuntimeError);

    model.UnloadDll();
    BOOST_CHECK_THROW(model.Load("mock.stp"), RuntimeError);
}
