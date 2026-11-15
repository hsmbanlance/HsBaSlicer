/**
 * @brief Text CSG through the C ABI: the capability probe, the script entry and the reason it reports.
 *
 * DllHsBaSlicer/model_preprocess.h is what a non-C++ caller reaches this feature with, and it differs
 * from every other entry in that file by taking geometry as text. Text fails, so the interface has to
 * say why: these cases pin that the reason arrives, that it stays empty when the call worked, and that
 * a refused call leaves the pool exactly as it was. The last case ends the chain the feature exists for
 * - a model that never touched a file going into a real slice.
 *
 * Registered only when the CAD kernel target exists, the same gate occt_model_test uses; text CSG is
 * then guaranteed by HSBA_REQUIRE_TEXT_CSG (ON by default), so the probe is asserted rather than
 * branched on.
 * @author HsBa
 */
#include <cstring>
#include <string>
#include <thread>

#define BOOST_TEST_MODULE GuiApiCsgCabiTest
#include <boost/test/included/unit_test.hpp>

#include "DllHsBaSlicer/fdm_pipeline.h"
#include "DllHsBaSlicer/model_preprocess.h"
#include "pipelinetypes/pipeline_types.h"

namespace
{
constexpr int kReasonCap = 512;
constexpr float kPi = 3.14159265358979f;

/** @brief Bounding box and volume of a pooled model, read back through the C ABI. */
struct Stats
{
    float volume = 0.0F;
    float min[3] = {0.0F, 0.0F, 0.0F};
    float max[3] = {0.0F, 0.0F, 0.0F};
};

Stats InfoOf(const char* name)
{
    Stats stats;
    BOOST_TEST_REQUIRE(HsBaGetModelInfo(name, stats.min, stats.max, &stats.volume) == 1);
    return stats;
}

/**
 * @brief One CSG build that undoes itself: the handle is released and the name dropped from the pool.
 *
 * A case that fails in the middle must not leave the name behind - the pool is what tells a duplicate
 * build apart from a successful one, so a leaked name would break the next case as well.
 */
class BuiltModel
{
public:
    BuiltModel(const BuiltModel&) = delete;
    BuiltModel& operator=(const BuiltModel&) = delete;

    BuiltModel(const char* name, const char* script, const char* shape_name = nullptr) : name_(name)
    {
        char reason[kReasonCap] = {};
        handle_ = HsBaBuildCsgModel(name_, script, shape_name, reason, kReasonCap);
        reason_.assign(reason);
    }

    ~BuiltModel()
    {
        if (handle_ != nullptr)
        {
            HsBaReleaseModelHandle(handle_);
        }
        HsBaRemoveModel(name_);
    }

    /// @brief Require the build to have worked, then hand back the stats the pool now holds.
    Stats RequireBuilt() const
    {
        BOOST_REQUIRE_MESSAGE(handle_ != nullptr, "build failed: " << reason_);
        BOOST_REQUIRE_MESSAGE(reason_.empty(), "a successful build left a reason: " << reason_);
        return InfoOf(name_);
    }

    bool built() const { return handle_ != nullptr; }
    const std::string& reason() const { return reason_; }

private:
    const char* name_ = "";
    void* handle_ = nullptr;
    std::string reason_;
};
}  // namespace

BOOST_AUTO_TEST_CASE(capability_probe_reports_this_build)
{
    BOOST_CHECK_EQUAL(HsBaTextCsgAvailable(), 1);
}

BOOST_AUTO_TEST_CASE(box_volume_and_corner_bbox)
{
    // DRAW's box takes the edge lengths and anchors the far corner at the origin
    const Stats stats = BuiltModel("cabi_plate", "box result 60 40 12").RequireBuilt();
    BOOST_CHECK_CLOSE(stats.volume, 60.0F * 40.0F * 12.0F, 1e-3f);
    for (int axis = 0; axis < 3; ++axis)
    {
        BOOST_CHECK_SMALL(stats.min[axis], 1e-4f);
    }
    BOOST_CHECK_CLOSE(stats.max[0], 60.0F, 1e-3f);
    BOOST_CHECK_CLOSE(stats.max[1], 40.0F, 1e-3f);
    BOOST_CHECK_CLOSE(stats.max[2], 12.0F, 1e-3f);
}

BOOST_AUTO_TEST_CASE(tcl_parameter_and_named_shape_variable)
{
    // shape_name picks a variable out of the script instead of the default "result", so an
    // intermediate shape can be the model while the script also builds others.
    const Stats stats = BuiltModel("cabi_slab", "set t 18\nbox slab 30 30 $t\n", "slab").RequireBuilt();
    BOOST_CHECK_CLOSE(stats.volume, 30.0F * 30.0F * 18.0F, 1e-3f);
    BOOST_CHECK_CLOSE(stats.max[2], 18.0F, 1e-3f);
}

BOOST_AUTO_TEST_CASE(fuse_and_cut_match_closed_form_volumes)
{
    // The boss sits on the base and shares no volume with it: union = base + boss.
    const Stats fused = BuiltModel("cabi_bracket",
                                   "set t 3\nbox base 60 40 $t\npcylinder boss 8 12\nttranslate boss 30 20 $t\n"
                                   "bfuse result base boss\n")
                            .RequireBuilt();
    BOOST_CHECK_CLOSE(fused.volume, 60.0F * 40.0F * 3.0F + kPi * 8.0F * 8.0F * 12.0F, 1e-2f);
    BOOST_CHECK_CLOSE(fused.max[2], 15.0F, 1e-2f);

    // The pin runs through the block, so the cut removes a full 12 mm of it.
    const Stats cut = BuiltModel("cabi_pad",
                                 "box block 60 40 12\npcylinder pin 5 20\nttranslate pin 30 20 -4\n"
                                 "bcut result block pin\n")
                          .RequireBuilt();
    BOOST_CHECK_CLOSE(cut.volume, 60.0F * 40.0F * 12.0F - kPi * 5.0F * 5.0F * 12.0F, 1e-2f);
}

BOOST_AUTO_TEST_CASE(reason_is_filled_on_failure_and_cleared_on_success)
{
    char reason[kReasonCap] = {};
    std::memset(reason, 'X', sizeof(reason) - 1);

    // A malformed command: DRAW's box wants six numbers.
    BOOST_CHECK(HsBaBuildCsgModel("cabi_bad", "box result 60", nullptr, reason, kReasonCap) == nullptr);
    BOOST_CHECK_MESSAGE(std::strlen(reason) > 0u, "a refused script left no reason to show");

    std::memset(reason, 'X', sizeof(reason) - 1);
    // Nothing wrong with the script, but the caller asks for a variable it never fills.
    BOOST_CHECK(HsBaBuildCsgModel("cabi_ghost", "box built 1 1 1", "neverAssigned", reason, kReasonCap) ==
                nullptr);
    BOOST_CHECK_MESSAGE(std::strlen(reason) > 0u, "a missing result variable left no reason");

    std::memset(reason, 'X', sizeof(reason) - 1);
    void* handle = HsBaBuildCsgModel("cabi_clean", "box result 2 2 2", nullptr, reason, kReasonCap);
    BOOST_REQUIRE(handle != nullptr);
    BOOST_CHECK_MESSAGE(reason[0] == '\0', "the success path must clear the buffer, got: " << reason);
    HsBaReleaseModelHandle(handle);
    HsBaRemoveModel("cabi_clean");

    // A short buffer has to truncate, not run past the caller's memory or forget the terminator.
    char tiny[8] = {};
    BOOST_CHECK(HsBaBuildCsgModel("cabi_tiny", "box result", nullptr, tiny, static_cast<int>(sizeof(tiny))) ==
                nullptr);
    BOOST_CHECK_MESSAGE(std::strlen(tiny) < sizeof(tiny), "the reason overran the caller's buffer");
    BOOST_CHECK_EQUAL(tiny[sizeof(tiny) - 1], '\0');

    // Declining to see a reason at all must not crash the call.
    void* ignored = HsBaBuildCsgModel("cabi_noreason", "box result 1 1 1", nullptr, nullptr, 0);
    BOOST_REQUIRE(ignored != nullptr);
    HsBaReleaseModelHandle(ignored);
    HsBaRemoveModel("cabi_noreason");
}

BOOST_AUTO_TEST_CASE(null_arguments_are_refused_with_a_reason)
{
    char reason[kReasonCap] = {};
    BOOST_CHECK(HsBaBuildCsgModel(nullptr, "box n 1 1 1", nullptr, reason, kReasonCap) == nullptr);
    BOOST_CHECK_MESSAGE(std::strlen(reason) > 0u, "NULL name reported no reason");

    std::memset(reason, 'X', sizeof(reason) - 1);
    BOOST_CHECK(HsBaBuildCsgModel("cabi_null_script", nullptr, nullptr, reason, kReasonCap) == nullptr);
    BOOST_CHECK_MESSAGE(std::strlen(reason) > 0u, "NULL script reported no reason");
}

BOOST_AUTO_TEST_CASE(duplicate_name_is_refused_and_keeps_the_first_shape)
{
    BuiltModel first("cabi_taken", "box result 1 1 1");
    BOOST_CHECK_CLOSE(first.RequireBuilt().volume, 1.0F, 1e-3f);

    // Same name, different geometry: refusing is the only safe answer, replacing would drop a model
    // another caller still holds a handle to.
    BuiltModel again("cabi_taken", "box result 8 8 8");
    BOOST_CHECK_MESSAGE(!again.built(), "a duplicate name was accepted: " << again.reason());
    BOOST_CHECK_MESSAGE(again.reason().size() > 0u, "the refusal left no reason");
    BOOST_CHECK_CLOSE(InfoOf("cabi_taken").volume, 1.0F, 1e-3f);
}

BOOST_AUTO_TEST_CASE(handle_release_keeps_the_pooled_model)
{
    char reason[kReasonCap] = {};
    void* handle = HsBaBuildCsgModel("cabi_pool", "box result 4 4 4", nullptr, reason, kReasonCap);
    BOOST_REQUIRE(handle != nullptr);
    BOOST_CHECK_EQUAL(HsBaContainsModel("cabi_pool"), 1);

    // The handle is one reference; the pool keeps its own until the name is removed.
    HsBaReleaseModelHandle(handle);
    BOOST_CHECK_EQUAL(HsBaContainsModel("cabi_pool"), 1);
    BOOST_CHECK_CLOSE(InfoOf("cabi_pool").volume, 64.0F, 1e-3f);

    HsBaRemoveModel("cabi_pool");
    BOOST_CHECK_EQUAL(HsBaContainsModel("cabi_pool"), 0);
    BOOST_CHECK(HsBaGetModel("cabi_pool") == nullptr);

    float min[3] = {}, max[3] = {}, volume = 0.0F;
    BOOST_CHECK_EQUAL(HsBaGetModelInfo("cabi_pool", min, max, &volume), 0);
}

BOOST_AUTO_TEST_CASE(published_model_resolves_from_another_thread)
{
    const BuiltModel body("cabi_publish", "box result 2 1 1");
    body.RequireBuilt();

    // The pool is thread local, so a name the pipeline has to reach starts out invisible elsewhere.
    std::thread other([]
                     { BOOST_CHECK_EQUAL(HsBaContainsModel("cabi_publish"), 0); });
    other.join();

    BOOST_CHECK_EQUAL(HsBaPublishModel("cabi_publish"), 1);
    std::thread published([]
                          {
                              BOOST_CHECK_EQUAL(HsBaContainsModel("cabi_publish"), 1);
                              float min[3] = {}, max[3] = {}, volume = 0.0F;
                              BOOST_REQUIRE(HsBaGetModelInfo("cabi_publish", min, max, &volume) == 1);
                              BOOST_CHECK_CLOSE(volume, 2.0F, 1e-3f);
                          });
    published.join();

    // Unpublishing only drops the process-wide copy; the builder's thread still resolves the name.
    BOOST_CHECK_EQUAL(HsBaUnpublishModel("cabi_publish"), 1);
    BOOST_CHECK_EQUAL(HsBaPublishModel("cabi_absent"), 0);
    BOOST_CHECK_EQUAL(HsBaUnpublishModel("cabi_absent"), 0);
    BOOST_CHECK_EQUAL(HsBaContainsModel("cabi_publish"), 1);
}

BOOST_AUTO_TEST_CASE(csg_model_slices_without_a_model_file)
{
    // A cylinder 25 mm tall at 0.2 mm layers: the slice has to reach the model built from text,
    // because nothing on disk could stand in for it.
    const BuiltModel body("cabi_vase", "pcylinder body 25 20\n", "body");
    body.RequireBuilt();

    // Pipeline stages run on executor threads, where the caller's thread-local pool is invisible;
    // publishing is what hands the name over to GetModel inside the coroutine.
    BOOST_CHECK_EQUAL(HsBaPublishModel("cabi_vase"), 1);

    HsBaFdmPipelineConfig_t cfg = HsBaCreateDefaultConfig();
    cfg.model_name = "cabi_vase";
    cfg.model_path = nullptr;  // the whole point: no file behind this model
    cfg.layer_height = 0.2F;
    cfg.first_layer_height = 0.2F;
    cfg.enable_support = 0;

    HsBaFdmPipelineResult_t result = HsBaRunFdmPipeline(&cfg, nullptr, nullptr);
    BOOST_REQUIRE_MESSAGE(result.success == 1,
                          "slicing a CSG model failed: " << (result.error_message ? result.error_message : ""));
    BOOST_CHECK_EQUAL(result.total_layers, 100);
    BOOST_REQUIRE(result.gcode_content != nullptr);
    BOOST_CHECK_MESSAGE(std::strstr(result.gcode_content, "G1 ") != nullptr, "no moves in the G-code");

    HsBaFreePipelineResult(&result);
    BOOST_CHECK_EQUAL(HsBaUnpublishModel("cabi_vase"), 1);
}
