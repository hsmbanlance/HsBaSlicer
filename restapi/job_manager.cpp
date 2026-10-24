/** @file job_manager.cpp
 * @brief Implements the async job queue; see job_manager.hpp.
 * @author HsBa
 */
#include "job_manager.hpp"

#include <algorithm>
#include <exception>
#include <string>
#include <string_view>
#include <utility>

#include <magic_enum/magic_enum.hpp>

#include "base/string_helper.hpp"

namespace HsBa::Slicer::RestApi
{
std::string JobStateName(JobState state)
{
    const std::string_view name = magic_enum::enum_name(state);
    return name.empty() ? "unknown" : Utils::to_lower(name);
}

namespace
{
// Bridge between the C progress callback and the manager: passed as user_data to
// RunPipeline, which invokes it synchronously on the worker thread while the job
// is running, so this context's lifetime (a stack local of Execute) is sufficient.
struct ProgressCtx
{
    JobManager* mgr;
    JobEntry* job;

    static void Trampoline(int percent, const char* stage, void* ud)
    {
        auto* self = static_cast<ProgressCtx*>(ud);
        self->mgr->SetProgress(self->job->id, percent, stage);
    }
};
}  // namespace

JobManager::JobManager(int workers, int max_jobs, std::chrono::seconds ttl, Runner runner)
    : max_jobs_(max_jobs > 0 ? max_jobs : 1), ttl_(ttl), runner_(std::move(runner))
{
    if (!runner_)
    {
        runner_ = [](HsBaPipelineKind kind, const void* config, HsBaProgressCallback cb, void* ud)
        { return RunPipeline(kind, config, cb, ud); };
    }
    const int count = workers > 0 ? workers : 1;
    workers_.reserve(count);
    for (int i = 0; i < count; ++i)
    {
        workers_.emplace_back([this] { WorkerLoop(); });
    }
}

JobManager::~JobManager()
{
    {
        std::lock_guard<std::mutex> lk(mtx_);
        stop_ = true;
    }
    cv_.notify_all();
    for (auto& t : workers_)
    {
        if (t.joinable())
        {
            t.join();
        }
    }
    // Release any config structs left un-executed (shutdown with queued work).
    for (auto& [id, job] : jobs_)
    {
        if (job->config)
        {
            DestroyConfig(job->kind, job->config);
        }
    }
}

std::string JobManager::Submit(HsBaPipelineKind kind, void* config, StringArena arena)
{
    std::lock_guard<std::mutex> lk(mtx_);
    ReapLocked(std::chrono::steady_clock::now());
    if (static_cast<int>(jobs_.size()) >= max_jobs_)
    {
        // Queue full: reject. The caller still owns config/arena, so free them here.
        DestroyConfig(kind, config);
        return {};
    }
    auto job = std::make_unique<JobEntry>();
    job->id = "job-" + std::to_string(++next_id_);
    job->kind = kind;
    job->state = JobState::Queued;
    job->config = config;
    job->arena = std::move(arena);
    const std::string id = job->id;
    jobs_.emplace(id, std::move(job));
    ready_.push_back(id);
    cv_.notify_one();
    return id;
}

bool JobManager::Status(const std::string& id, JobStatusView& out) const
{
    std::lock_guard<std::mutex> lk(mtx_);
    const auto it = jobs_.find(id);
    if (it == jobs_.end())
    {
        return false;
    }
    const JobEntry& job = *it->second;
    out.id = job.id;
    out.kind = KindName(job.kind);
    out.state = JobStateName(job.state);
    out.progress_percent = job.percent;
    out.progress_stage = job.stage;
    out.error_message = job.error_message;
    if (job.state == JobState::Success || job.state == JobState::Failed)
    {
        out.success = job.outcome.success;
        out.total_layers = job.outcome.total_layers;
        out.elapsed_seconds = job.outcome.elapsed_seconds;
        out.output_path = job.outcome.output_path;
        out.files_transferred = job.outcome.files_transferred;
        out.total_files = job.outcome.total_files;
    }
    return true;
}

bool JobManager::Outcome(const std::string& id, SliceOutcome& out) const
{
    std::lock_guard<std::mutex> lk(mtx_);
    const auto it = jobs_.find(id);
    if (it == jobs_.end() || it->second->state != JobState::Success)
    {
        return false;
    }
    out = it->second->outcome;
    return true;
}

bool JobManager::Exists(const std::string& id) const
{
    std::lock_guard<std::mutex> lk(mtx_);
    return jobs_.contains(id);
}

namespace
{
// Numeric suffix of a "job-<n>" id, used to keep list order stable (creation order).
unsigned long long IdNumber(const std::string& id)
{
    const auto pos = id.find_last_of('-');
    if (pos == std::string::npos)
    {
        return 0;
    }
    unsigned long long n = 0;
    for (auto it = id.begin() + static_cast<long>(pos) + 1; it != id.end(); ++it)
    {
        if (*it >= '0' && *it <= '9')
        {
            n = n * 10 + static_cast<unsigned long long>(*it - '0');
        }
    }
    return n;
}

void FillView(const JobEntry& job, JobStatusView& out)
{
    out.id = job.id;
    out.kind = KindName(job.kind);
    out.state = JobStateName(job.state);
    out.progress_percent = job.percent;
    out.progress_stage = job.stage;
    out.error_message = job.error_message;
    if (job.state == JobState::Success || job.state == JobState::Failed)
    {
        out.success = job.outcome.success;
        out.total_layers = job.outcome.total_layers;
        out.elapsed_seconds = job.outcome.elapsed_seconds;
        out.output_path = job.outcome.output_path;
        out.files_transferred = job.outcome.files_transferred;
        out.total_files = job.outcome.total_files;
    }
}
}  // namespace

std::vector<JobStatusView> JobManager::List(const std::string& state_filter, std::size_t offset,
                                            std::size_t limit) const
{
    std::vector<JobStatusView> matched;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        for (const auto& [id, job] : jobs_)
        {
            const std::string name = JobStateName(job->state);
            if (!state_filter.empty() && state_filter != name)
            {
                continue;
            }
            JobStatusView view;
            FillView(*job, view);
            matched.push_back(std::move(view));
        }
    }
    std::sort(matched.begin(), matched.end(),
              [](const JobStatusView& a, const JobStatusView& b) { return IdNumber(a.id) < IdNumber(b.id); });
    if (offset >= matched.size())
    {
        matched.clear();
        return matched;
    }
    matched.erase(matched.begin(), matched.begin() + static_cast<long>(offset));
    if (limit != 0 && matched.size() > limit)
    {
        matched.resize(limit);
    }
    return matched;
}

JobManager::RemoveResult JobManager::Remove(const std::string& id)
{
    std::lock_guard<std::mutex> lk(mtx_);
    const auto it = jobs_.find(id);
    if (it == jobs_.end())
    {
        return RemoveResult::NotFound;
    }
    if (it->second->state == JobState::Running)
    {
        return RemoveResult::Running;
    }
    // A queued job still sits in the ready deque; drop its slot so no worker
    // picks up a dangling pointer. A terminal job has already nulled config.
    if (it->second->state == JobState::Queued)
    {
        ready_.erase(std::remove(ready_.begin(), ready_.end(), id), ready_.end());
        if (it->second->config)
        {
            DestroyConfig(it->second->kind, it->second->config);
            it->second->config = nullptr;
        }
    }
    jobs_.erase(it);
    return RemoveResult::Removed;
}

void JobManager::SetProgress(const std::string& id, int percent, const char* stage)
{
    std::lock_guard<std::mutex> lk(mtx_);
    const auto it = jobs_.find(id);
    if (it == jobs_.end())
    {
        return;
    }
    it->second->percent = std::clamp(percent, 0, 100);
    it->second->stage = stage ? stage : "";
}

void JobManager::WorkerLoop()
{
    for (;;)
    {
        JobEntry* job = nullptr;
        {
            std::unique_lock<std::mutex> lk(mtx_);
            cv_.wait(lk, [this] { return stop_ || !ready_.empty(); });
            if (stop_ && ready_.empty())
            {
                return;
            }
            const std::string id = ready_.front();
            ready_.pop_front();
            const auto it = jobs_.find(id);
            if (it != jobs_.end())
            {
                job = it->second.get();
                job->state = JobState::Running;
            }
        }
        if (job)
        {
            Execute(job);
        }
    }
}

void JobManager::Execute(JobEntry* job)
{
    ProgressCtx ctx{this, job};
    SliceOutcome outcome;
    std::string thrown;
    bool failed_exception = false;
    try
    {
        outcome = runner_(job->kind, job->config, &ProgressCtx::Trampoline, &ctx);
    }
    catch (const std::exception& e)
    {
        failed_exception = true;
        thrown = e.what();
    }
    catch (...)
    {
        failed_exception = true;
        thrown = "unknown pipeline error";
    }

    // Release the config struct and its string arena now that the run is done.
    DestroyConfig(job->kind, job->config);

    std::lock_guard<std::mutex> lk(mtx_);
    job->config = nullptr;
    job->arena.clear();
    job->outcome = std::move(outcome);
    if (failed_exception)
    {
        job->state = JobState::Failed;
        job->error_message = thrown;
    }
    else
    {
        job->state = job->outcome.success ? JobState::Success : JobState::Failed;
        job->error_message = job->outcome.error_message;
    }
    job->percent = 100;
    job->finish = std::chrono::steady_clock::now();
}

void JobManager::ReapLocked(std::chrono::steady_clock::time_point now)
{
    for (auto it = jobs_.begin(); it != jobs_.end();)
    {
        const JobEntry& job = *it->second;
        const bool finished = job.state == JobState::Success || job.state == JobState::Failed;
        if (finished && (now - job.finish) >= ttl_)
        {
            it = jobs_.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

}  // namespace HsBa::Slicer::RestApi
