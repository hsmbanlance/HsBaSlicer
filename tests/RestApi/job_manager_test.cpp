/** @file job_manager_test.cpp
 * @brief Async job-queue state machine tests with an injected mock pipeline runner.
 * @author HsBa
 */
#include <atomic>
#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>

#define BOOST_TEST_MODULE RestApiJobManagerTest
#include <boost/test/included/unit_test.hpp>

#include "pipelinetypes/pipeline_types.h"

#include "../../restapi/job_manager.hpp"
#include "../../restapi/pipeline_executor.hpp"

using namespace HsBa::Slicer;
using namespace HsBa::Slicer::RestApi;
using namespace std::chrono_literals;

namespace
{
// A runner that always succeeds, emits one progress tick and a small inline payload.
JobManager::Runner SuccessRunner()
{
    return [](HsBaPipelineKind, const void*, HsBaProgressCallback cb, void* ud)
    {
        if (cb)
        {
            cb(50, "half", ud);
        }
        SliceOutcome o;
        o.success = 1;
        o.total_layers = 7;
        o.inline_content = "G0 X0";
        o.elapsed_seconds = 0.5;
        if (cb)
        {
            cb(100, "done", ud);
        }
        return o;
    };
}

// Block until the job leaves the queued/running states, or timeout.
bool WaitForTerminal(JobManager& jm, const std::string& id, JobStatusView& out)
{
    for (int i = 0; i < 2000; ++i)
    {
        if (jm.Status(id, out) && (out.state == "success" || out.state == "failed" || out.state == "cancelled"))
        {
            return true;
        }
        std::this_thread::sleep_for(5ms);
    }
    return false;
}

// Block until a job reports the wanted state. Cancellation is only requested, never
// applied, so a run leaves "running" for "cancelled" on its own schedule and both the
// queue and the HTTP layer have to be observed by polling.
bool WaitForState(JobManager& jm, const std::string& id, const std::string& wanted, JobStatusView& out)
{
    for (int i = 0; i < 2000; ++i)
    {
        if (jm.Status(id, out) && out.state == wanted)
        {
            return true;
        }
        std::this_thread::sleep_for(5ms);
    }
    return false;
}

// Block until a progress tick with the wanted stage text has been recorded. A job turns
// "running" before its first tick lands, so the state alone is no evidence of progress.
bool WaitForProgress(JobManager& jm, const std::string& id, const std::string& stage, JobStatusView& out)
{
    for (int i = 0; i < 2000; ++i)
    {
        if (jm.Status(id, out) && out.progress_stage == stage)
        {
            return true;
        }
        std::this_thread::sleep_for(5ms);
    }
    return false;
}

/** @brief Reads the cancellation flag the queue writes into the config tail. */
bool CancelRequested(const void* config)
{
    const auto* fdm = static_cast<const HsBaFdmPipelineConfig_t*>(config);
    const std::atomic<bool>* flag = static_cast<const std::atomic<bool>*>(fdm->cancel_flag);
    return flag != nullptr && flag->load();
}

/**
 * @brief A runner that stays inside the run until the host cancels it.
 *
 * The wait is bounded to ten seconds, so a missing Cancel() fails an assertion instead of
 * hanging the suite. @p throw_on_cancel selects between the two shapes a real pipeline uses
 * to report cancellation: an outcome carrying the reserved "Cancelled" text, or an exception.
 */
JobManager::Runner FlagRunner(bool throw_on_cancel)
{
    return [throw_on_cancel](HsBaPipelineKind, const void* config, HsBaProgressCallback cb, void* ud)
    {
        if (cb)
        {
            cb(30, "Slicing layer", ud);
        }
        for (int waited = 0; waited < 10000 && !CancelRequested(config); waited += 10)
        {
            std::this_thread::sleep_for(10ms);
        }
        SliceOutcome o;
        if (!CancelRequested(config))
        {
            o.success = 1;
            o.total_layers = 4;
            return o;
        }
        if (throw_on_cancel)
        {
            throw std::runtime_error("Cancelled: pipeline cancelled");
        }
        o.success = 0;
        o.error_message = "Cancelled: pipeline cancelled";
        return o;
    };
}

/** @brief A runner that reports one tick of a text the stage table cannot classify. */
JobManager::Runner UnclassifiedRunner()
{
    return [](HsBaPipelineKind, const void*, HsBaProgressCallback cb, void* ud)
    {
        if (cb)
        {
            cb(40, "Wiggling the lattice", ud);
        }
        SliceOutcome o;
        o.success = 1;
        return o;
    };
}

/** @brief Textual name of a cancellation outcome, so a failed assert reads as a state. */
const char* CancelResultName(CancelResult result)
{
    switch (result)
    {
    case CancelResult::Accepted:
        return "accepted";
    case CancelResult::Removed:
        return "removed";
    case CancelResult::AlreadyTerminal:
        return "already-terminal";
    case CancelResult::NotFound:
        return "not-found";
    }
    return "unknown";
}
}  // namespace

BOOST_AUTO_TEST_SUITE(restapi_job_manager)

BOOST_AUTO_TEST_CASE(submitted_job_reaches_success)
{
    JobManager jm(1, 16, 1h, SuccessRunner());
    void* cfg = CreateDefaultConfig(HSBA_PIPELINE_FDM);
    const std::string id = jm.Submit(HSBA_PIPELINE_FDM, cfg, StringArena{});
    BOOST_CHECK(!id.empty());

    JobStatusView view;
    BOOST_REQUIRE(WaitForTerminal(jm, id, view));
    BOOST_CHECK_EQUAL(view.state, "success");
    BOOST_CHECK_EQUAL(view.success, 1);
    BOOST_CHECK_EQUAL(view.total_layers, 7);
    BOOST_CHECK_EQUAL(view.progress_percent, 100);

    SliceOutcome outcome;
    BOOST_CHECK(jm.Outcome(id, outcome));
    BOOST_CHECK_EQUAL(outcome.inline_content, "G0 X0");
}

BOOST_AUTO_TEST_CASE(queue_full_is_rejected)
{
    JobManager jm(1, 1, 1h, SuccessRunner());
    const std::string first = jm.Submit(HSBA_PIPELINE_FDM, CreateDefaultConfig(HSBA_PIPELINE_FDM), StringArena{});
    BOOST_CHECK(!first.empty());
    // max_jobs == 1 and the first job is still tracked, so the next submission is refused.
    const std::string second = jm.Submit(HSBA_PIPELINE_FDM, CreateDefaultConfig(HSBA_PIPELINE_FDM), StringArena{});
    BOOST_CHECK(second.empty());
}

BOOST_AUTO_TEST_CASE(unknown_id_has_no_status)
{
    JobManager jm(1, 4, 1h, SuccessRunner());
    JobStatusView view;
    BOOST_CHECK(!jm.Status("does-not-exist", view));
    BOOST_CHECK(!jm.Exists("does-not-exist"));
}

BOOST_AUTO_TEST_CASE(finished_jobs_are_reaped_after_ttl)
{
    JobManager jm(1, 8, 0s, SuccessRunner());  // zero TTL -> reaped on next Submit
    const std::string first = jm.Submit(HSBA_PIPELINE_FDM, CreateDefaultConfig(HSBA_PIPELINE_FDM), StringArena{});
    JobStatusView view;
    BOOST_REQUIRE(WaitForTerminal(jm, first, view));

    // Submitting again triggers the reaper, which drops the already-expired first job.
    const std::string second = jm.Submit(HSBA_PIPELINE_FDM, CreateDefaultConfig(HSBA_PIPELINE_FDM), StringArena{});
    BOOST_CHECK(!second.empty());
    BOOST_CHECK(!jm.Exists(first));
    BOOST_CHECK(jm.Exists(second));
}

BOOST_AUTO_TEST_CASE(preallocated_id_is_respected)
{
    JobManager jm(1, 8, 1h, SuccessRunner());
    // The REST layer reserves the id before writing the uploaded model so the
    // file can live in a per-job directory; Submit must then reuse it verbatim.
    const std::string reserved = jm.NextId();
    const std::string submitted =
        jm.Submit(HSBA_PIPELINE_FDM, CreateDefaultConfig(HSBA_PIPELINE_FDM), StringArena{}, reserved);
    BOOST_CHECK_EQUAL(submitted, reserved);
    BOOST_CHECK(jm.Exists(reserved));
    // The counter advanced past the reserved id, so the next reservation differs.
    BOOST_CHECK(jm.NextId() != reserved);
}

BOOST_AUTO_TEST_CASE(terminal_callback_fires_once_with_the_job_id)
{
    std::atomic<int> calls{0};
    std::string observed;
    JobManager jm(1, 8, 1h, SuccessRunner(),
                  [&calls, &observed](const std::string& job_id)
                  {
                      ++calls;
                      observed = job_id;
                  });
    const std::string id = jm.Submit(HSBA_PIPELINE_FDM, CreateDefaultConfig(HSBA_PIPELINE_FDM), StringArena{});
    BOOST_REQUIRE(!id.empty());

    JobStatusView view;
    BOOST_REQUIRE(WaitForTerminal(jm, id, view));
    // The sink runs outside the job mutex, so it may land just after the status flips.
    for (int i = 0; i < 200 && calls.load() == 0; ++i)
    {
        std::this_thread::sleep_for(10ms);
    }
    BOOST_CHECK_EQUAL(calls.load(), 1);
    BOOST_CHECK_EQUAL(observed, id);
}

BOOST_AUTO_TEST_CASE(cancelling_a_running_job_lands_on_the_cancelled_state)
{
    JobManager jm(1, 8, 1h, FlagRunner(false));
    const std::string id = jm.Submit(HSBA_PIPELINE_FDM, CreateDefaultConfig(HSBA_PIPELINE_FDM), StringArena{});
    BOOST_REQUIRE(!id.empty());

    JobStatusView view;
    BOOST_REQUIRE(WaitForState(jm, id, "running", view));
    BOOST_REQUIRE(WaitForProgress(jm, id, "Slicing layer", view));
    // The queue classifies the reported text itself, so a GUI can key its timeline off a
    // stable id instead of matching on a human-readable string that may be reworded.
    BOOST_CHECK_EQUAL(view.progress_stage_id, "slicing.layer");
    // A run that captured nothing must not advertise geometry the client cannot fetch.
    BOOST_CHECK_EQUAL(view.geometry_available, 0);
    BOOST_CHECK_EQUAL(view.geometry_layers, 0);

    // Cancel only signals; the state turns on its own once the run unwinds.
    BOOST_CHECK_EQUAL(CancelResultName(jm.Cancel(id)), "accepted");
    BOOST_REQUIRE(WaitForTerminal(jm, id, view));
    BOOST_CHECK_EQUAL(view.state, "cancelled");
    BOOST_CHECK_EQUAL(JobStateName(JobState::Cancelled), "cancelled");
    BOOST_CHECK_EQUAL(view.error_message, "Cancelled: pipeline cancelled");
    BOOST_CHECK_EQUAL(view.success, 0);
    BOOST_CHECK_EQUAL(view.progress_stage_id, "cancelled");

    // Partial work is not a result, cancellation is not repeatable, and both the geometry
    // lookup and the state filter have to follow the final state.
    SliceOutcome outcome;
    BOOST_CHECK(!jm.Outcome(id, outcome));
    BOOST_CHECK_EQUAL(CancelResultName(jm.Cancel(id)), "already-terminal");
    long long geometry_id = 1;
    BOOST_CHECK(!jm.GeometryId(id, geometry_id));
    BOOST_CHECK_EQUAL(jm.List("cancelled", 0, 0).size(), 1u);
    BOOST_CHECK(jm.List("success", 0, 0).empty());
}

BOOST_AUTO_TEST_CASE(a_cancelling_runner_that_throws_is_not_reported_as_a_failure)
{
    JobManager jm(1, 8, 1h, FlagRunner(true));
    const std::string id = jm.Submit(HSBA_PIPELINE_FDM, CreateDefaultConfig(HSBA_PIPELINE_FDM), StringArena{});
    JobStatusView view;
    BOOST_REQUIRE(WaitForState(jm, id, "running", view));
    BOOST_CHECK_EQUAL(CancelResultName(jm.Cancel(id)), "accepted");
    BOOST_REQUIRE(WaitForTerminal(jm, id, view));
    // A cancelled run unwinds through the exception handler as well; the reserved "Cancelled"
    // prefix outranks that path, so the client is not told the pipeline broke internally.
    BOOST_CHECK_EQUAL(view.state, "cancelled");
    BOOST_CHECK_EQUAL(view.error_message, "Cancelled: pipeline cancelled");
}

BOOST_AUTO_TEST_CASE(cancelling_a_queued_job_removes_it_before_it_starts)
{
    JobManager jm(1, 8, 1h, FlagRunner(false));
    // One worker: the first job holds it, so the second one provably stays in the queue.
    const std::string blocker = jm.Submit(HSBA_PIPELINE_FDM, CreateDefaultConfig(HSBA_PIPELINE_FDM), StringArena{});
    JobStatusView view;
    BOOST_REQUIRE(WaitForState(jm, blocker, "running", view));

    const std::string queued = jm.Submit(HSBA_PIPELINE_FDM, CreateDefaultConfig(HSBA_PIPELINE_FDM), StringArena{});
    BOOST_REQUIRE(jm.Status(queued, view));
    BOOST_CHECK_EQUAL(view.state, "queued");

    BOOST_CHECK_EQUAL(CancelResultName(jm.Cancel(queued)), "removed");
    // A job that never ran is dropped outright: there is no result, no progress and no
    // record to cancel a second time, so the id is simply gone.
    BOOST_CHECK(!jm.Exists(queued));
    BOOST_CHECK(!jm.Status(queued, view));
    BOOST_CHECK_EQUAL(CancelResultName(jm.Cancel(queued)), "not-found");

    BOOST_CHECK_EQUAL(CancelResultName(jm.Cancel(blocker)), "accepted");
    BOOST_REQUIRE(WaitForTerminal(jm, blocker, view));
    BOOST_CHECK_EQUAL(view.state, "cancelled");
}

BOOST_AUTO_TEST_CASE(cancel_after_completion_is_refused_but_keeps_the_record)
{
    JobManager jm(1, 8, 1h, SuccessRunner());
    const std::string id = jm.Submit(HSBA_PIPELINE_FDM, CreateDefaultConfig(HSBA_PIPELINE_FDM), StringArena{});
    JobStatusView view;
    BOOST_REQUIRE(WaitForTerminal(jm, id, view));
    BOOST_CHECK_EQUAL(CancelResultName(jm.Cancel(id)), "already-terminal");
    // Refusing a late cancel must not remove anything: the client still wants the status.
    BOOST_CHECK(jm.Exists(id));
    BOOST_CHECK(jm.Status(id, view));
    BOOST_CHECK_EQUAL(view.state, "success");
}

BOOST_AUTO_TEST_CASE(unknown_stage_text_still_gets_a_stable_id)
{
    JobManager jm(1, 8, 1h, UnclassifiedRunner());
    const std::string id = jm.Submit(HSBA_PIPELINE_FDM, CreateDefaultConfig(HSBA_PIPELINE_FDM), StringArena{});
    JobStatusView view;
    BOOST_REQUIRE(WaitForTerminal(jm, id, view));
    // Custom (Lua) pipelines invent their own stage texts. The original string is kept for
    // old hosts, while the id degrades to "unknown" rather than to a wrong stage or a blank.
    BOOST_CHECK_EQUAL(view.progress_stage, "Wiggling the lattice");
    BOOST_CHECK_EQUAL(view.progress_stage_id, "unknown");
}

BOOST_AUTO_TEST_SUITE_END()
