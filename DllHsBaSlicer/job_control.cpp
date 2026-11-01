/** @file job_control.cpp
 * @brief Job handle implementation: config deep copy, worker thread, cancellation and result view.
 * @author HsBa
 */
#include "job_control.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "base/error.hpp"
#include "custom_pipeline.h"
#include "fdm_pipeline.h"
#include "file_transfer_pipeline.h"
#include "geometry_store.hpp"
#include "lom_pipeline.h"
#include "progress_stage.h"
#include "sla_pipeline.h"
#include "slm_pipeline.h"
#include "sls_pipeline.h"
#include "tdp_pipeline.h"
#include "waam_pipeline.h"

namespace HsBa::Slicer::Pipeline
{

/**
 * @brief Heap copy of one per-kind configuration, with its strings owned by the library.
 *
 * The published C entries require every string to stay alive until the run ends, a
 * requirement a non-blocking submit cannot ask of the caller. The clone therefore
 * copies the struct bytes and re-points each listed string field at storage that
 * lives with the job. Moving a clone keeps published addresses stable because the
 * string vector's buffer is stolen rather than its elements copied, so the clone
 * must never be copied or resized after Build().
 */
class ConfigClone
{
public:
    ConfigClone() = default;
    ConfigClone(const ConfigClone&) = delete;
    ConfigClone& operator=(const ConfigClone&) = delete;
    ConfigClone(ConfigClone&& other) noexcept;
    ConfigClone& operator=(ConfigClone&& other) noexcept;
    ~ConfigClone() { Reset(); }

    /**
     * @brief Copy @p source and replace each string member in @p fields with owned storage.
     * @param cancel_flag Address the pipeline polls to observe a cancellation request.
     */
    template <typename ConfigT>
    void Build(const ConfigT& source, std::initializer_list<const char* ConfigT::*> fields, const void* cancel_flag);

    /** @brief Variant for the file-transfer config, which also owns a char** path array. */
    void BuildFileTransfer(const HsBaFileTransferPipelineConfig_t& source, const void* cancel_flag);

    /** @brief The copied configuration, ready to be passed to the synchronous C entry. */
    void* Get() const { return raw; }

private:
    /** @brief Release everything owned by a previous Build(). */
    void Reset();

    void* raw = nullptr;  ///< malloc'ed copy of the config struct
    std::size_t size_bytes = 0;
    std::vector<std::string> strings;     ///< Storage published through the config's char* fields
    std::unique_ptr<char*[]> path_array;  ///< File-transfer only
};

}  // namespace HsBa::Slicer::Pipeline

/**
 * @brief Library state behind an HsBaJobHandle.
 *
 * Declared at global scope because the C typedef in job_control.h names this class;
 * it is never published in a header, so hosts only ever see the opaque pointer.
 */
struct HsBaJob
{
    explicit HsBaJob(HsBaPipelineKind job_kind) : kind(job_kind) {}
    HsBaJob(const HsBaJob&) = delete;
    HsBaJob& operator=(const HsBaJob&) = delete;
    ~HsBaJob() = default;

    /** @brief One reference for the running work plus one handed to the caller. */
    std::atomic<int> refs{2};
    std::atomic<int> state{HSBA_JOB_PENDING};
    std::atomic<int> percent{0};
    std::atomic<int> stage{HSBA_STAGE_UNKNOWN};
    std::atomic<long long> geometry_id{0};

    /// @brief Cancellation request read by the pipeline at stage and layer boundaries.
    std::atomic<bool> cancel{false};

    std::mutex wait_mutex;
    std::condition_variable wait_cv;

    std::mutex text_mutex;  ///< Guards stage_text
    std::string stage_text;

    std::mutex result_mutex;  ///< Guards every field below, written once by the worker thread
    bool succeeded = false;
    int total_layers = 0;
    double elapsed_seconds = 0.0;
    int files_transferred = 0;
    int total_files = 0;
    std::string inline_content;  ///< G-code or Lua return text
    std::string output_path;     ///< Artefact path reported by the pipeline
    std::string error_message;

    HsBaPipelineKind kind;
    HsBa::Slicer::Pipeline::ConfigClone config;  ///< Deep copy, valid for the whole run

    HsBaProgressCallbackEx on_progress = nullptr;
    void* progress_user_data = nullptr;
    HsBaJobDoneCallback on_done = nullptr;
    void* done_user_data = nullptr;

    // Legacy HsBaRun*PipelineAsync bridge
    HsBa::Slicer::Pipeline::LegacyResultThunk on_legacy_done = nullptr;
    void* legacy_thunk = nullptr;
    HsBaProgressCallback legacy_progress = nullptr;
    void* legacy_progress_user_data = nullptr;
};

namespace HsBa::Slicer::Pipeline
{
namespace
{

/** @brief Forward: defined in the legacy bridge section further down this file. */
void DeliverLegacyResult(HsBaJob* job);

/** @brief Whether a kind maps to one of the nine pipelines the job machinery can configure. */
bool IsValidKind(HsBaPipelineKind kind)
{
    return kind >= HSBA_PIPELINE_FDM && kind < HSBA_PIPELINE_KIND_COUNT;
}

/** @brief Whether a state ends the job, so waiters may return. */
bool IsFinalState(int state)
{
    return state == HSBA_JOB_SUCCEEDED || state == HSBA_JOB_FAILED || state == HSBA_JOB_CANCELLED;
}

/** @brief Prefix the pipelines use for their error text when a run was cancelled. */
constexpr const char* kCancelledPrefix = "Cancelled";

/** @brief Whether a failed run reported cancellation instead of an error. */
bool ReportsCancellation(const std::string& message)
{
    return message.compare(0, std::strlen(kCancelledPrefix), kCancelledPrefix) == 0;
}

/** @brief Copy a malloc-owned result string into a std::string, tolerating NULL. */
std::string TextOf(const char* text)
{
    return text ? std::string(text) : std::string();
}

/** @brief malloc a copy of a std::string, keeping the empty-string-as-NULL convention of the results. */
char* DupText(const std::string& text)
{
    if (text.empty())
    {
        return nullptr;
    }
    auto* copy = static_cast<char*>(std::malloc(text.size() + 1));
    if (copy)
    {
        std::memcpy(copy, text.c_str(), text.size() + 1);
    }
    return copy;
}

/**
 * @brief Progress thunk handed to the synchronous pipeline entries.
 *
 * Runs on the job thread. Every per-kind progress typedef is the same function type,
 * so one thunk serves all nine pipelines.
 */
void OnJobProgress(int percent, const char* stage, void* user_data)
{
    auto* job = static_cast<HsBaJob*>(user_data);
    const HsBaStage_t classified = HsBaStageFromText(stage);
    job->percent.store(percent);
    job->stage.store(static_cast<int>(classified));
    if (stage)
    {
        const std::lock_guard<std::mutex> lock(job->text_mutex);
        job->stage_text = stage;
    }
    if (job->on_progress)
    {
        job->on_progress(percent, classified, stage, job->progress_user_data);
    }
    if (job->legacy_progress)
    {
        job->legacy_progress(percent, stage, job->legacy_progress_user_data);
    }
}

/**
 * @brief Run one pipeline synchronously and normalise its result into @p job.
 * @return 1 when the pipeline reported success, 0 on failure, -1 when an unexpected exception escaped.
 */
int ExecuteKind(HsBaJob* job)
{
    const void* config = job->config.Get();
    try
    {
        switch (job->kind)
        {
        case HSBA_PIPELINE_FDM:
        {
            HsBaFdmPipelineResult_t result =
                HsBaRunFdmPipeline(static_cast<const HsBaFdmPipelineConfig_t*>(config), OnJobProgress, job);
            job->succeeded = result.success != 0;
            job->total_layers = result.total_layers;
            job->elapsed_seconds = result.elapsed_seconds;
            job->geometry_id.store(result.geometry_id);
            {
                const std::lock_guard<std::mutex> lock(job->result_mutex);
                job->inline_content = TextOf(result.gcode_content);
                job->error_message = TextOf(result.error_message);
            }
            HsBaFreePipelineResult(&result);
            break;
        }
        case HSBA_PIPELINE_SLA:
        {
            HsBaSlaPipelineResult_t result =
                HsBaRunSlaPipeline(static_cast<const HsBaSlaPipelineConfig_t*>(config), OnJobProgress, job);
            job->succeeded = result.success != 0;
            job->total_layers = result.total_layers;
            job->elapsed_seconds = result.elapsed_seconds;
            job->geometry_id.store(result.geometry_id);
            {
                const std::lock_guard<std::mutex> lock(job->result_mutex);
                job->output_path = TextOf(result.export_path);
                job->error_message = TextOf(result.error_message);
            }
            HsBaFreeSlaPipelineResult(&result);
            break;
        }
        case HSBA_PIPELINE_SLS:
        {
            HsBaSlsPipelineResult_t result =
                HsBaRunSlsPipeline(static_cast<const HsBaSlsPipelineConfig_t*>(config), OnJobProgress, job);
            job->succeeded = result.success != 0;
            job->total_layers = result.total_layers;
            job->elapsed_seconds = result.elapsed_seconds;
            job->geometry_id.store(result.geometry_id);
            {
                const std::lock_guard<std::mutex> lock(job->result_mutex);
                job->output_path = TextOf(result.export_path);
                job->error_message = TextOf(result.error_message);
            }
            HsBaFreeSlsPipelineResult(&result);
            break;
        }
        case HSBA_PIPELINE_SLM:
        {
            HsBaSlmPipelineResult_t result =
                HsBaRunSlmPipeline(static_cast<const HsBaSlmPipelineConfig_t*>(config), OnJobProgress, job);
            job->succeeded = result.success != 0;
            job->total_layers = result.total_layers;
            job->elapsed_seconds = result.elapsed_seconds;
            job->geometry_id.store(result.geometry_id);
            {
                const std::lock_guard<std::mutex> lock(job->result_mutex);
                job->output_path = TextOf(result.export_path);
                job->error_message = TextOf(result.error_message);
            }
            HsBaFreeSlmPipelineResult(&result);
            break;
        }
        case HSBA_PIPELINE_LOM:
        {
            HsBaLomPipelineResult_t result =
                HsBaRunLomPipeline(static_cast<const HsBaLomPipelineConfig_t*>(config), OnJobProgress, job);
            job->succeeded = result.success != 0;
            job->total_layers = result.total_layers;
            job->elapsed_seconds = result.elapsed_seconds;
            job->geometry_id.store(result.geometry_id);
            {
                const std::lock_guard<std::mutex> lock(job->result_mutex);
                job->output_path = TextOf(result.export_path);
                job->error_message = TextOf(result.error_message);
            }
            HsBaFreeLomPipelineResult(&result);
            break;
        }
        case HSBA_PIPELINE_TDP:
        {
            HsBaTdpPipelineResult_t result =
                HsBaRunTdpPipeline(static_cast<const HsBaTdpPipelineConfig_t*>(config), OnJobProgress, job);
            job->succeeded = result.success != 0;
            job->total_layers = result.total_layers;
            job->elapsed_seconds = result.elapsed_seconds;
            job->geometry_id.store(result.geometry_id);
            {
                const std::lock_guard<std::mutex> lock(job->result_mutex);
                job->output_path = TextOf(result.export_path);
                job->error_message = TextOf(result.error_message);
            }
            HsBaFreeTdpPipelineResult(&result);
            break;
        }
        case HSBA_PIPELINE_WAAM:
        {
            HsBaWaamPipelineResult_t result =
                HsBaRunWaamPipeline(static_cast<const HsBaWaamPipelineConfig_t*>(config), OnJobProgress, job);
            job->succeeded = result.success != 0;
            job->total_layers = result.total_layers;
            job->elapsed_seconds = result.elapsed_seconds;
            job->geometry_id.store(result.geometry_id);
            {
                const std::lock_guard<std::mutex> lock(job->result_mutex);
                job->output_path = TextOf(result.output_path);
                job->error_message = TextOf(result.error_message);
            }
            HsBaFreeWaamPipelineResult(&result);
            break;
        }
        case HSBA_PIPELINE_CUSTOM:
        {
            HsBaCustomPipelineResult_t result =
                HsBaRunCustomPipeline(static_cast<const HsBaCustomPipelineConfig_t*>(config), OnJobProgress, job);
            job->succeeded = result.success != 0;
            job->total_layers = result.total_layers;
            job->elapsed_seconds = result.elapsed_seconds;
            job->geometry_id.store(result.geometry_id);
            {
                const std::lock_guard<std::mutex> lock(job->result_mutex);
                job->output_path = TextOf(result.output_path);
                job->inline_content = TextOf(result.result_string);
                job->error_message = TextOf(result.error_message);
            }
            HsBaFreeCustomPipelineResult(&result);
            break;
        }
        case HSBA_PIPELINE_FILETRANSFER:
        {
            HsBaFileTransferPipelineResult_t result = HsBaRunFileTransferPipeline(
                static_cast<const HsBaFileTransferPipelineConfig_t*>(config), OnJobProgress, job);
            job->succeeded = result.success != 0;
            job->files_transferred = result.files_transferred;
            job->total_files = result.total_files;
            job->elapsed_seconds = result.elapsed_seconds;
            {
                const std::lock_guard<std::mutex> lock(job->result_mutex);
                job->error_message = TextOf(result.error_message);
            }
            HsBaFreeFileTransferPipelineResult(&result);
            break;
        }
        default:
            return -1;
        }
    }
    catch (const std::exception& e)
    {
        // The pipeline entries already convert RuntimeError into a failed result; anything reaching
        // here is a bug or a foreign exception, and must not kill the job thread either.
        job->succeeded = false;
        const std::lock_guard<std::mutex> lock(job->result_mutex);
        job->error_message = std::string("Pipeline error: ") + e.what();
        return -1;
    }
    const std::lock_guard<std::mutex> lock(job->result_mutex);
    return job->succeeded ? 1 : 0;
}

/** @brief Drop the reference the library took when the job was submitted; must be the last action. */
void ReleaseLibraryRef(HsBaJob* job)
{
    if (job->refs.fetch_sub(1) == 1)
    {
        delete job;
    }
}

/** @brief Worker entry: run, publish the final state, fire callbacks, then drop the library reference. */
void RunJob(HsBaJob* job)
{
    job->state.store(HSBA_JOB_RUNNING);
    ExecuteKind(job);

    int final_state = HSBA_JOB_FAILED;
    {
        const std::lock_guard<std::mutex> lock(job->result_mutex);
        if (job->succeeded)
        {
            final_state = HSBA_JOB_SUCCEEDED;
        }
        else if (job->cancel.load() || ReportsCancellation(job->error_message))
        {
            final_state = HSBA_JOB_CANCELLED;
            if (job->error_message.empty())
            {
                job->error_message = "Cancelled: pipeline cancelled";
            }
        }
    }
    if (final_state == HSBA_JOB_CANCELLED)
    {
        // A run that stopped early never reported its closing stage, so show the cancellation explicitly.
        const std::lock_guard<std::mutex> lock(job->text_mutex);
        job->stage_text = "Cancelled";
        job->stage.store(HSBA_STAGE_CANCELLED);
    }
    job->state.store(final_state);
    job->wait_cv.notify_all();

    if (job->on_done)
    {
        job->on_done(job, job->done_user_data);
    }
    if (job->on_legacy_done)
    {
        DeliverLegacyResult(job);
    }
    ReleaseLibraryRef(job);
}

/** @brief Start the job thread; detached because the job reference counting owns its lifetime. */
void StartThread(HsBaJob* job)
{
    std::thread([job]() { RunJob(job); }).detach();
}

}  // namespace

/* ============================================================================
 *  ConfigClone
 * ========================================================================== */

template <typename ConfigT>
void ConfigClone::Build(const ConfigT& source, std::initializer_list<const char* ConfigT::*> fields,
                        const void* cancel_flag)
{
    Reset();
    size_bytes = sizeof(ConfigT);
    raw = std::malloc(size_bytes);
    if (!raw)
    {
        return;
    }
    std::memcpy(raw, &source, size_bytes);
    auto* dst = static_cast<ConfigT*>(raw);
    // Resize up front: no reallocation may move a std::string whose c_str() we publish below.
    strings.resize(fields.size());
    std::size_t index = 0;
    for (const char* ConfigT::* field : fields)
    {
        strings[index] = source.*field ? std::string(source.*field) : std::string();
        dst->*field = strings[index].c_str();
        ++index;
    }
    // Every Config struct carries the request flag as its tail field; the job owns the atomic.
    dst->cancel_flag = cancel_flag;
    dst->capture_geometry = source.capture_geometry != 0 ? 1 : 0;
}

void ConfigClone::BuildFileTransfer(const HsBaFileTransferPipelineConfig_t& source, const void* cancel_flag)
{
    Reset();
    size_bytes = sizeof(HsBaFileTransferPipelineConfig_t);
    raw = std::malloc(size_bytes);
    if (!raw)
    {
        return;
    }
    std::memcpy(raw, &source, size_bytes);
    auto* dst = static_cast<HsBaFileTransferPipelineConfig_t*>(raw);
    const int count = source.file_count > 0 ? source.file_count : 0;
    strings.resize(static_cast<std::size_t>(count) + 2);
    strings[0] = source.host ? std::string(source.host) : std::string();
    strings[1] = source.port ? std::string(source.port) : std::string();
    dst->host = strings[0].c_str();
    dst->port = strings[1].c_str();
    if (count > 0 && source.file_paths)
    {
        path_array = std::make_unique<char*[]>(static_cast<std::size_t>(count));
        for (int i = 0; i < count; ++i)
        {
            strings[static_cast<std::size_t>(i) + 2] =
                source.file_paths[i] ? std::string(source.file_paths[i]) : std::string();
            path_array[static_cast<std::size_t>(i)] =
                const_cast<char*>(strings[static_cast<std::size_t>(i) + 2].c_str());
        }
        dst->file_paths = const_cast<const char**>(path_array.get());
    }
    else
    {
        dst->file_paths = nullptr;
        dst->file_count = 0;
    }
    dst->cancel_flag = cancel_flag;
}

void ConfigClone::Reset()
{
    std::free(raw);
    raw = nullptr;
    size_bytes = 0;
    strings.clear();
    path_array.reset();
}

ConfigClone::ConfigClone(ConfigClone&& other) noexcept
    : raw(other.raw), size_bytes(other.size_bytes), strings(std::move(other.strings)),
      path_array(std::move(other.path_array))
{
    other.raw = nullptr;
    other.size_bytes = 0;
}

ConfigClone& ConfigClone::operator=(ConfigClone&& other) noexcept
{
    if (this != &other)
    {
        Reset();
        raw = other.raw;
        size_bytes = other.size_bytes;
        strings = std::move(other.strings);
        path_array = std::move(other.path_array);
        other.raw = nullptr;
        other.size_bytes = 0;
    }
    return *this;
}

namespace
{

/** @brief Deep-copy the configuration of the requested pipeline kind. */
ConfigClone MakeConfigClone(HsBaPipelineKind kind, const void* config, const void* cancel_flag)
{
    ConfigClone clone;
    switch (kind)
    {
    case HSBA_PIPELINE_FDM:
        clone.Build(*static_cast<const HsBaFdmPipelineConfig_t*>(config),
                    {&HsBaFdmPipelineConfig_t::model_name, &HsBaFdmPipelineConfig_t::model_path,
                     &HsBaFdmPipelineConfig_t::support_lua_script, &HsBaFdmPipelineConfig_t::support_lua_func,
                     &HsBaFdmPipelineConfig_t::infill_lua_script, &HsBaFdmPipelineConfig_t::infill_lua_func,
                     &HsBaFdmPipelineConfig_t::output_path},
                    cancel_flag);
        break;
    case HSBA_PIPELINE_SLA:
        clone.Build(*static_cast<const HsBaSlaPipelineConfig_t*>(config),
                    {&HsBaSlaPipelineConfig_t::model_name, &HsBaSlaPipelineConfig_t::model_path,
                     &HsBaSlaPipelineConfig_t::support_lua_script, &HsBaSlaPipelineConfig_t::support_lua_func,
                     &HsBaSlaPipelineConfig_t::floor_lua_script, &HsBaSlaPipelineConfig_t::floor_lua_func,
                     &HsBaSlaPipelineConfig_t::export_lua_script, &HsBaSlaPipelineConfig_t::export_lua_func,
                     &HsBaSlaPipelineConfig_t::output_path},
                    cancel_flag);
        break;
    case HSBA_PIPELINE_SLS:
        clone.Build(*static_cast<const HsBaSlsPipelineConfig_t*>(config),
                    {&HsBaSlsPipelineConfig_t::model_name, &HsBaSlsPipelineConfig_t::model_path,
                     &HsBaSlsPipelineConfig_t::export_lua_script, &HsBaSlsPipelineConfig_t::export_lua_func,
                     &HsBaSlsPipelineConfig_t::output_path},
                    cancel_flag);
        break;
    case HSBA_PIPELINE_SLM:
        clone.Build(*static_cast<const HsBaSlmPipelineConfig_t*>(config),
                    {&HsBaSlmPipelineConfig_t::model_name, &HsBaSlmPipelineConfig_t::model_path,
                     &HsBaSlmPipelineConfig_t::export_lua_script, &HsBaSlmPipelineConfig_t::export_lua_func,
                     &HsBaSlmPipelineConfig_t::output_path},
                    cancel_flag);
        break;
    case HSBA_PIPELINE_LOM:
        clone.Build(*static_cast<const HsBaLomPipelineConfig_t*>(config),
                    {&HsBaLomPipelineConfig_t::model_name, &HsBaLomPipelineConfig_t::model_path,
                     &HsBaLomPipelineConfig_t::export_lua_script, &HsBaLomPipelineConfig_t::export_lua_func,
                     &HsBaLomPipelineConfig_t::output_path},
                    cancel_flag);
        break;
    case HSBA_PIPELINE_TDP:
        clone.Build(*static_cast<const HsBaTdpPipelineConfig_t*>(config),
                    {&HsBaTdpPipelineConfig_t::model_name, &HsBaTdpPipelineConfig_t::model_path,
                     &HsBaTdpPipelineConfig_t::export_lua_script, &HsBaTdpPipelineConfig_t::export_lua_func,
                     &HsBaTdpPipelineConfig_t::output_path},
                    cancel_flag);
        break;
    case HSBA_PIPELINE_WAAM:
        clone.Build(*static_cast<const HsBaWaamPipelineConfig_t*>(config),
                    {&HsBaWaamPipelineConfig_t::model_name, &HsBaWaamPipelineConfig_t::model_path,
                     &HsBaWaamPipelineConfig_t::path_lua_script, &HsBaWaamPipelineConfig_t::path_lua_func,
                     &HsBaWaamPipelineConfig_t::output_path},
                    cancel_flag);
        break;
    case HSBA_PIPELINE_CUSTOM:
        clone.Build(*static_cast<const HsBaCustomPipelineConfig_t*>(config),
                    {&HsBaCustomPipelineConfig_t::pipeline_lua_script, &HsBaCustomPipelineConfig_t::pipeline_lua_source,
                     &HsBaCustomPipelineConfig_t::entry_func, &HsBaCustomPipelineConfig_t::config_json,
                     &HsBaCustomPipelineConfig_t::model_name, &HsBaCustomPipelineConfig_t::model_path,
                     &HsBaCustomPipelineConfig_t::output_path},
                    cancel_flag);
        break;
    case HSBA_PIPELINE_FILETRANSFER:
        clone.BuildFileTransfer(*static_cast<const HsBaFileTransferPipelineConfig_t*>(config), cancel_flag);
        break;
    default:
        break;
    }
    return clone;
}

}  // namespace

/* ============================================================================
 *  Legacy HsBaRun*PipelineAsync bridge
 * ========================================================================== */

namespace
{

/** @brief Rebuild the per-kind result struct of a finished job and hand it to the legacy thunk. */
void DeliverLegacyResult(HsBaJob* job)
{
    // The thunk expects a result of the job's own kind, so the per-kind fields are filled one by one.
    std::string error;
    std::string artifact;
    std::string extra;
    {
        const std::lock_guard<std::mutex> lock(job->result_mutex);
        error = job->error_message;
        artifact = job->output_path;
        extra = job->inline_content;
    }
    const int success = job->succeeded ? 1 : 0;
    const double elapsed = job->elapsed_seconds;
    const int layers = job->total_layers;
    const long long geometry = job->geometry_id.load();

    switch (job->kind)
    {
    case HSBA_PIPELINE_FDM:
    {
        HsBaFdmPipelineResult_t* result = new HsBaFdmPipelineResult_t{};
        result->success = success;
        result->total_layers = layers;
        result->gcode_content = DupText(extra);
        result->error_message = DupText(error);
        result->elapsed_seconds = elapsed;
        result->geometry_id = geometry;
        job->on_legacy_done(result, job->legacy_thunk);
        HsBaFreePipelineResult(result);
        delete result;
        break;
    }
    case HSBA_PIPELINE_SLA:
    {
        HsBaSlaPipelineResult_t* result = new HsBaSlaPipelineResult_t{};
        result->success = success;
        result->total_layers = layers;
        result->export_path = DupText(artifact);
        result->error_message = DupText(error);
        result->elapsed_seconds = elapsed;
        result->geometry_id = geometry;
        job->on_legacy_done(result, job->legacy_thunk);
        HsBaFreeSlaPipelineResult(result);
        delete result;
        break;
    }
    case HSBA_PIPELINE_SLS:
    {
        HsBaSlsPipelineResult_t* result = new HsBaSlsPipelineResult_t{};
        result->success = success;
        result->total_layers = layers;
        result->export_path = DupText(artifact);
        result->error_message = DupText(error);
        result->elapsed_seconds = elapsed;
        result->geometry_id = geometry;
        job->on_legacy_done(result, job->legacy_thunk);
        HsBaFreeSlsPipelineResult(result);
        delete result;
        break;
    }
    case HSBA_PIPELINE_SLM:
    {
        HsBaSlmPipelineResult_t* result = new HsBaSlmPipelineResult_t{};
        result->success = success;
        result->total_layers = layers;
        result->export_path = DupText(artifact);
        result->error_message = DupText(error);
        result->elapsed_seconds = elapsed;
        result->geometry_id = geometry;
        job->on_legacy_done(result, job->legacy_thunk);
        HsBaFreeSlmPipelineResult(result);
        delete result;
        break;
    }
    case HSBA_PIPELINE_LOM:
    {
        HsBaLomPipelineResult_t* result = new HsBaLomPipelineResult_t{};
        result->success = success;
        result->total_layers = layers;
        result->export_path = DupText(artifact);
        result->error_message = DupText(error);
        result->elapsed_seconds = elapsed;
        result->geometry_id = geometry;
        job->on_legacy_done(result, job->legacy_thunk);
        HsBaFreeLomPipelineResult(result);
        delete result;
        break;
    }
    case HSBA_PIPELINE_TDP:
    {
        HsBaTdpPipelineResult_t* result = new HsBaTdpPipelineResult_t{};
        result->success = success;
        result->total_layers = layers;
        result->export_path = DupText(artifact);
        result->error_message = DupText(error);
        result->elapsed_seconds = elapsed;
        result->geometry_id = geometry;
        job->on_legacy_done(result, job->legacy_thunk);
        HsBaFreeTdpPipelineResult(result);
        delete result;
        break;
    }
    case HSBA_PIPELINE_WAAM:
    {
        HsBaWaamPipelineResult_t* result = new HsBaWaamPipelineResult_t{};
        result->success = success;
        result->total_layers = layers;
        result->output_path = DupText(artifact);
        result->error_message = DupText(error);
        result->elapsed_seconds = elapsed;
        result->geometry_id = geometry;
        job->on_legacy_done(result, job->legacy_thunk);
        HsBaFreeWaamPipelineResult(result);
        delete result;
        break;
    }
    case HSBA_PIPELINE_CUSTOM:
    {
        HsBaCustomPipelineResult_t* result = new HsBaCustomPipelineResult_t{};
        result->success = success;
        result->total_layers = layers;
        result->output_path = DupText(artifact);
        result->result_string = DupText(extra);
        result->error_message = DupText(error);
        result->elapsed_seconds = elapsed;
        result->geometry_id = geometry;
        job->on_legacy_done(result, job->legacy_thunk);
        HsBaFreeCustomPipelineResult(result);
        delete result;
        break;
    }
    case HSBA_PIPELINE_FILETRANSFER:
    {
        HsBaFileTransferPipelineResult_t* result = new HsBaFileTransferPipelineResult_t{};
        result->success = success;
        result->files_transferred = job->files_transferred;
        result->total_files = job->total_files;
        result->error_message = DupText(error);
        result->elapsed_seconds = elapsed;
        job->on_legacy_done(result, job->legacy_thunk);
        HsBaFreeFileTransferPipelineResult(result);
        delete result;
        break;
    }
    default:
        break;
    }
}

}  // namespace

void SubmitLegacyJob(HsBaPipelineKind kind, const void* config, HsBaProgressCallback progress, void* progress_user_data,
                     LegacyResultThunk on_done, void* thunk)
{
    if (!IsValidKind(kind) || !config)
    {
        // Report the failure through the same callback contract, with no result to free.
        if (on_done)
        {
            on_done(nullptr, thunk);
        }
        return;
    }
    auto* job = new HsBaJob(kind);
    job->config = MakeConfigClone(kind, config, &job->cancel);
    job->on_legacy_done = on_done;
    job->legacy_thunk = thunk;
    // The legacy callback signature matches HsBaProgressCallback, so OnJobProgress forwards to it.
    job->legacy_progress = progress;
    job->legacy_progress_user_data = progress_user_data;
    if (!job->config.Get())
    {
        delete job;
        if (on_done)
        {
            on_done(nullptr, thunk);
        }
        return;
    }
    StartThread(job);
}

/* ============================================================================
 *  Public C ABI
 * ========================================================================== */

}  // namespace HsBa::Slicer::Pipeline

// The C ABI layer below is deliberately outside the namespace (the published symbols are
// extern "C"), so it reaches the job machinery through this using-directive.
using namespace HsBa::Slicer::Pipeline;

extern "C"
{

    HSBA_SLICER_API HsBaJobHandle HsBaSubmitJob(HsBaPipelineKind kind, const void* config,
                                                HsBaProgressCallbackEx on_progress, void* progress_user_data,
                                                HsBaJobDoneCallback on_done, void* done_user_data)
    {
        try
        {
            if (!config || !IsValidKind(kind))
            {
                return nullptr;
            }
            auto* job = new HsBaJob(kind);
            job->config = MakeConfigClone(kind, config, &job->cancel);
            job->on_progress = on_progress;
            job->progress_user_data = progress_user_data;
            job->on_done = on_done;
            job->done_user_data = done_user_data;
            if (!job->config.Get())
            {
                delete job;
                return nullptr;
            }
            StartThread(job);
            return job;
        }
        catch (const HsBa::Slicer::RuntimeError&)
        {
            // NOLINTNEXTLINE(bugprone-empty-catch) C ABI: swallow, do not propagate across C boundary
            return nullptr;
        }
        catch (const std::bad_alloc&)
        {
            return nullptr;
        }
    }

    HSBA_SLICER_API int HsBaCancelJob(HsBaJobHandle job)
    {
        if (!job)
        {
            return 0;
        }
        if (IsFinalState(job->state.load()))
        {
            return 0;
        }
        job->cancel.store(true);
        return 1;
    }

    HSBA_SLICER_API int HsBaWaitJob(HsBaJobHandle job, long long timeout_ms)
    {
        if (!job)
        {
            return 0;
        }
        std::unique_lock<std::mutex> lock(job->wait_mutex);
        const auto finalised = [job]() { return IsFinalState(job->state.load()); };
        if (timeout_ms < 0)
        {
            job->wait_cv.wait(lock, finalised);
            return 1;
        }
        return job->wait_cv.wait_for(lock, std::chrono::milliseconds(timeout_ms), finalised) ? 1 : 0;
    }

    HSBA_SLICER_API HsBaJobState_t HsBaJobGetState(HsBaJobHandle job)
    {
        if (!job)
        {
            return HSBA_JOB_FAILED;
        }
        return static_cast<HsBaJobState_t>(job->state.load());
    }

    HSBA_SLICER_API int HsBaJobGetProgress(HsBaJobHandle job, int* percent, HsBaStage_t* stage, char* text_out,
                                           int text_cap)
    {
        if (!job)
        {
            return 0;
        }
        if (percent)
        {
            *percent = job->percent.load();
        }
        if (stage)
        {
            *stage = static_cast<HsBaStage_t>(job->stage.load());
        }
        if (text_out && text_cap > 0)
        {
            const std::lock_guard<std::mutex> lock(job->text_mutex);
            const std::size_t copied =
                std::min<std::size_t>(job->stage_text.size(), static_cast<std::size_t>(text_cap - 1));
            std::memcpy(text_out, job->stage_text.data(), copied);
            text_out[copied] = '\0';
        }
        return 1;
    }

    HSBA_SLICER_API long long HsBaJobGeometryId(HsBaJobHandle job)
    {
        return job ? job->geometry_id.load() : 0;
    }

    HSBA_SLICER_API int HsBaJobCopyResult(HsBaJobHandle job, HsBaJobResultView_t* out)
    {
        if (!job || !out)
        {
            return 0;
        }
        std::memset(out, 0, sizeof(HsBaJobResultView_t));
        if (!IsFinalState(job->state.load()))
        {
            return 0;
        }
        const std::lock_guard<std::mutex> lock(job->result_mutex);
        out->success = job->succeeded ? 1 : 0;
        out->total_layers = job->total_layers;
        out->geometry_id = job->geometry_id.load();
        out->elapsed_seconds = job->elapsed_seconds;
        out->files_transferred = job->files_transferred;
        out->total_files = job->total_files;
        out->inline_content = DupText(job->inline_content);
        out->output_path = DupText(job->output_path);
        out->error_message = DupText(job->error_message);
        return 1;
    }

    HSBA_SLICER_API void HsBaFreeJobResultView(HsBaJobResultView_t* view)
    {
        if (!view)
        {
            return;
        }
        std::free(std::exchange(view->inline_content, nullptr));
        std::free(std::exchange(view->output_path, nullptr));
        std::free(std::exchange(view->error_message, nullptr));
        std::memset(view, 0, sizeof(HsBaJobResultView_t));
    }

    HSBA_SLICER_API void HsBaReleaseJob(HsBaJobHandle job)
    {
        if (!job)
        {
            return;
        }
        if (job->refs.fetch_sub(1) == 1)
        {
            delete job;
        }
    }

}  // extern "C"
