/**
 * @brief Triangle export through the C ABI: the query, its failure modes and the single-block
 * memory contract it shares with the layer geometry export.
 *
 * HsBaGetModelTriangles is what a non-C++ caller reaches a model's raw geometry with, so these
 * cases pin what that caller depends on: an unknown name or NULL argument answers 0 and leaves
 * the struct zeroed, the exported arrays describe the live model (a transform is visible in the
 * next export), index values stay inside the vertex section, and one HsBaFreeModelTriangles per
 * successful Get frees the whole block - repeating it or passing a zeroed struct is a no-op.
 *
 * The model is an ASCII STL cube written by the test itself: mesh loading needs neither the CAD
 * kernel nor a sample file on disk, so the case registers whenever DllHsBaSlicer does.
 * @author HsBa
 */
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>

#define BOOST_TEST_MODULE GuiApiTriangleExportTest
#include <boost/test/included/unit_test.hpp>

#include "DllHsBaSlicer/model_preprocess.h"

namespace
{
constexpr float kExtent = 2.0F;  // The cube spans [0, kExtent]^3.

/** @brief One closed cube as twelve ASCII STL facets, corners shared between faces. */
std::string MakeBoxStl()
{
    static const float verts[8][3] = {
        {0.0F, 0.0F, 0.0F}, {kExtent, 0.0F, 0.0F}, {kExtent, kExtent, 0.0F}, {0.0F, kExtent, 0.0F},
        {0.0F, 0.0F, kExtent}, {kExtent, 0.0F, kExtent}, {kExtent, kExtent, kExtent}, {0.0F, kExtent, kExtent},
    };
    static const int tris[12][3] = {
        {0, 3, 2}, {0, 2, 1}, {4, 5, 6}, {4, 6, 7}, {0, 1, 5}, {0, 5, 4},
        {2, 3, 7}, {2, 7, 6}, {0, 4, 7}, {0, 7, 3}, {1, 2, 6}, {1, 6, 5},
    };
    std::string text = "solid box\n";
    for (const auto& tri : tris)
    {
        text += "  facet normal 0.0 0.0 1.0\n    outer loop\n";
        for (const int index : tri)
        {
            text += "      vertex " + std::to_string(verts[index][0]) + " " + std::to_string(verts[index][1])
                  + " " + std::to_string(verts[index][2]) + "\n";
        }
        text += "    endloop\n  endfacet\n";
    }
    text += "endsolid box\n";
    return text;
}

/** @brief A cube STL in the temp directory, removed when the case ends. */
class TempBoxStl
{
public:
    explicit TempBoxStl(const char* tag)
        : path_(std::filesystem::temp_directory_path() /
                (std::string("hsba_triangle_cabi_") + tag + ".stl"))
    {
        std::ofstream output(path_);
        output << MakeBoxStl();
    }
    ~TempBoxStl()
    {
        std::filesystem::remove(path_);
    }
    TempBoxStl(const TempBoxStl&) = delete;
    TempBoxStl& operator=(const TempBoxStl&) = delete;

    std::string string() const { return path_.string(); }

private:
    std::filesystem::path path_;
};

/**
 * @brief A model loaded through the C ABI, handle released and name dropped from the pool
 *        when the case ends, so a mid-case failure cannot poison the next case's name space.
 */
class LoadedModel
{
public:
    LoadedModel(const std::string& name, const TempBoxStl& file) : name_(name)
    {
        handle_ = HsBaLoadModel(name_.c_str(), file.string().c_str());
    }
    ~LoadedModel()
    {
        if (handle_ != nullptr)
        {
            HsBaReleaseModelHandle(handle_);
        }
        HsBaRemoveModel(name_.c_str());
    }
    LoadedModel(const LoadedModel&) = delete;
    LoadedModel& operator=(const LoadedModel&) = delete;

    void RequireLoaded() const { BOOST_TEST_REQUIRE(handle_ != nullptr); }

private:
    std::string name_;
    void* handle_ = nullptr;
};

/** @brief Bounds of the exported vertex section, read back through the C ABI. */
struct Extent
{
    float min[3] = {std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
                    std::numeric_limits<float>::max()};
    float max[3] = {std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(),
                    std::numeric_limits<float>::lowest()};
};

Extent BoundsOf(const HsBaModelTriangles_t& tris)
{
    Extent bounds;
    for (int i = 0; i < tris.vertex_count; ++i)
    {
        for (int axis = 0; axis < 3; ++axis)
        {
            const float value = tris.xyz[3 * i + axis];
            bounds.min[axis] = value < bounds.min[axis] ? value : bounds.min[axis];
            bounds.max[axis] = value > bounds.max[axis] ? value : bounds.max[axis];
        }
    }
    return bounds;
}

}  // namespace

BOOST_AUTO_TEST_SUITE(triangle_export_cabi)

BOOST_AUTO_TEST_CASE(null_arguments_answer_zero)
{
    HsBaModelTriangles_t tris = {};
    BOOST_CHECK_EQUAL(HsBaGetModelTriangles(nullptr, &tris), 0);
    BOOST_CHECK_EQUAL(HsBaGetModelTriangles("whatever", nullptr), 0);
    // Freeing a NULL struct or a never-filled one must not crash either.
    HsBaFreeModelTriangles(nullptr);
    HsBaFreeModelTriangles(&tris);
}

BOOST_AUTO_TEST_CASE(unknown_model_leaves_the_struct_zeroed)
{
    HsBaModelTriangles_t tris = {};
    BOOST_CHECK_EQUAL(HsBaGetModelTriangles("triangle_export_absent_name", &tris), 0);
    BOOST_CHECK_EQUAL(tris.vertex_count, 0);
    BOOST_CHECK(tris.xyz == nullptr);
    BOOST_CHECK_EQUAL(tris.face_count, 0);
    BOOST_CHECK(tris.indices == nullptr);
}

BOOST_AUTO_TEST_CASE(cube_exports_twelve_valid_triangles)
{
    const TempBoxStl file("cube");
    const LoadedModel box("cabi_tri_cube", file);
    box.RequireLoaded();

    HsBaModelTriangles_t tris = {};
    BOOST_REQUIRE_EQUAL(HsBaGetModelTriangles("cabi_tri_cube", &tris), 1);
    BOOST_CHECK_EQUAL(tris.face_count, 12);
    BOOST_CHECK(tris.vertex_count >= 8);  // shared corners may be merged, not split
    BOOST_TEST_REQUIRE(tris.xyz != nullptr);
    BOOST_TEST_REQUIRE(tris.indices != nullptr);

    // Every index addresses a stored vertex and every facet keeps three distinct corners.
    for (int f = 0; f < tris.face_count; ++f)
    {
        const int a = tris.indices[3 * f + 0];
        const int b = tris.indices[3 * f + 1];
        const int c = tris.indices[3 * f + 2];
        BOOST_CHECK(a >= 0 && a < tris.vertex_count);
        BOOST_CHECK(b >= 0 && b < tris.vertex_count);
        BOOST_CHECK(c >= 0 && c < tris.vertex_count);
        BOOST_CHECK(a != b && b != c && a != c);
    }

    const Extent bounds = BoundsOf(tris);
    for (int axis = 0; axis < 3; ++axis)
    {
        BOOST_CHECK_CLOSE_FRACTION(bounds.min[axis], 0.0F, 1e-4F);
        BOOST_CHECK_CLOSE_FRACTION(bounds.max[axis], kExtent, 1e-4F);
    }

    // The struct is zeroed after the single free, so a second call is the documented no-op.
    HsBaFreeModelTriangles(&tris);
    BOOST_CHECK_EQUAL(tris.vertex_count, 0);
    BOOST_CHECK(tris.xyz == nullptr);
    HsBaFreeModelTriangles(&tris);
}

BOOST_AUTO_TEST_CASE(export_reflects_a_transform_applied_after_loading)
{
    const TempBoxStl file("scaled");
    const LoadedModel box("cabi_tri_scaled", file);
    box.RequireLoaded();

    BOOST_REQUIRE_EQUAL(HsBaScaleModelUniform("cabi_tri_scaled", 2.0F), 1);

    HsBaModelTriangles_t tris = {};
    BOOST_REQUIRE_EQUAL(HsBaGetModelTriangles("cabi_tri_scaled", &tris), 1);
    const Extent bounds = BoundsOf(tris);
    for (int axis = 0; axis < 3; ++axis)
    {
        BOOST_CHECK_CLOSE_FRACTION(bounds.max[axis], 2.0F * kExtent, 1e-4F);
    }
    HsBaFreeModelTriangles(&tris);
}

BOOST_AUTO_TEST_SUITE_END()
