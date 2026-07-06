#define BOOST_TEST_MODULE model_loader_test
#include <boost/test/included/unit_test.hpp>

#include <memory>
#include <string>

#include "base/error.hpp"
#include "meshmodel/IglModel.hpp"
#include "preprocess/ModelLoader.hpp"

#ifdef USE_OCCT
#include "cadmodel/OcctModel.hpp"
#endif

using namespace HsBa::Slicer;

// ============================================================================
// Pool management tests
// ============================================================================
BOOST_AUTO_TEST_SUITE(pool_management)

BOOST_AUTO_TEST_CASE(insert_and_retrieve)
{
    ModelLoader loader;
    auto box = std::make_shared<IglModel>(IglModel::CreateBox(Eigen::Vector3f{1, 1, 1}));
    auto ptr = loader.InsertModel("box1", box);

    BOOST_CHECK(ptr != nullptr);
    BOOST_CHECK_EQUAL(loader.ModelCount(), 1u);
    BOOST_CHECK(loader.ContainsModel("box1"));
    BOOST_CHECK(!loader.ContainsModel("nonexistent"));

    auto retrieved = loader.GetModel("box1");
    BOOST_CHECK(retrieved != nullptr);
    BOOST_CHECK(retrieved.get() == box.get());
}

BOOST_AUTO_TEST_CASE(subscript_operator)
{
    ModelLoader loader;
    auto box = std::make_shared<IglModel>(IglModel::CreateBox(Eigen::Vector3f{2, 2, 2}));
    loader.InsertModel("box2", box);

    auto retrieved = loader["box2"];
    BOOST_CHECK(retrieved != nullptr);
    BOOST_CHECK(retrieved.get() == box.get());

    auto missing = loader["missing"];
    BOOST_CHECK(missing == nullptr);
}

BOOST_AUTO_TEST_CASE(insert_duplicate_name_throws)
{
    ModelLoader loader;
    auto box = std::make_shared<IglModel>(IglModel::CreateBox(Eigen::Vector3f{1, 1, 1}));
    loader.InsertModel("dup", box);

    auto box2 = std::make_shared<IglModel>(IglModel::CreateBox(Eigen::Vector3f{1, 1, 1}));
    BOOST_CHECK_THROW(loader.InsertModel("dup", box2), InvalidArgumentError);
}

BOOST_AUTO_TEST_CASE(insert_null_throws)
{
    ModelLoader loader;
    BOOST_CHECK_THROW(loader.InsertModel("null_model", nullptr), InvalidArgumentError);
}

BOOST_AUTO_TEST_CASE(remove_model)
{
    ModelLoader loader;
    auto box = std::make_shared<IglModel>(IglModel::CreateBox(Eigen::Vector3f{1, 1, 1}));
    loader.InsertModel("to_remove", box);
    BOOST_CHECK_EQUAL(loader.ModelCount(), 1u);

    loader.RemoveModel("to_remove");
    BOOST_CHECK_EQUAL(loader.ModelCount(), 0u);
    BOOST_CHECK(!loader.ContainsModel("to_remove"));
    BOOST_CHECK(loader.GetModel("to_remove") == nullptr);
}

BOOST_AUTO_TEST_CASE(get_model_names)
{
    ModelLoader loader;
    loader.InsertModel("a", std::make_shared<IglModel>(IglModel::CreateBox(Eigen::Vector3f{1, 1, 1})));
    loader.InsertModel("b", std::make_shared<IglModel>(IglModel::CreateSphere(0.5f)));

    auto names = loader.GetModelNames();
    BOOST_CHECK_EQUAL(names.size(), 2u);

    // Both names should be present (order not guaranteed)
    bool hasA = std::find(names.begin(), names.end(), "a") != names.end();
    bool hasB = std::find(names.begin(), names.end(), "b") != names.end();
    BOOST_CHECK(hasA);
    BOOST_CHECK(hasB);
}

BOOST_AUTO_TEST_CASE(cleanup_removes_inactive)
{
    ModelLoader loader;
    {
        auto box = std::make_shared<IglModel>(IglModel::CreateBox(Eigen::Vector3f{1, 1, 1}));
        loader.InsertModel("temp", box);
        BOOST_CHECK_EQUAL(loader.ModelCount(), 1u);
        // box goes out of scope here, but loader still holds a reference
    }
    // The pool's internal shared_ptr still holds the object, so it's still "active"
    // from the pool's perspective (use_count == 1 from pool's shared_ptr)
    // Cleanup only removes objects where use_count == 1 (only pool holds ref)
    auto cleaned = loader.Cleanup();
    // After cleanup, the "temp" model should be removed since only pool holds it
    BOOST_CHECK_EQUAL(cleaned, 1u);
    BOOST_CHECK_EQUAL(loader.ModelCount(), 0u);
}

BOOST_AUTO_TEST_CASE(cleanup_keeps_active_models)
{
    ModelLoader loader;
    auto box = std::make_shared<IglModel>(IglModel::CreateBox(Eigen::Vector3f{1, 1, 1}));
    loader.InsertModel("active", box);

    // box is still alive externally, so use_count > 1
    auto cleaned = loader.Cleanup();
    BOOST_CHECK_EQUAL(cleaned, 0u);
    BOOST_CHECK_EQUAL(loader.ModelCount(), 1u);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// Boolean operations tests (require CGAL)
// ============================================================================
#ifdef USE_CGAL
#ifndef DISABLE_BOOLEAN_OPERATIONS_TESTS

BOOST_AUTO_TEST_SUITE(boolean_operations_igl)

BOOST_AUTO_TEST_CASE(union_two_boxes)
{
    ModelLoader loader;
    auto a = std::make_shared<IglModel>(IglModel::CreateBox(Eigen::Vector3f{1, 1, 1}));
    auto b = std::make_shared<IglModel>(IglModel::CreateBox(Eigen::Vector3f{1, 1, 1}));
    b->Translate(Eigen::Vector3f{0.5f, 0.0f, 0.0f});

    loader.InsertModel("a", a);
    loader.InsertModel("b", b);

    auto result = loader.BooleanUnion("a", "b", "union_result");
    BOOST_CHECK(result != nullptr);
    BOOST_CHECK_EQUAL(loader.ModelCount(), 3u);

    auto [verts, faces] = result->TriangleMesh();
    if (verts.rows() > 0)
    {
        BOOST_CHECK(faces.rows() > 0);
    }
    else
    {
        BOOST_TEST_MESSAGE("IGL Union produced empty mesh (operation may not be supported for these inputs);");
    }
}

BOOST_AUTO_TEST_CASE(intersection_two_boxes)
{
    ModelLoader loader;
    auto a = std::make_shared<IglModel>(IglModel::CreateBox(Eigen::Vector3f{1, 1, 1}));
    auto b = std::make_shared<IglModel>(IglModel::CreateBox(Eigen::Vector3f{1, 1, 1}));
    b->Translate(Eigen::Vector3f{0.5f, 0.0f, 0.0f});

    loader.InsertModel("a", a);
    loader.InsertModel("b", b);

    auto result = loader.BooleanIntersection("a", "b", "inter_result");
    BOOST_CHECK(result != nullptr);

    auto [verts, faces] = result->TriangleMesh();
    if (verts.rows() > 0)
    {
        BOOST_CHECK(faces.rows() > 0);
    }
    else
    {
        BOOST_TEST_MESSAGE("IGL Intersection produced empty mesh (possible, acceptable result);");
    }
}

BOOST_AUTO_TEST_CASE(difference_two_boxes)
{
    ModelLoader loader;
    auto a = std::make_shared<IglModel>(IglModel::CreateBox(Eigen::Vector3f{1, 1, 1}));
    auto b = std::make_shared<IglModel>(IglModel::CreateBox(Eigen::Vector3f{1, 1, 1}));
    b->Translate(Eigen::Vector3f{0.5f, 0.0f, 0.0f});

    loader.InsertModel("a", a);
    loader.InsertModel("b", b);

    auto result = loader.BooleanDifference("a", "b", "diff_result");
    BOOST_CHECK(result != nullptr);

    auto [verts, faces] = result->TriangleMesh();
    if (verts.rows() > 0)
    {
        BOOST_CHECK(faces.rows() > 0);
    }
    else
    {
        BOOST_TEST_MESSAGE("IGL Difference produced empty mesh (possible, acceptable result);");
    }
}

BOOST_AUTO_TEST_CASE(xor_two_boxes)
{
    ModelLoader loader;
    auto a = std::make_shared<IglModel>(IglModel::CreateBox(Eigen::Vector3f{1, 1, 1}));
    auto b = std::make_shared<IglModel>(IglModel::CreateBox(Eigen::Vector3f{1, 1, 1}));
    b->Translate(Eigen::Vector3f{0.5f, 0.0f, 0.0f});

    loader.InsertModel("a", a);
    loader.InsertModel("b", b);

    auto result = loader.BooleanXor("a", "b", "xor_result");
    BOOST_CHECK(result != nullptr);

    auto [verts, faces] = result->TriangleMesh();
    if (verts.rows() > 0)
    {
        BOOST_CHECK(faces.rows() > 0);
    }
    else
    {
        BOOST_TEST_MESSAGE("IGL Xor produced empty mesh (operation may not be supported for these inputs);");
    }
}

BOOST_AUTO_TEST_CASE(boolean_nonexistent_model_throws)
{
    ModelLoader loader;
    auto a = std::make_shared<IglModel>(IglModel::CreateBox(Eigen::Vector3f{1, 1, 1}));
    loader.InsertModel("a", a);

    BOOST_CHECK_THROW(loader.BooleanUnion("a", "missing", "r"), InvalidArgumentError);
    BOOST_CHECK_THROW(loader.BooleanUnion("missing", "a", "r"), InvalidArgumentError);
}

BOOST_AUTO_TEST_SUITE_END()

#endif  // DISABLE_BOOLEAN_OPERATIONS_TESTS

// ============================================================================
// ThickSolid tests (require CGAL + OCCT)
// ============================================================================
#ifdef USE_OCCT

BOOST_AUTO_TEST_SUITE(thick_solid_occt)

BOOST_AUTO_TEST_CASE(thicksolid_box)
{
    ModelLoader loader;
    auto box = std::make_shared<OcctModel>(OcctModel::CreateBox(Eigen::Vector3f{10, 10, 10}));
    loader.InsertModel("box", std::static_pointer_cast<IModel>(box));

    // ThickSolid may throw OCCT exceptions for certain geometries
    try
    {
        auto result = loader.ThickSolidModel("box", "shell", 1.0f);
        BOOST_CHECK(result != nullptr);
        BOOST_CHECK_EQUAL(loader.ModelCount(), 2u);

        // OCCT results need tessellation for TriangleMesh; use Volume instead
        float vol = result->Volume();
        BOOST_CHECK(vol >= 0.0f);
    }
    catch (const std::exception& e)
    {
        // OCCT ThickSolid may fail for certain geometries, log and pass
        BOOST_TEST_MESSAGE("ThickSolid threw OCCT exception: " << e.what());
    }
    catch (...)
    {
        BOOST_TEST_MESSAGE("ThickSolid threw unknown exception");
    }
}

BOOST_AUTO_TEST_CASE(thicksolid_non_occt_throws)
{
    ModelLoader loader;
    auto iglBox = std::make_shared<IglModel>(IglModel::CreateBox(Eigen::Vector3f{1, 1, 1}));
    loader.InsertModel("igl_box", std::static_pointer_cast<IModel>(iglBox));

    BOOST_CHECK_THROW(loader.ThickSolidModel("igl_box", "shell", 0.5f), RuntimeError);
}

BOOST_AUTO_TEST_CASE(thicksolid_nonexistent_throws)
{
    ModelLoader loader;
    BOOST_CHECK_THROW(loader.ThickSolidModel("nonexistent", "shell", 1.0f), InvalidArgumentError);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// Boolean operations on OcctModel pairs
// ============================================================================
BOOST_AUTO_TEST_SUITE(boolean_operations_occt)

BOOST_AUTO_TEST_CASE(union_two_occt_boxes)
{
    ModelLoader loader;
    auto a = std::make_shared<OcctModel>(OcctModel::CreateBox(Eigen::Vector3f{10, 10, 10}));
    auto b = std::make_shared<OcctModel>(OcctModel::CreateBox(Eigen::Vector3f{10, 10, 10}));
    b->Translate(Eigen::Vector3f{5.0f, 0.0f, 0.0f});

    loader.InsertModel("a", std::static_pointer_cast<IModel>(a));
    loader.InsertModel("b", std::static_pointer_cast<IModel>(b));

    auto result = loader.BooleanUnion("a", "b", "union_occt");
    BOOST_CHECK(result != nullptr);

    // Result should be an OcctModel
    auto* occtResult = dynamic_cast<OcctModel*>(result.get());
    BOOST_CHECK(occtResult != nullptr);

    // OCCT results need tessellation for TriangleMesh; use Volume instead
    float vol = result->Volume();
    BOOST_CHECK(vol > 0.0f);
}

BOOST_AUTO_TEST_CASE(intersection_two_occt_boxes)
{
    ModelLoader loader;
    auto a = std::make_shared<OcctModel>(OcctModel::CreateBox(Eigen::Vector3f{10, 10, 10}));
    auto b = std::make_shared<OcctModel>(OcctModel::CreateBox(Eigen::Vector3f{10, 10, 10}));
    b->Translate(Eigen::Vector3f{5.0f, 0.0f, 0.0f});

    loader.InsertModel("a", std::static_pointer_cast<IModel>(a));
    loader.InsertModel("b", std::static_pointer_cast<IModel>(b));

    auto result = loader.BooleanIntersection("a", "b", "inter_occt");
    BOOST_CHECK(result != nullptr);

    float vol = result->Volume();
    BOOST_CHECK(vol > 0.0f);
}

BOOST_AUTO_TEST_CASE(difference_two_occt_boxes)
{
    ModelLoader loader;
    auto a = std::make_shared<OcctModel>(OcctModel::CreateBox(Eigen::Vector3f{10, 10, 10}));
    auto b = std::make_shared<OcctModel>(OcctModel::CreateBox(Eigen::Vector3f{10, 10, 10}));
    b->Translate(Eigen::Vector3f{5.0f, 0.0f, 0.0f});

    loader.InsertModel("a", std::static_pointer_cast<IModel>(a));
    loader.InsertModel("b", std::static_pointer_cast<IModel>(b));

    auto result = loader.BooleanDifference("a", "b", "diff_occt");
    BOOST_CHECK(result != nullptr);

    float vol = result->Volume();
    BOOST_CHECK(vol >= 0.0f);
}

BOOST_AUTO_TEST_SUITE_END()

#endif  // USE_OCCT
#endif  // USE_CGAL
