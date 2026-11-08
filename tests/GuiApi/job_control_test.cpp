/** @file job_control_test.cpp
 * @brief Job handle contract of DllHsBaSlicer/job_control.h (submit, cancel, wait, result view).
 *
 * Every fixture is built from a run that ends on its own after one progress tick: a
 * missing model file. That keeps the suite free of meshes and remote services while
 * still covering the whole state machine (pending, running, cancelled, failed) and the
 * threading contract, because the progress callback proves which thread it runs on.
 * @author HsBa
 */
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdlib>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

#define BOOST_TEST_MODULE GuiApiJobControlTest
#include <boost/test/included/unit_test.hpp>

#include "DllHsBaSlicer/file_transfer_pipeline.h"
#include "DllHsBaSlicer/geometry_export.h"
#include "DllHsBaSlicer/job_control.h"
#include "DllHsBaSlicer/progress_stage.h"
#include "cipher/encoder.hpp"
#include "pipelinetypes/pipeline_types.h"

namespace
{
/** @brief Upper bound for a run that only has to fail on a missing file. */
constexpr long long kJobTimeoutMs = 60000;

/** @brief FDM configuration whose model path does not exist. */
HsBaFdmPipelineConfig_t MissingModelConfig()
{
    HsBaFdmPipelineConfig_t cfg = HsBaFdmConfigDefault();
    cfg.model_name = "gui-job-control-missing";
    cfg.model_path = "hsba-gui-job-control-missing-model.stl";
    return cfg;
}

/**
 * @brief Mesh a full slice can be run on, or an empty string when the caller did not
 *        provide one (HSBA_SAMPLE_MODEL), which is how the real-geometry cases opt out.
 */
std::string SampleModelPath()
{
#ifdef _WIN32
    char* buffer = nullptr;
    std::size_t buffer_size = 0;
    _dupenv_s(&buffer, &buffer_size, "HSBA_SAMPLE_MODEL");
    const std::string model_path = buffer ? std::string(buffer) : std::string();
    std::free(buffer);
#else
    const char* raw_model = std::getenv("HSBA_SAMPLE_MODEL");
    const std::string model_path = raw_model ? std::string(raw_model) : std::string();
#endif
    return model_path;
}

/**
 * @brief Handshake that lets the submitting thread park the pipeline inside its callback.
 *
 * The FDM pipeline reports "Loading model..." before it touches the file system, so the
 * first tick is a guaranteed safe point at which the job is RUNNING and not yet final.
 */
struct ProgressGate
{
    std::mutex mutex;
    std::condition_variable cv;
    bool ticked = false;    ///< Set by the job thread once it entered the callback
    bool released = false;  ///< Set by the submitting thread to let the job continue
};

/** @brief Progress callback: announces the first tick, then waits for the host. */
void HoldOnFirstTick(int, HsBaStage_t, const char*, void* user_data)
{
    auto* gate = static_cast<ProgressGate*>(user_data);
    {
        const std::lock_guard<std::mutex> lock(gate->mutex);
        if (!gate->ticked)
        {
            gate->ticked = true;
            gate->cv.notify_all();
        }
    }
    std::unique_lock<std::mutex> lock(gate->mutex);
    gate->cv.wait(lock, [gate]() { return gate->released; });
}

/** @brief Let a gated job run to its end. */
void ReleaseGate(ProgressGate& gate)
{
    {
        const std::lock_guard<std::mutex> lock(gate.mutex);
        gate.released = true;
    }
    gate.cv.notify_all();
}

/** @brief Records where the completion callback fired. */
struct DoneRecord
{
    std::atomic<bool> called{false};
    std::atomic<int> state_at_done{HSBA_JOB_PENDING};
    std::thread::id thread_id{};
};

/** @brief Completion callback: must run on the job thread, after the state is final. */
void OnJobDone(HsBaJobHandle job, void* user_data)
{
    auto* record = static_cast<DoneRecord*>(user_data);
    record->thread_id = std::this_thread::get_id();
    record->state_at_done = static_cast<int>(HsBaJobGetState(job));
    record->called.store(true);
}

/** @brief Whether a message starts with the cancellation prefix the C ABI publishes. */
bool CancelledText(const char* message)
{
    return message && std::string(message).rfind("Cancelled", 0) == 0;
}

/**
 * @brief Base64 text of one thumbnail block with its comment lines rejoined, empty when the
 *        model view was never written or came out as a skip notice.
 *
 * This is the reading a G-code consumer has to do: gather the lines between the begin and end
 * markers of a named image and drop the comment prefix from each of them.
 */
std::string ThumbnailBase64(const std::string& gcode, const std::string& name)
{
    const std::size_t start = gcode.find("; thumbnail begin name=" + name);
    if (start == std::string::npos)
    {
        return {};
    }
    const std::size_t block_end = gcode.find("; thumbnail end name=" + name, start);
    if (block_end == std::string::npos)
    {
        BOOST_ERROR("thumbnail block of " + name + " is never closed");
        return {};
    }
    std::string joined;
    std::size_t pos = gcode.find('\n', start) + 1;  // behind the metadata line
    while (pos < block_end)
    {
        const std::size_t line_end = gcode.find('\n', pos);
        if (line_end == std::string::npos || line_end > block_end)
        {
            break;
        }
        const std::string_view line(gcode.data() + pos, line_end - pos);
        BOOST_CHECK_EQUAL(line.substr(0, 2), "; ");  // every payload line stays a comment
        joined.append(line.substr(2));
        pos = line_end + 1;
    }
    return joined;
}
}  // namespace

BOOST_AUTO_TEST_SUITE(gui_job_control)

BOOST_AUTO_TEST_CASE(invalid_submissions_and_null_handles_are_rejected)
{
    const HsBaFdmPipelineConfig_t cfg = MissingModelConfig();
    // A submit either hands back a usable handle or NULL; it never half-succeeds.
    BOOST_CHECK(HsBaSubmitJob(HSBA_PIPELINE_KIND_COUNT, &cfg, nullptr, nullptr, nullptr, nullptr) == nullptr);
    BOOST_CHECK(HsBaSubmitJob(static_cast<HsBaPipelineKind>(-1), &cfg, nullptr, nullptr, nullptr, nullptr) == nullptr);
    BOOST_CHECK(HsBaSubmitJob(HSBA_PIPELINE_FDM, nullptr, nullptr, nullptr, nullptr, nullptr) == nullptr);

    // A host that never got a handle must still be able to clean up unconditionally.
    BOOST_CHECK_EQUAL(HsBaCancelJob(nullptr), 0);
    BOOST_CHECK_EQUAL(HsBaWaitJob(nullptr, 10), 0);
    BOOST_CHECK(HsBaJobGetState(nullptr) == HSBA_JOB_FAILED);
    BOOST_CHECK_EQUAL(HsBaJobGetProgress(nullptr, nullptr, nullptr, nullptr, 0), 0);
    BOOST_CHECK_EQUAL(HsBaJobGeometryId(nullptr), 0);
    BOOST_CHECK_EQUAL(HsBaJobCopyResult(nullptr, nullptr), 0);
    HsBaJobResultView_t view{};
    BOOST_CHECK_EQUAL(HsBaJobCopyResult(nullptr, &view), 0);
    BOOST_CHECK_EQUAL(view.error_message, nullptr);
    HsBaFreeJobResultView(nullptr);
    HsBaFreeJobResultView(&view);
    HsBaReleaseJob(nullptr);
}

BOOST_AUTO_TEST_CASE(submit_blocks_no_longer_than_a_config_copy)
{
    const HsBaFdmPipelineConfig_t cfg = MissingModelConfig();
    const auto begin = std::chrono::steady_clock::now();
    HsBaJobHandle job = HsBaSubmitJob(HSBA_PIPELINE_FDM, &cfg, nullptr, nullptr, nullptr, nullptr);
    const auto elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin);
    BOOST_REQUIRE(job != nullptr);
    // The caller only paid for the deep copy and the thread start. An implementation that
    // ran the pipeline inline would have to report the failure already, which it must not.
    BOOST_CHECK(elapsed.count() < 500);

    BOOST_CHECK_EQUAL(HsBaWaitJob(job, kJobTimeoutMs), 1);
    BOOST_CHECK(HsBaJobGetState(job) == HSBA_JOB_FAILED);
    // A final job refuses cancellation, so a late button click cannot lie about the outcome.
    BOOST_CHECK_EQUAL(HsBaCancelJob(job), 0);

    HsBaJobResultView_t view{};
    BOOST_REQUIRE_EQUAL(HsBaJobCopyResult(job, &view), 1);
    BOOST_CHECK_EQUAL(view.success, 0);
    BOOST_CHECK(view.error_message != nullptr);
    BOOST_CHECK(!CancelledText(view.error_message));
    BOOST_CHECK_EQUAL(view.total_layers, 0);
    BOOST_CHECK_EQUAL(view.geometry_id, 0);
    BOOST_CHECK_EQUAL(HsBaJobGeometryId(job), 0);
    HsBaFreeJobResultView(&view);
    BOOST_CHECK_EQUAL(view.error_message, nullptr);

    // Waiting again is allowed and returns immediately.
    BOOST_CHECK_EQUAL(HsBaWaitJob(job, 0), 1);
    HsBaReleaseJob(job);
}

BOOST_AUTO_TEST_CASE(cancel_requested_while_running_ends_as_cancelled)
{
    ProgressGate gate;
    const HsBaFdmPipelineConfig_t cfg = MissingModelConfig();
    HsBaJobHandle job = HsBaSubmitJob(HSBA_PIPELINE_FDM, &cfg, HoldOnFirstTick, &gate, nullptr, nullptr);
    BOOST_REQUIRE(job != nullptr);

    {
        std::unique_lock<std::mutex> lock(gate.mutex);
        BOOST_REQUIRE(gate.cv.wait_for(lock, std::chrono::seconds(30), [&gate]() { return gate.ticked; }));
    }
    // The pipeline is still inside the callback, so the request is recorded, not rejected.
    BOOST_CHECK_EQUAL(HsBaCancelJob(job), 1);
    BOOST_CHECK(HsBaJobGetState(job) == HSBA_JOB_RUNNING);
    ReleaseGate(gate);

    BOOST_CHECK_EQUAL(HsBaWaitJob(job, kJobTimeoutMs), 1);
    BOOST_CHECK(HsBaJobGetState(job) == HSBA_JOB_CANCELLED);

    int percent = -1;
    HsBaStage_t stage = HSBA_STAGE_UNKNOWN;
    char text[64] = {};
    BOOST_REQUIRE_EQUAL(HsBaJobGetProgress(job, &percent, &stage, text, static_cast<int>(sizeof(text))), 1);
    BOOST_CHECK(stage == HSBA_STAGE_CANCELLED);
    BOOST_CHECK_EQUAL(std::string(text), "Cancelled");
    BOOST_CHECK_EQUAL(std::string(HsBaStageName(stage)), "cancelled");
    // The progress query keeps working with the optional destinations left out.
    BOOST_CHECK_EQUAL(HsBaJobGetProgress(job, nullptr, nullptr, nullptr, 0), 1);

    HsBaJobResultView_t view{};
    BOOST_REQUIRE_EQUAL(HsBaJobCopyResult(job, &view), 1);
    BOOST_CHECK_EQUAL(view.success, 0);
    HsBaFreeJobResultView(&view);
    HsBaReleaseJob(job);
}

BOOST_AUTO_TEST_CASE(completion_callback_runs_on_the_job_thread)
{
    DoneRecord record;
    const HsBaFdmPipelineConfig_t cfg = MissingModelConfig();
    HsBaJobHandle job = HsBaSubmitJob(HSBA_PIPELINE_FDM, &cfg, nullptr, nullptr, OnJobDone, &record);
    BOOST_REQUIRE(job != nullptr);
    BOOST_CHECK_EQUAL(HsBaWaitJob(job, kJobTimeoutMs), 1);

    // The completion callback fires after the final state was published, and never on the
    // thread that submitted, which is what a GUI relies on to marshal into its own loop.
    BOOST_CHECK(record.called.load());
    BOOST_CHECK_EQUAL(record.state_at_done.load(), static_cast<int>(HSBA_JOB_FAILED));
    BOOST_CHECK(record.thread_id != std::this_thread::get_id());
    HsBaReleaseJob(job);
}

BOOST_AUTO_TEST_CASE(cancel_flag_of_a_synchronous_run_is_reported_as_cancelled)
{
    // A host-owned flag is the other half of the contract: the pipeline throws at its first
    // checkpoint, so no transfer is attempted and no network configuration is needed.
    std::atomic<bool> cancel(true);
    const char* paths[] = {"hsba-gui-job-control-not-transferred.txt"};
    HsBaFileTransferPipelineConfig_t cfg = HsBaFileTransferConfigDefault();
    cfg.host = "127.0.0.1";
    cfg.port = "2121";
    cfg.file_paths = paths;
    cfg.file_count = 1;
    cfg.cancel_flag = &cancel;

    HsBaFileTransferPipelineResult_t result = HsBaRunFileTransferPipeline(&cfg, nullptr, nullptr);
    BOOST_CHECK_EQUAL(result.success, 0);
    BOOST_CHECK(CancelledText(result.error_message));
    BOOST_CHECK_EQUAL(result.total_files, 1);
    BOOST_CHECK_EQUAL(result.files_transferred, 0);
    BOOST_CHECK_EQUAL(result.geometry_id, 0);
    HsBaFreeFileTransferPipelineResult(&result);

    // The same config without a flag must fail for its own reason instead of reading as
    // cancelled, which is what keeps the "Cancelled" prefix a reliable signal for hosts.
    cfg.cancel_flag = nullptr;
    HsBaFileTransferPipelineResult_t uncancelled = HsBaRunFileTransferPipeline(&cfg, nullptr, nullptr);
    BOOST_CHECK(!CancelledText(uncancelled.error_message));
    HsBaFreeFileTransferPipelineResult(&uncancelled);
}

BOOST_AUTO_TEST_CASE(captured_geometry_is_readable_after_the_job_ended)
{
    // Needs a real mesh, so it only runs when the caller points at one.
    const std::string model_path = SampleModelPath();
    if (model_path.empty())
    {
        BOOST_TEST_MESSAGE("skip: set HSBA_SAMPLE_MODEL to a sliceable mesh to cover geometry capture");
        return;
    }

    HsBaFdmPipelineConfig_t cfg = MissingModelConfig();
    cfg.model_path = model_path.c_str();
    cfg.enable_support = 0;
    cfg.capture_geometry = 1;
    HsBaJobHandle job = HsBaSubmitJob(HSBA_PIPELINE_FDM, &cfg, nullptr, nullptr, nullptr, nullptr);
    BOOST_REQUIRE(job != nullptr);
    BOOST_CHECK_EQUAL(HsBaWaitJob(job, kJobTimeoutMs), 1);

    if (HsBaJobGetState(job) != HSBA_JOB_SUCCEEDED)
    {
        BOOST_FAIL(std::string("sample model did not slice, job state ") +
                   std::to_string(static_cast<int>(HsBaJobGetState(job))));
    }
    HsBaJobResultView_t view{};
    BOOST_REQUIRE_EQUAL(HsBaJobCopyResult(job, &view), 1);
    BOOST_CHECK_EQUAL(view.success, 1);
    BOOST_CHECK_GT(view.total_layers, 0);

    // The previews are strictly opt-in, and the default stays off: a run that never asked for a
    // picture must not grow a comment block the caller would then have to explain away.
    BOOST_REQUIRE(view.inline_content != nullptr);
    BOOST_CHECK_EQUAL(std::string(view.inline_content).find("; thumbnail"), std::string::npos);

    const long long geometry_id = HsBaJobGeometryId(job);
    BOOST_CHECK_GT(geometry_id, 0);
    BOOST_CHECK_EQUAL(view.geometry_id, geometry_id);
    BOOST_CHECK_EQUAL(HsBaGeometryLayerCount(geometry_id), view.total_layers);

    // Pin the snapshot, read the first layer, then hand both references back.
    BOOST_CHECK_EQUAL(HsBaRetainGeometrySnapshot(geometry_id), geometry_id);
    HsBaLayerGeometry_t geom{};
    BOOST_REQUIRE_EQUAL(HsBaGetLayerGeometry(geometry_id, 0, &geom), 1);
    BOOST_CHECK_EQUAL(geom.layer_index, 0);
    BOOST_CHECK_GT(geom.outline_count, 0);
    BOOST_REQUIRE(geom.outlines[0].xy);
    BOOST_CHECK_GT(geom.outlines[0].point_count, 0);
    HsBaFreeLayerGeometry(&geom);
    BOOST_CHECK_EQUAL(geom.outlines, nullptr);

    HsBaReleaseGeometrySnapshot(geometry_id);  // the host reference
    BOOST_CHECK_GT(HsBaGeometryLayerCount(geometry_id), 0);
    HsBaReleaseGeometrySnapshot(geometry_id);  // the reference the capture itself owns
    BOOST_CHECK_EQUAL(HsBaGeometryLayerCount(geometry_id), 0);

    HsBaFreeJobResultView(&view);
    HsBaReleaseJob(job);
}

BOOST_AUTO_TEST_CASE(spiral_run_captures_a_rising_path)
{
    // Same mesh requirement as the layer-capture case above.
    const std::string model_path = SampleModelPath();
    if (model_path.empty())
    {
        BOOST_TEST_MESSAGE("skip: set HSBA_SAMPLE_MODEL to a sliceable mesh to cover path capture");
        return;
    }

    HsBaFdmPipelineConfig_t cfg = MissingModelConfig();
    cfg.model_path = model_path.c_str();
    cfg.enable_support = 0;
    cfg.capture_geometry = 1;
    cfg.spiral_mode = 1;
    HsBaJobHandle job = HsBaSubmitJob(HSBA_PIPELINE_FDM, &cfg, nullptr, nullptr, nullptr, nullptr);
    BOOST_REQUIRE(job != nullptr);
    BOOST_CHECK_EQUAL(HsBaWaitJob(job, kJobTimeoutMs), 1);

    if (HsBaJobGetState(job) != HSBA_JOB_SUCCEEDED)
    {
        BOOST_FAIL(std::string("spiral run did not slice, job state ") +
                   std::to_string(static_cast<int>(HsBaJobGetState(job))));
    }
    HsBaJobResultView_t view{};
    BOOST_REQUIRE_EQUAL(HsBaJobCopyResult(job, &view), 1);
    BOOST_CHECK_EQUAL(view.success, 1);

    const long long geometry_id = HsBaJobGeometryId(job);
    BOOST_REQUIRE_GT(geometry_id, 0);
    const int layers = HsBaGeometryLayerCount(geometry_id);
    BOOST_REQUIRE_GT(layers, 0);
    // A helix visits every layer at least twice (the seam is shared with the next one),
    // so a stored path shorter than that means the capture branch never ran.
    BOOST_REQUIRE_GE(HsBaGeometryPathPointCount(geometry_id), 2 * layers);

    BOOST_CHECK_EQUAL(HsBaRetainGeometrySnapshot(geometry_id), geometry_id);
    HsBaGeometryPath_t path{};
    BOOST_REQUIRE_EQUAL(HsBaGetGeometryPath(geometry_id, &path), 1);
    BOOST_REQUIRE(path.xyz);
    BOOST_CHECK_GT(path.point_count, 0);
    // The polyline is Z-monotone by construction: it only ever climbs.
    for (int i = 1; i < path.point_count; ++i)
    {
        const double previous = path.xyz[3 * (i - 1) + 2];
        const double current = path.xyz[3 * i + 2];
        BOOST_CHECK_GE(current, previous - 1e-9);
    }
    BOOST_CHECK_GT(path.z_max, path.z_min);
    HsBaFreeGeometryPath(&path);
    BOOST_CHECK_EQUAL(path.xyz, nullptr);

    HsBaReleaseGeometrySnapshot(geometry_id);  // the host reference
    BOOST_CHECK_GT(HsBaGeometryLayerCount(geometry_id), 0);
    HsBaReleaseGeometrySnapshot(geometry_id);  // the reference the capture itself owns
    BOOST_CHECK_EQUAL(HsBaGeometryLayerCount(geometry_id), 0);

    HsBaFreeJobResultView(&view);
    HsBaReleaseJob(job);
}

BOOST_AUTO_TEST_CASE(thumbnail_switch_embeds_a_png_in_the_gcode_comments)
{
    // The previews come from a real mesh, so this case needs the same sample model as the two above.
    const std::string model_path = SampleModelPath();
    if (model_path.empty())
    {
        BOOST_TEST_MESSAGE("skip: set HSBA_SAMPLE_MODEL to a sliceable mesh to cover g-code previews");
        return;
    }

    HsBaFdmPipelineConfig_t cfg = MissingModelConfig();
    cfg.model_path = model_path.c_str();
    cfg.enable_support = 0;
    cfg.gcode_thumbnails = 1;
    cfg.gcode_thumbnail_size = 64;  // a preview, not a render
    HsBaJobHandle job = HsBaSubmitJob(HSBA_PIPELINE_FDM, &cfg, nullptr, nullptr, nullptr, nullptr);
    BOOST_REQUIRE(job != nullptr);
    BOOST_CHECK_EQUAL(HsBaWaitJob(job, kJobTimeoutMs), 1);

    if (HsBaJobGetState(job) != HSBA_JOB_SUCCEEDED)
    {
        BOOST_FAIL(std::string("preview run did not slice, job state ") +
                   std::to_string(static_cast<int>(HsBaJobGetState(job))));
    }
    HsBaJobResultView_t view{};
    BOOST_REQUIRE_EQUAL(HsBaJobCopyResult(job, &view), 1);
    BOOST_REQUIRE_EQUAL(view.success, 1);
    BOOST_REQUIRE(view.inline_content != nullptr);
    const std::string gcode(view.inline_content);

    // Both views belong to the head of the file, ahead of the banner and of the first move.
    BOOST_CHECK_LT(gcode.find("; thumbnail begin"), gcode.find("; Generated by HsBaSlicer"));
    BOOST_CHECK_LT(gcode.find("; thumbnail end"), gcode.find("G1 "));

    // The requested edge reaches the image, and a sample mesh projects into every view: a skip
    // notice here would mean the pipeline handed over polygons it could not draw.
    BOOST_CHECK_EQUAL(gcode.find("; thumbnail skipped"), std::string::npos);
    for (const std::string& name : {"model_top", "model_side"})
    {
        BOOST_CHECK(gcode.find("; thumbnail begin name=" + name + " width=64 height=64 format=png b64len=") !=
                    std::string::npos);
        BOOST_CHECK(gcode.find("; thumbnail end name=" + name) != std::string::npos);
    }

    // Decoding is what makes the block worth having: the rejoined chunks must be a PNG of the size
    // the caller asked for, not some placeholder bytes.
    const std::string encoded = ThumbnailBase64(gcode, "model_top");
    BOOST_CHECK_GT(encoded.size(), 100U);
    const std::string png = HsBa::Slicer::Cipher::Encoder::base64_decode_to_string(encoded);
    BOOST_REQUIRE_GE(png.size(), 24U);
    BOOST_CHECK_EQUAL(png.substr(0, 8), std::string("\x89PNG\r\n\x1a\n", 8));
    // IHDR stores width and height as big-endian 32-bit values, both below 256 here.
    BOOST_CHECK_EQUAL(png[16], '\0');
    BOOST_CHECK_EQUAL(png[17], '\0');
    BOOST_CHECK_EQUAL(png[18], '\0');
    BOOST_CHECK_EQUAL(png[19], '\x40');  // 64
    BOOST_CHECK_EQUAL(png[22], '\0');
    BOOST_CHECK_EQUAL(png[23], '\x40');  // 64

    HsBaFreeJobResultView(&view);
    HsBaReleaseJob(job);
}

BOOST_AUTO_TEST_CASE(struct_size_of_every_default_config_matches_sizeof)
{
    // ABI guard: the nine *ConfigDefault() factories must publish the caller's sizeof so
    // ConfigClone can pick the right truncation. A silent regression here would leave every
    // host on the "unknown size" full-copy path, re-exposing the tail-field OOB the guard
    // is meant to prevent.
    BOOST_CHECK_EQUAL(HsBaFdmConfigDefault().struct_size, sizeof(HsBaFdmPipelineConfig_t));
    BOOST_CHECK_EQUAL(HsBaSlaConfigDefault().struct_size, sizeof(HsBaSlaPipelineConfig_t));
    BOOST_CHECK_EQUAL(HsBaSlsConfigDefault().struct_size, sizeof(HsBaSlsPipelineConfig_t));
    BOOST_CHECK_EQUAL(HsBaSlmConfigDefault().struct_size, sizeof(HsBaSlmPipelineConfig_t));
    BOOST_CHECK_EQUAL(HsBaLomConfigDefault().struct_size, sizeof(HsBaLomPipelineConfig_t));
    BOOST_CHECK_EQUAL(HsBaTdpConfigDefault().struct_size, sizeof(HsBaTdpPipelineConfig_t));
    BOOST_CHECK_EQUAL(HsBaWaamConfigDefault().struct_size, sizeof(HsBaWaamPipelineConfig_t));
    BOOST_CHECK_EQUAL(HsBaFileTransferConfigDefault().struct_size, sizeof(HsBaFileTransferPipelineConfig_t));
    BOOST_CHECK_EQUAL(HsBaCustomConfigDefault().struct_size, sizeof(HsBaCustomPipelineConfig_t));
}

BOOST_AUTO_TEST_CASE(truncated_struct_size_is_clamped_instead_of_read_past_the_host)
{
    // Simulate an old host compiled against a header without the tail GUI fields: they can only
    // honestly report the offset up to (but excluding) capture_geometry. ConfigClone must copy
    // only that prefix and zero the rest; the pipeline still has to run to completion without
    // dereferencing the garbage that a naive full-size memcpy would have left in cancel_flag.
    HsBaFdmPipelineConfig_t cfg = MissingModelConfig();
    cfg.capture_geometry = 1;  // pretend the caller set it, but reported a smaller struct_size
    cfg.struct_size = offsetof(HsBaFdmPipelineConfig_t, capture_geometry);
    HsBaJobHandle job = HsBaSubmitJob(HSBA_PIPELINE_FDM, &cfg, nullptr, nullptr, nullptr, nullptr);
    BOOST_REQUIRE(job != nullptr);
    BOOST_CHECK_EQUAL(HsBaWaitJob(job, kJobTimeoutMs), 1);
    // A run that failed on the missing model still ends deterministically; the key assertion is
    // that the process survived the ConfigClone truncation path at all.
    BOOST_CHECK(HsBaJobGetState(job) == HSBA_JOB_FAILED || HsBaJobGetState(job) == HSBA_JOB_SUCCEEDED);
    // Capture was beyond the reported struct_size, so the library must have seen 0.
    BOOST_CHECK_EQUAL(HsBaJobGeometryId(job), 0);
    HsBaReleaseJob(job);
}

BOOST_AUTO_TEST_SUITE_END()
