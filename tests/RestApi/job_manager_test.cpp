/** @file job_manager_test.cpp
 * @brief Async job-queue state machine tests with an injected mock pipeline runner.
 * @author HsBa
 */
#include <chrono>
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
    return [](HsBaPipelineKind, const void*, HsBaProgressCallback cb, void* ud) {
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
    for (int i = 0; i < 200; ++i)
    {
        if (jm.Status(id, out) && (out.state == "success" || out.state == "failed"))
        {
            return true;
        }
        std::this_thread::sleep_for(10ms);
    }
    return false;
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

BOOST_AUTO_TEST_SUITE_END()
