/** @file job_control.h
 * @brief Opaque job handle that runs a pipeline on a library thread and can be cancelled.
 *
 * The nine HsBaRun*Pipeline entry points are synchronous: they return only when the
 * slice finished, and the nine HsBaRun*PipelineAsync entry points have always blocked
 * their caller as well, because the coroutine executor they used joined before
 * returning. HsBaSubmitJob is the real non-blocking interface: it copies the config
 * (including every string, so the caller's buffers may be freed right after the
 * call), starts a dedicated thread and hands back a job handle.
 *
 * Threading contract: the progress and completion callbacks run on the job's own
 * thread, never on the submitting thread, so a GUI must marshal into its UI thread
 * (Qt: signal/slot or QMetaObject::invokeMethod, wxWidgets: CallAfter).
 *
 * Lifetime contract: a submitted job holds two references, one for the running work
 * and one for the caller. The completion callback drops the library reference, so a
 * caller that never touches the handle again does not have to release it; a caller
 * that keeps the handle (to poll from another thread, for instance) must call
 * HsBaReleaseJob exactly once when done, otherwise the job leaks.
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_JOB_CONTROL_H
#define HSBA_SLICER_JOB_CONTROL_H

#include "dllexport.h"
#include "pipelinetypes/pipeline_types.h"

#ifdef __cplusplus
extern "C"
{
#endif  // __cplusplus

    /** @brief Opaque handle of a submitted pipeline job. */
    typedef struct HsBaJob* HsBaJobHandle;

    /** @brief Lifecycle state of a job. */
    typedef enum HsBaJobState
    {
        HSBA_JOB_PENDING = 0,    ///< Queued, the worker thread has not started the pipeline yet
        HSBA_JOB_RUNNING = 1,    ///< Pipeline executing
        HSBA_JOB_SUCCEEDED = 2,  ///< Pipeline finished without error
        HSBA_JOB_FAILED = 3,     ///< Pipeline stopped with an error (see the result view)
        HSBA_JOB_CANCELLED = 4   ///< Pipeline stopped because cancellation was requested
    } HsBaJobState_t;

    /**
     * @brief Structured progress callback of a job.
     * @param percent Progress percentage (0-100).
     * @param stage Classified stage, HSBA_STAGE_UNKNOWN for text the library does not know.
     * @param stage_text Original stage description (UTF-8); owned by the library, valid only during the call.
     * @param user_data Pointer handed to HsBaSubmitJob.
     */
    typedef void (*HsBaProgressCallbackEx)(int percent, HsBaStage_t stage, const char* stage_text, void* user_data);

    /**
     * @brief Completion callback of a job, invoked on the job thread.
     * @param job Finished job; reading its result is safe, releasing the handle is not required.
     * @param user_data Pointer handed to HsBaSubmitJob.
     */
    typedef void (*HsBaJobDoneCallback)(HsBaJobHandle job, void* user_data);

    /**
     * @brief Start a pipeline on a dedicated library thread.
     * @param kind Which pipeline to run.
     * @param config Pointer to the matching HsBa*PipelineConfig_t; copied before the call returns,
     *               so it must not be NULL but the caller keeps ownership.
     * @param on_progress Structured progress callback (may be NULL); runs on the job thread.
     * @param progress_user_data Passed to @p on_progress.
     * @param on_done Completion callback (may be NULL); runs on the job thread.
     * @param done_user_data Passed to @p on_done.
     * @return Job handle, NULL when kind or config is invalid.
     */
    HSBA_SLICER_API HsBaJobHandle HsBaSubmitJob(HsBaPipelineKind kind, const void* config,
                                                HsBaProgressCallbackEx on_progress, void* progress_user_data,
                                                HsBaJobDoneCallback on_done, void* done_user_data);

    /**
     * @brief Request cancellation of a running job.
     *
     * Cancellation is cooperative: the pipeline notices the request at the next stage
     * boundary, and additionally inside the serial layer loops and export loops. A long
     * single step (mesh loading, Lua callbacks) still has to finish first.
     * @param job Job handle.
     * @return 1 when the request was recorded, 0 when the job had already reached a final state.
     */
    HSBA_SLICER_API int HsBaCancelJob(HsBaJobHandle job);

    /**
     * @brief Block until the job reaches a final state.
     * @param job Job handle.
     * @param timeout_ms Maximum wait in milliseconds, negative to wait without limit.
     * @return 1 when the job is final, 0 on timeout or an invalid handle.
     */
    HSBA_SLICER_API int HsBaWaitJob(HsBaJobHandle job, long long timeout_ms);

    /**
     * @brief Current lifecycle state.
     * @param job Job handle.
     * @return Job state, HSBA_JOB_FAILED for an invalid handle.
     */
    HSBA_SLICER_API HsBaJobState_t HsBaJobGetState(HsBaJobHandle job);

    /**
     * @brief Poll progress without installing a callback, for hosts that tick a timer instead.
     * @param job Job handle.
     * @param percent Destination percentage, may be NULL.
     * @param stage Destination stage enumerator, may be NULL.
     * @param text_out Destination buffer for the stage text (UTF-8), may be NULL.
     * @param text_cap Size of @p text_out in bytes; a trailing NUL is always written.
     * @return 1 on success, 0 for an invalid handle.
     */
    HSBA_SLICER_API int HsBaJobGetProgress(HsBaJobHandle job, int* percent, HsBaStage_t* stage, char* text_out,
                                           int text_cap);

    /**
     * @brief Snapshot id of the job's layer geometry, once the job is final.
     * @param job Job handle.
     * @return geometry_id suitable for HsBaGetLayerGeometry, 0 when nothing was captured.
     */
    HSBA_SLICER_API long long HsBaJobGeometryId(HsBaJobHandle job);

    /**
     * @brief Copy the job result into a caller-owned view.
     * @param job Job handle of a final job.
     * @param out Destination view; its three strings are malloc'ed and must be released with
     *            HsBaFreeJobResultView. Left zeroed when the job is still running.
     * @return 1 when a result was available, 0 for an invalid or unfinished job.
     */
    HSBA_SLICER_API int HsBaJobCopyResult(HsBaJobHandle job, HsBaJobResultView_t* out);

    /**
     * @brief Release the strings of a view obtained from HsBaJobCopyResult and zero the struct.
     * @param view View to release; may be NULL.
     */
    HSBA_SLICER_API void HsBaFreeJobResultView(HsBaJobResultView_t* view);

    /**
     * @brief Drop one reference to a job handle.
     *
     * Call once per handle you kept after the completion callback, and never after the
     * handle was already released. The last reference destroys the job; its result view
     * and geometry snapshot are released with it.
     * @param job Job handle to release; NULL is a no-op.
     */
    HSBA_SLICER_API void HsBaReleaseJob(HsBaJobHandle job);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#ifdef __cplusplus
/* ============================================================================
 *  Internal bridge used by the legacy HsBaRun*PipelineAsync entry points.
 *  Not part of the published C ABI: the per-kind result type differs, so the
 *  caller receives the finished result through a typed thunk.
 * ========================================================================== */
namespace HsBa::Slicer::Pipeline
{

/** @brief Receives a pointer to the per-kind result struct of a finished legacy async job, or NULL
 *         when the job could not be created (the thunk is passed either way, so it is always released). */
using LegacyResultThunk = void (*)(void* typed_result, void* thunk);

/**
 * @brief Run a pipeline through the job machinery for the legacy async C entries.
 * @param kind Pipeline selector.
 * @param config Per-kind configuration, deep-copied before the call returns.
 * @param progress Legacy unstructured progress callback (may be NULL).
 * @param progress_user_data Passed to @p progress.
 * @param on_done Invoked on the job thread with a heap result the thunk must free.
 * @param thunk Passed to @p on_done and owned by the caller.
 */
void SubmitLegacyJob(HsBaPipelineKind kind, const void* config, HsBaProgressCallback progress,
                     void* progress_user_data, LegacyResultThunk on_done, void* thunk);

}  // namespace HsBa::Slicer::Pipeline
#endif  // __cplusplus

#endif  // !HSBA_SLICER_JOB_CONTROL_H
