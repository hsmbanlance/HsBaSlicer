/** @file mock_cad_dll.cpp
 * @brief A mock CAD dynamic library used by user_custom_cad_model_test.
 * It exports C style functions with the "mockcad_" prefix that mimic the user custom CAD dll
 * contract of UserCustomCADModel. The mock only verifies the dll loading and call plumbing,
 * the actual CAD results are the responsibility of the real user provided dll.
 */
#include <cstring>
#include <string>
#include <utility>

#include <Eigen/Core>
#include <Eigen/Geometry>

#include "base/IModel.hpp"

#ifdef _WIN32
#define MOCK_CAD_EXPORT extern "C" __declspec(dllexport)
#else
#define MOCK_CAD_EXPORT extern "C" __attribute__((visibility("default")))
#endif

namespace
{
/// @brief A minimal CAD model stub, every query returns fixed values.
class MockCADModel final : public HsBa::Slicer::IModel
{
public:
    bool Load(std::string_view fileName) override { return !fileName.empty(); }
    bool Save(std::string_view, const HsBa::Slicer::ModelFormat) const override { return true; }

    void Translate(const Eigen::Vector3f&) override {}
    void Rotate(const Eigen::Quaternionf&) override {}
    void Scale(const float) override {}
    void Scale(const Eigen::Vector3f&) override {}
    void Transform(const Eigen::Isometry3f&) override {}
    void Transform(const Eigen::Matrix4f&) override {}
    void Transform(const Eigen::Transform<float, 3, Eigen::Affine>&) override {}

    void BoundingBox(Eigen::Vector3f& min, Eigen::Vector3f& max) const override
    {
        min = Eigen::Vector3f::Zero();
        max = Eigen::Vector3f::Ones();
    }
    float Volume() const override { return 1.0f; }
    std::pair<Eigen::MatrixXf, Eigen::MatrixXi> TriangleMesh() const override
    {
        return {Eigen::MatrixXf{}, Eigen::MatrixXi{}};
    }
};
}  // namespace

MOCK_CAD_EXPORT HsBa::Slicer::IModel* mockcad_create_model()
{
    return new MockCADModel();
}

MOCK_CAD_EXPORT void mockcad_destroy_model(HsBa::Slicer::IModel* model)
{
    delete model;
}

MOCK_CAD_EXPORT bool mockcad_boolean_operation(HsBa::Slicer::IModel* target, const HsBa::Slicer::IModel* other,
                                               const char* operation)
{
    if (target == nullptr || other == nullptr || operation == nullptr)
    {
        return false;
    }
    const std::string operationName(operation);
    return operationName == "union" || operationName == "intersection" || operationName == "difference";
}

MOCK_CAD_EXPORT HsBa::Slicer::IModel* mockcad_create_box(float, float, float)
{
    return new MockCADModel();
}

MOCK_CAD_EXPORT HsBa::Slicer::IModel* mockcad_create_sphere(float, int)
{
    return new MockCADModel();
}

MOCK_CAD_EXPORT HsBa::Slicer::IModel* mockcad_create_cylinder(float, float, int)
{
    return new MockCADModel();
}

MOCK_CAD_EXPORT void mockcad_set_thickness(HsBa::Slicer::IModel*, float)
{
}

MOCK_CAD_EXPORT HsBa::Slicer::IModel* mockcad_create_prism(HsBaPoly2D_t polygon, HsBaVector3f_t)
{
    // the polygon must be readable across the dll boundary
    if (polygon.vertices == nullptr || polygon.vertexCount == 0)
    {
        return nullptr;
    }
    return new MockCADModel();
}

MOCK_CAD_EXPORT HsBa::Slicer::IModel* mockcad_create_prism_ex(HsBaPolys2D_t polygons, HsBaVector3f_t)
{
    // the polygon list must be readable across the dll boundary
    if (polygons.polygons == nullptr || polygons.vertexCount == 0)
    {
        return nullptr;
    }
    return new MockCADModel();
}
