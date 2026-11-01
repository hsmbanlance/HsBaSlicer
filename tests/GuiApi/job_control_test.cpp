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
#include <cstdlib>
#include <mutex>
#include <string>
#include <thread>

#define BOOST_TEST_MODULE GuiApiJobControlTest
#include <boost/test/included/unit_test.hpp>

#include "DllHsBaSlicer/file_transfer_pipeline.h"
#include "DllHsBaSlicer/geometry_export.h"
#include "DllHsBaSlicer/job_control.h"
#include "DllHsBaSlicer/progress_stage.h"
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

BOOST_AUTO_TEST_SUITE_END()
