/**
 * @file hsba_slicer_module_test.cpp
 * @brief Smoke tests for the exported C++20 module `hsba.slicer`, covering the four
 *        capability groups the module was extended with to become a C ABI superset.
 *
 * Unlike the GuiApi tests, which reach the pipeline through the DllHsBaSlicer C ABI, these
 * cases drive the native C++ class API directly (`import hsba.slicer;`) and exercise the
 * module-side additions only:
 *   1. Config JSON codec      - defaultConfigJson / configToJson / configFromJson round-trip.
 *   2. Advanced model ops     - pool queries, triangle export, fromExisting, publish.
 *   3. Native async + cancel  - Job<FdmResult> state machine, cooperative cancel.
 *   4. Geometry / spiral       - run() Geometry capture invariants and spiralPath().
 *
 * Every case feeds itself an ASCII STL cube written to the temp directory, so mesh loading is
 * the only capability required: no sample model file and no CAD kernel are needed, mirroring
 * GuiApi/triangle_export_cabi_test.cpp. CSG cases are gated at run time by hasTextCsgSupport().
 * @author HsBa
 */

// Standard library includes MUST precede `import` in MSVC C++20 modules, otherwise the BMI's
// std declarations conflict with re-included headers (C2572).
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

// GMF headers: types the module deliberately does not re-export (C++20 module limitation) must
// be named through their original headers, exactly like samples/LibHsBaSlicer/main_module.cpp.
#include <Eigen/Core>
#include "pipelinetypes/pipeline_types.h"
#include "LibHsBaSlicer/Path/spiral_path.hpp"

#define BOOST_TEST_MODULE HsBaSlicerModuleTest
#include <boost/test/included/unit_test.hpp>

import hsba.slicer;

using namespace HsBa::Slicer;

namespace
{
constexpr float kExtent = 2.0F;  // The cube spans [0, kExtent]^3.

/** @brief One closed cube as twelve ASCII STL facets (shared corners). */
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
            text += "      vertex " + std::to_string(verts[index][0]) + " " + std::to_string(verts[index][1]) + " "
                  + std::to_string(verts[index][2]) + "\n";
        }
        text += "    endloop\n  endfacet\n";
    }
    text += "endsolid box\n";
    return text;
}

/** @brief A cube STL in the temp directory, removed when the case ends. */
class TempCubeStl
{
public:
    explicit TempCubeStl(const char* tag)
        : path_(std::filesystem::temp_directory_path() / (std::string("hsba_module_") + tag + ".stl"))
    {
        std::ofstream output(path_);
        output << MakeBoxStl();
    }
    ~TempCubeStl() { std::filesystem::remove(path_); }
    TempCubeStl(const TempCubeStl&) = delete;
    TempCubeStl& operator=(const TempCubeStl&) = delete;

    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

}  // namespace

// ---------------------------------------------------------------------------
// 1. Config JSON codec
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_SUITE(module_json_codec)

BOOST_AUTO_TEST_CASE(default_and_roundtrip)
{
    const std::string def = defaultConfigJson(ParamStoreKind::Fdm);
    BOOST_CHECK(!def.empty());
    BOOST_CHECK(def.find("layer_height") != std::string::npos);

    HsBaFdmPipelineConfig_t cfg = defaultFdmConfig();
    cfg.layer_height = 0.42F;
    cfg.wall_count = 7;

    const std::string json = configToJson(ParamStoreKind::Fdm, &cfg);
    BOOST_CHECK(!json.empty());

    // configFromJson lands into a default-initialized struct; only fields present in the
    // JSON are overwritten, so the two edited scalars must come back identical.
    HsBaFdmPipelineConfig_t back = defaultFdmConfig();
    BOOST_CHECK(configFromJson(ParamStoreKind::Fdm, json, &back));
    BOOST_CHECK_CLOSE(back.layer_height, 0.42F, 1e-3);
    BOOST_CHECK_EQUAL(back.wall_count, 7);

    // configFromJson malloc-owns any non-null const char* fields it produced. freeLoaded()
    // only forwards to FreeLoadedConfigStrings (no DB access), so a dummy connection is fine.
    const ParamStoreConnection dummy;
    ParamStorePipeline store(dummy);
    store.freeLoaded(ParamStoreKind::Fdm, &back);
}

BOOST_AUTO_TEST_SUITE_END()

// ---------------------------------------------------------------------------
// 2. Advanced model operations
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_SUITE(module_model_ops)

BOOST_AUTO_TEST_CASE(pool_queries_and_triangles)
{
    const TempCubeStl cube("pool");
    Model m("module_cube", cube.path());

    BOOST_CHECK_EQUAL(m.name(), "module_cube");
    BOOST_CHECK(Model::exists("module_cube"));
    BOOST_CHECK(!Model::exists("module_absent_name"));
    BOOST_CHECK_GE(Model::count(), 1U);

    const std::vector<std::string> names = Model::names();
    BOOST_CHECK(std::find(names.begin(), names.end(), "module_cube") != names.end());

    const auto [verts, faces] = m.triangles();
    BOOST_CHECK_GE(verts.rows(), 8);    // shared corners may merge, never split below 8
    BOOST_CHECK_EQUAL(faces.rows(), 12);

    // fromExisting adopts the already-loaded model without touching the file again.
    Model adopted = Model::fromExisting("module_cube");
    BOOST_CHECK_EQUAL(adopted.name(), "module_cube");

    // publish/unpublish round-trips the process-wide name reference; the exact effect on the
    // other pool is a C ABI concern, here we only check the calls succeed on a loaded model.
    BOOST_CHECK(m.publish());
    BOOST_CHECK(m.unpublish());

    // CSG capability is build dependent: only assert the probe returns without throwing.
    BOOST_CHECK_NO_THROW((void)hasTextCsgSupport());
}

BOOST_AUTO_TEST_SUITE_END()

// ---------------------------------------------------------------------------
// 3. Native async job + cooperative cancellation
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_SUITE(module_async_job)

BOOST_AUTO_TEST_CASE(run_async_completes)
{
    const TempCubeStl cube("async_ok");
    Model m("module_async_ok", cube.path());

    HsBaFdmPipelineConfig_t cfg = defaultFdmConfig();
    cfg.layer_height = 0.3F;
    cfg.first_layer_height = 0.3F;
    const FdmPipeline pipeline(cfg);

    int last_percent = -1;
    Job<FdmResult> job =
        pipeline.runAsync(m, [&last_percent](int percent, std::string_view) { last_percent = percent; });

    job.wait(std::chrono::milliseconds(30000));
    BOOST_CHECK(job.done());
    BOOST_CHECK(job.state() == JobState::Succeeded);
    BOOST_CHECK_GE(job.progress(), 0);

    const FdmResult result = job.get();
    BOOST_CHECK_GT(result.total_layers, 0);
    BOOST_CHECK(!result.gcode.empty());
}

BOOST_AUTO_TEST_CASE(run_async_cancel_is_terminal)
{
    const TempCubeStl cube("async_cancel");
    Model m("module_async_cancel", cube.path());

    // Thin layers give the kernel many checkpoints so a cooperative cancel can land mid-run.
    HsBaFdmPipelineConfig_t cfg = defaultFdmConfig();
    cfg.layer_height = 0.05F;
    cfg.first_layer_height = 0.05F;
    const FdmPipeline pipeline(cfg);

    Job<FdmResult> job = pipeline.runAsync(m);
    job.cancel();  // request immediately; the kernel honours it at the next checkpoint

    BOOST_CHECK(job.wait(std::chrono::milliseconds(30000)));
    const JobState state = job.state();
    // Cancellation is cooperative: if the tiny model finished before the flag was observed the
    // job legitimately succeeded. Either way it must reach a terminal state without hanging.
    BOOST_CHECK(state == JobState::Cancelled || state == JobState::Succeeded || state == JobState::Failed);
    if (state == JobState::Cancelled)
        BOOST_CHECK_THROW(job.get(), SlicerError);
}

BOOST_AUTO_TEST_SUITE_END()

// ---------------------------------------------------------------------------
// 4. Geometry capture + spiral path
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_SUITE(module_geometry)

BOOST_AUTO_TEST_CASE(run_captures_aligned_geometry)
{
    const TempCubeStl cube("geom");
    Model m("module_geom", cube.path());

    HsBaFdmPipelineConfig_t cfg = defaultFdmConfig();
    cfg.layer_height = 0.3F;
    cfg.first_layer_height = 0.3F;
    const FdmPipeline pipeline(cfg);

    const FdmResult result = pipeline.run(m);
    BOOST_CHECK_GT(result.total_layers, 0);

    // run() forces capture_geometry, so the module result must carry per-layer arrays whose
    // lengths equal the layer count (the kernel's GeometryCapture invariant).
    BOOST_CHECK(result.geometry.valid);
    BOOST_CHECK_EQUAL(result.geometry.zs.size(), static_cast<std::size_t>(result.total_layers));
    BOOST_CHECK_EQUAL(result.geometry.outlines.size(), result.geometry.zs.size());
    BOOST_CHECK_EQUAL(result.geometry.fills.size(), result.geometry.zs.size());
    // zs must be monotonically non-decreasing (layer platform heights).
    for (std::size_t i = 1; i < result.geometry.zs.size(); ++i)
        BOOST_CHECK_GE(result.geometry.zs[i], result.geometry.zs[i - 1]);
}

BOOST_AUTO_TEST_CASE(spiral_path_z_nondecreasing)
{
    const TempCubeStl cube("spiral");
    Model m("module_spiral", cube.path());

    HsBaFdmPipelineConfig_t cfg = defaultFdmConfig();
    cfg.layer_height = 0.3F;
    cfg.first_layer_height = 0.3F;
    const FdmPipeline pipeline(cfg);

    const std::vector<SpiralPoint> path = pipeline.spiralPath(m);
    BOOST_CHECK(!path.empty());
    for (std::size_t i = 1; i < path.size(); ++i)
        BOOST_CHECK_GE(path[i].z, path[i - 1].z);
}

BOOST_AUTO_TEST_SUITE_END()
