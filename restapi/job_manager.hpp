/** @file job_manager.hpp
 * @brief In-process asynchronous job queue that proxies the slicing pipelines.
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_RESTAPI_JOB_MANAGER_HPP
#define HSBA_SLICER_RESTAPI_JOB_MANAGER_HPP

#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "fileoperator/param_convert.hpp"
#include "pipelinetypes/pipeline_types.h"

#include "pipeline_executor.hpp"

namespace HsBa::Slicer::RestApi
{
/** @brief Lifecycle state of a submitted job. */
enum class JobState
{
    Queued,   ///< Accepted, waiting for a worker.
    Running,  ///< Being executed by a worker thread.
    Success,  ///< Finished with a usable result.
    Failed,   ///< Finished with an error (pipeline failure or thrown exception).
};

/** @brief Stable textual name for a job state (lowercased enumerator name). */
std::string JobStateName(JobState state);

/**
 * @brief Serializable snapshot of a job (GET /api/jobs/{id}).
 *
 * A plain aggregate of std::string / int / double so struct_json and struct_xml
 * can render it directly via SerializeValue. Large payloads (gcode content, file
 * paths) are intentionally kept here as short metadata; the raw gcode is served
 * separately by the result endpoint.
 */
struct JobStatusView
{
    std::string id;                ///< Job identifier.
    std::string kind;              ///< Pipeline kind wire name.
    std::string state;             ///< "queued" | "running" | "success" | "failed".
    int progress_percent = 0;      ///< Last reported progress (0-100).
    std::string progress_stage;    ///< Last reported stage description.
    int success = 0;               ///< Pipeline success flag once finished (0/1).
    int total_layers = 0;          ///< Result layer count.
    double elapsed_seconds = 0.0;  ///< Result wall-clock time.
    std::string output_path;       ///< Result output/export path (empty when none).
    std::string error_message;     ///< Failure text (empty when successful/unfinished).
    int files_transferred = 0;     ///< File-transfer result counter.
    int total_files = 0;           ///< File-transfer result counter.
};

/** @brief Internal per-job record (owns the config struct and its string arena). */
struct JobEntry
{
    std::string id;
    HsBaPipelineKind kind = HSBA_PIPELINE_FDM;
    JobState state = JobState::Queued;
    int percent = 0;
    std::string stage;
    void* config = nullptr;                          ///< Heap config from pipeline_executor (owned).
    StringArena arena;                               ///< Backs const char* fields written into config.
    SliceOutcome outcome;                            ///< Filled when the run finishes.
    std::string error_message;                       ///< Exception text when the run throws.
    std::chrono::steady_clock::time_point finish{};  ///< Set when state becomes Success/Failed.
};

/**
 * @brief Thread-safe job queue with a fixed worker pool.
 *
 * Submit() takes ownership of a fully populated config (allocated by
 * pipeline_executor) plus its string arena and runs the pipeline on a worker
 * thread. Status/result lookups are non-blocking snapshots.
 */
class JobManager
{
public:
    /**
     * @brief Pipeline runner seam. Defaults to pipeline_executor::RunPipeline;
     *        tests inject a mock to exercise the state machine without real models.
     */
    using Runner = std::function<SliceOutcome(HsBaPipelineKind, const void*, HsBaProgressCallback, void*)>;

    /** @param workers  concurrent job slots; @param max_jobs tracked-job cap; @param ttl finished-job retention. */
    JobManager(int workers, int max_jobs, std::chrono::seconds ttl, Runner runner = {});
    ~JobManager();

    JobManager(const JobManager&) = delete;
    JobManager& operator=(const JobManager&) = delete;

    /**
     * @brief Enqueue a job, taking ownership of @p config and @p arena.
     * @return New job id, or an empty string when the queue is full (HTTP 429).
     */
    std::string Submit(HsBaPipelineKind kind, void* config, StringArena arena);

    /** @brief Snapshot a job's status, or false when @p id is unknown. */
    bool Status(const std::string& id, JobStatusView& out) const;

    /** @brief Copy out a finished job's full outcome; false when unknown or not finished successfully. */
    bool Outcome(const std::string& id, SliceOutcome& out) const;

    /** @brief Whether a job id exists (any state). */
    bool Exists(const std::string& id) const;

    /** @brief Outcome of a DELETE request on a job. */
    enum class RemoveResult
    {
        Removed,   ///< Queued or finished job deleted and cleaned up.
        NotFound,  ///< No such job id.
        Running,   ///< Job is executing and must not be deleted (HTTP 409).
    };

    /**
     * @brief Delete a job, freeing its config/arena (queued) or result (terminal).
     *        Running jobs are refused; a queued job is also unqueued.
     */
    RemoveResult Remove(const std::string& id);

    /**
     * @brief Snapshot every job's status, newest last.
     * @param state_filter Optional state name ("queued"/"running"/"success"/"failed"); empty = all.
     * @param offset       Skip this many matching jobs (pagination).
     * @param limit        Max jobs to return (0 = no cap).
     */
    std::vector<JobStatusView> List(const std::string& state_filter, std::size_t offset, std::size_t limit) const;

    /**
     * @brief Record a progress update for a running job (invoked by the pipeline
     *        progress callback bridge). No-op when the id is unknown.
     */
    void SetProgress(const std::string& id, int percent, const char* stage);

private:
    void WorkerLoop();
    void Execute(JobEntry* job);
    void ReapLocked(std::chrono::steady_clock::time_point now);

    mutable std::mutex mtx_;
    std::condition_variable cv_;
    std::unordered_map<std::string, std::unique_ptr<JobEntry>> jobs_;
    std::deque<std::string> ready_;
    std::vector<std::thread> workers_;
    bool stop_ = false;
    unsigned long long next_id_ = 0;
    int max_jobs_ = 128;
    std::chrono::seconds ttl_{3600};
    Runner runner_;
};

}  // namespace HsBa::Slicer::RestApi

#endif  // HSBA_SLICER_RESTAPI_JOB_MANAGER_HPP
