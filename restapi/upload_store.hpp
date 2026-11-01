/** @file upload_store.hpp
 * @brief Retention pool for model files uploaded through multipart job submissions.
 * @author HsBa
 *
 * The pool owns two things: the on-disk files under <dir>/<job-id>/ and an
 * in-memory registry describing them. Its retention schedule is deliberately
 * independent of JobManager's result_ttl_seconds: a slicing job can run for many
 * hours, and a finished job is often re-run from the same uploaded model, so
 * reaping a job record must never delete its source file.
 *
 * Reclaim policy comes from the [upload] ini section (see UploadOptions):
 *   ttl    - age-based only
 *   count  - oldest-first once max_files is exceeded
 *   both   - apply ttl and count together (default)
 *   manual - only DELETE /api/uploads/{id} removes files
 * Entries referenced by a queued or running job (in_use) are never reclaimed;
 * their retention clock starts when the job reaches a terminal state.
 *
 * UploadInfo is an aggregate of std::string / long long so struct_json and
 * struct_xml can render it directly, and it lives in this named namespace (not an
 * anonymous one) to avoid MSVC's PFR local-type error C7631.
 */
#pragma once
#ifndef HSBA_SLICER_RESTAPI_UPLOAD_STORE_HPP
#define HSBA_SLICER_RESTAPI_UPLOAD_STORE_HPP

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace HsBa::Slicer::RestApi
{
/** @brief Retention knobs of the upload pool (mirrors the [upload] ini section). */
struct UploadOptions
{
    std::string dir;                 ///< Root directory; empty disables the pool entirely.
    int max_files = 256;             ///< Retained entries; the config loader floors it at HSBA_MODEL_POOL_SIZE.
    long long ttl_seconds = 604800;  ///< Retention after the owning job finishes; 0 = never expires by age.
    std::string evict_mode = "both"; ///< "ttl" | "count" | "both" | "manual".
    long long max_total_mb = 0;      ///< Directory-wide size cap in MB; 0 = unlimited.
    int sweep_interval_seconds = 60; ///< Background sweep period.
};

/** @brief Serializable view of one pooled upload (GET /api/uploads). */
struct UploadInfo
{
    std::string id;                  ///< Pool id ("upl-<n>").
    std::string job_id;              ///< Owning job ("job-<n>"), empty once unlinked.
    std::string file_name;           ///< Sanitized name on disk.
    std::string original_name;       ///< Client-supplied name (pre-sanitizing).
    std::string path;                ///< Server-side path of the stored file.
    std::string state;               ///< "pending" | "in_use" | "terminal".
    long long size_bytes = 0;        ///< File size in bytes.
    long long created_at = 0;        ///< Unix epoch seconds.
    long long expires_at = 0;        ///< Unix epoch seconds; 0 = never expires by age.
    long long remaining_seconds = 0; ///< Seconds until expires_at; 0 when unlimited or already expired.
};

/** @brief Outcome of a DELETE /api/uploads/{id} request. */
enum class UploadRemoveResult
{
    Removed,   ///< File and registry entry deleted.
    NotFound,  ///< No such pool id.
    InUse,     ///< Referenced by a queued/running job (HTTP 409).
};

/**
 * @brief Thread-safe registry plus reclaim timer for uploaded model files.
 *
 * File system work always happens outside the registry mutex: entries chosen for
 * eviction are moved into a local list first, then unlinked once the lock is gone.
 */
class UploadStore
{
public:
    /** @brief Configure the pool, create the root directory and rebuild the registry from disk. */
    explicit UploadStore(UploadOptions options);
    ~UploadStore();

    UploadStore(const UploadStore&) = delete;
    UploadStore& operator=(const UploadStore&) = delete;

    /** @brief Whether [upload] dir was configured (multipart submissions are refused otherwise). */
    bool Enabled() const noexcept { return enabled_; }

    /**
     * @brief Register a freshly written file and, if needed, reclaim older entries.
     * @param job_id        Job the file belongs to (already reserved through JobManager::NextId).
     * @param original_name Client-supplied name.
     * @param safe_name     Sanitized on-disk name.
     * @param path          Full server-side path.
     * @param size          File size in bytes (0 when unknown).
     * @return New pool id, or an empty string when the pool is disabled or full.
     */
    std::string Adopt(const std::string& job_id, const std::string& original_name, const std::string& safe_name,
                      const std::string& path, long long size);

    /** @brief Mark the entry owned by @p job_id as referenced (starts ignoring the age clock). */
    void MarkInUse(const std::string& job_id);

    /** @brief Mark the entry owned by @p job_id as finished; its retention clock starts now. */
    void MarkTerminal(const std::string& job_id);

    /** @brief Drop the job association but keep the file following the pool's own schedule. */
    void Unlink(const std::string& job_id);

    /**
     * @brief Snapshot pooled uploads, oldest first.
     * @param state_filter "pending" | "in_use" | "terminal"; empty = all.
     * @param offset       Skip this many matching entries.
     * @param limit        Max entries to return (0 = no cap).
     */
    std::vector<UploadInfo> List(const std::string& state_filter, std::size_t offset, std::size_t limit) const;

    /** @brief Snapshot one upload; false when @p id is unknown. */
    bool Get(const std::string& id, UploadInfo& out) const;

    /** @brief Delete one upload (refused while a job still references it). */
    UploadRemoveResult Remove(const std::string& id);

    /** @brief Apply the reclaim policy once (called by the sweep thread and by Adopt). */
    void Sweep();

    /**
     * @brief Rebuild the registry from <dir>/job-* on startup.
     *
     * Files found on disk are adopted as "pending" with their directory's mtime as
     * creation time, so a restart neither loses track of retained models nor keeps
     * already expired ones forever.
     */
    void ReconcileDisk();

private:
    /** @brief Internal registry record. */
    struct Entry
    {
        std::string id;
        std::string job_id;
        std::string file_name;
        std::string original_name;
        std::string path;
        long long size_bytes = 0;
        long long created_at = 0;  ///< Epoch seconds.
        long long terminal_at = 0; ///< Epoch seconds; 0 while the job has not finished.
        bool in_use = false;       ///< Referenced by a queued/running job.
    };

    void SweepLoop();
    /** @brief Choose entries to drop (removing them from the map); caller owns the returned files. */
    std::vector<Entry> EvictLocked(long long now, std::size_t incoming_count, long long incoming_bytes);
    /** @brief Total size tracked by the registry; caller holds the mutex. */
    long long TotalBytesLocked() const;
    void RemoveFiles(const std::vector<Entry>& entries);
    UploadInfo ToInfo(const Entry& entry) const;
    bool ByTime() const;
    bool ByCount() const;

    UploadOptions options_;
    bool enabled_ = false;
    mutable std::mutex mtx_;
    std::condition_variable cv_;
    std::unordered_map<std::string, Entry> entries_;
    unsigned long long next_id_ = 0;
    bool stop_ = false;
    std::thread sweeper_;
};

}  // namespace HsBa::Slicer::RestApi

#endif  // HSBA_SLICER_RESTAPI_UPLOAD_STORE_HPP
