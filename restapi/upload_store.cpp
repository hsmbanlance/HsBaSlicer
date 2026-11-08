/** @file upload_store.cpp
 * @brief Implements the upload retention pool; see upload_store.hpp.
 * @author HsBa
 */
#include "upload_store.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <system_error>
#include <utility>

namespace HsBa::Slicer::RestApi
{
namespace
{
long long NowSeconds()
{
    const auto since_epoch = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::seconds>(since_epoch).count();
}

std::string Utf8(const std::filesystem::path& path)
{
    const std::u8string utf8 = path.u8string();
    return std::string(utf8.begin(), utf8.end());
}

// last_write_time() lives on an implementation-defined clock; anchoring it to
// system_clock via the two clocks' "now" values keeps the conversion portable
// without relying on std::chrono::clock_cast availability.
long long ToEpochSeconds(std::filesystem::file_time_type stamp)
{
    using namespace std::chrono;
    const auto delta = stamp - std::filesystem::file_time_type::clock::now();
    const auto system_stamp = system_clock::now() + duration_cast<system_clock::duration>(delta);
    return duration_cast<seconds>(system_stamp.time_since_epoch()).count();
}
}  // namespace

UploadStore::UploadStore(UploadOptions options) : options_(std::move(options))
{
    enabled_ = !options_.dir.empty();
    if (!enabled_)
    {
        return;
    }
    std::error_code ec;
    const std::filesystem::path root = std::filesystem::u8path(options_.dir);
    std::filesystem::create_directories(root, ec);
    if (ec)
    {
        // An unusable root behaves like a disabled pool: multipart submits answer
        // 503 instead of failing later on the first write.
        enabled_ = false;
        return;
    }
    ReconcileDisk();
    sweeper_ = std::thread([this] { SweepLoop(); });
}

UploadStore::~UploadStore()
{
    {
        std::lock_guard<std::mutex> lk(mtx_);
        stop_ = true;
    }
    cv_.notify_all();
    if (sweeper_.joinable())
    {
        sweeper_.join();
    }
}

bool UploadStore::ByTime() const
{
    return options_.evict_mode == "ttl" || options_.evict_mode == "both";
}

bool UploadStore::ByCount() const
{
    return options_.evict_mode == "count" || options_.evict_mode == "both";
}

std::string UploadStore::Adopt(const std::string& job_id, const std::string& original_name,
                               const std::string& safe_name, const std::string& path, long long size)
{
    if (!enabled_)
    {
        return {};
    }
    // Reclaim first so a full pool can admit the newcomer by dropping the oldest
    // unreferenced entry instead of turning the request away.
    Sweep();

    std::error_code ec;
    const std::filesystem::path target = std::filesystem::u8path(path);
    if (!target.parent_path().empty())
    {
        std::filesystem::create_directories(target.parent_path(), ec);
    }

    std::vector<Entry> evicted;
    Entry entry;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        const long long now = NowSeconds();
        const long long cap_bytes = options_.max_total_mb * 1024 * 1024;
        const std::size_t cap_files = static_cast<std::size_t>(std::max(1, options_.max_files));

        std::size_t evictable = 0;
        long long evictable_bytes = 0;
        for (const auto& [id, e] : entries_)
        {
            if (!e.in_use)
            {
                ++evictable;
                evictable_bytes += e.size_bytes;
            }
        }

        const bool need_count = ByCount() && entries_.size() + 1 > cap_files;
        const bool need_size = ByTime() && cap_bytes > 0 && TotalBytesLocked() + size > cap_bytes;
        // Refuse when the pool cannot make room: either every entry is referenced by
        // a live job, or dropping them all still would not cover the newcomer.
        if (need_count && evictable == 0)
        {
            return {};
        }
        if (need_size && TotalBytesLocked() + size - evictable_bytes > cap_bytes)
        {
            return {};
        }

        evicted = EvictLocked(now, 1, size);
        entry.id = "upl-" + std::to_string(++next_id_);
        entry.job_id = job_id;
        entry.file_name = safe_name;
        entry.original_name = original_name;
        entry.path = path;
        entry.size_bytes = size;
        entry.created_at = now;
        entries_.emplace(entry.id, entry);
    }
    RemoveFiles(evicted);
    return entry.id;
}

void UploadStore::MarkInUse(const std::string& job_id)
{
    if (job_id.empty())
    {
        return;
    }
    std::lock_guard<std::mutex> lk(mtx_);
    for (auto& [id, entry] : entries_)
    {
        if (entry.job_id == job_id)
        {
            entry.in_use = true;
        }
    }
}

void UploadStore::MarkTerminal(const std::string& job_id)
{
    if (job_id.empty())
    {
        return;
    }
    const long long now = NowSeconds();
    std::lock_guard<std::mutex> lk(mtx_);
    for (auto& [id, entry] : entries_)
    {
        if (entry.job_id == job_id)
        {
            entry.in_use = false;
            // The retention clock starts here, not at upload time: a job may run for
            // hours and must never lose the model it is slicing.
            entry.terminal_at = now;
        }
    }
}

void UploadStore::Unlink(const std::string& job_id)
{
    if (job_id.empty())
    {
        return;
    }
    std::lock_guard<std::mutex> lk(mtx_);
    for (auto& [id, entry] : entries_)
    {
        if (entry.job_id == job_id)
        {
            entry.job_id.clear();
        }
    }
}

void UploadStore::ReleaseByJob(const std::string& job_id)
{
    if (job_id.empty())
    {
        return;
    }
    const long long now = NowSeconds();
    std::lock_guard<std::mutex> lk(mtx_);
    for (auto& [id, entry] : entries_)
    {
        if (entry.job_id == job_id)
        {
            entry.in_use = false;
            // The owning job is gone and never reported a terminal state, so the retention clock
            // starts here; the association is dropped as well because no future lookup can reach
            // this job id once its record has been erased.
            if (entry.terminal_at == 0)
            {
                entry.terminal_at = now;
            }
            entry.job_id.clear();
        }
    }
}

std::vector<UploadInfo> UploadStore::List(const std::string& state_filter, std::size_t offset, std::size_t limit) const
{
    std::vector<UploadInfo> matched;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        matched.reserve(entries_.size());
        for (const auto& [id, entry] : entries_)
        {
            UploadInfo info = ToInfo(entry);
            if (!state_filter.empty() && info.state != state_filter)
            {
                continue;
            }
            matched.push_back(std::move(info));
        }
    }
    std::sort(matched.begin(), matched.end(),
              [](const UploadInfo& a, const UploadInfo& b)
              {
                  if (a.created_at != b.created_at)
                  {
                      return a.created_at < b.created_at;
                  }
                  return a.id < b.id;
              });
    if (offset >= matched.size())
    {
        matched.clear();
        return matched;
    }
    matched.erase(matched.begin(), matched.begin() + static_cast<std::ptrdiff_t>(offset));
    if (limit != 0 && matched.size() > limit)
    {
        matched.resize(limit);
    }
    return matched;
}

bool UploadStore::Get(const std::string& id, UploadInfo& out) const
{
    std::lock_guard<std::mutex> lk(mtx_);
    const auto it = entries_.find(id);
    if (it == entries_.end())
    {
        return false;
    }
    out = ToInfo(it->second);
    return true;
}

UploadRemoveResult UploadStore::Remove(const std::string& id)
{
    Entry victim;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        const auto it = entries_.find(id);
        if (it == entries_.end())
        {
            return UploadRemoveResult::NotFound;
        }
        if (it->second.in_use)
        {
            return UploadRemoveResult::InUse;
        }
        victim = it->second;
        entries_.erase(it);
    }
    RemoveFiles({victim});
    return UploadRemoveResult::Removed;
}

void UploadStore::Sweep()
{
    if (!enabled_)
    {
        return;
    }
    std::vector<Entry> evicted;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        evicted = EvictLocked(NowSeconds(), 0, 0);
    }
    RemoveFiles(evicted);
}

void UploadStore::ReconcileDisk()
{
    std::error_code ec;
    const std::filesystem::path root = std::filesystem::u8path(options_.dir);
    std::vector<Entry> adopted;
    for (const auto& job_dir : std::filesystem::directory_iterator(root, ec))
    {
        if (ec)
        {
            break;
        }
        if (!job_dir.is_directory(ec))
        {
            continue;
        }
        const std::string job_id = Utf8(job_dir.path().filename());
        // Only directories this pool created are adopted; anything else in the root
        // belongs to the operator.
        if (!job_id.starts_with("job-"))
        {
            continue;
        }
        const long long created = ToEpochSeconds(job_dir.last_write_time(ec));
        if (ec)
        {
            continue;
        }
        for (const auto& file : std::filesystem::directory_iterator(job_dir.path(), ec))
        {
            if (ec)
            {
                break;
            }
            if (!file.is_regular_file(ec))
            {
                continue;
            }
            Entry entry;
            entry.job_id = job_id;
            entry.file_name = Utf8(file.path().filename());
            entry.original_name = entry.file_name;
            entry.path = Utf8(file.path());
            entry.size_bytes = static_cast<long long>(file.file_size(ec));
            entry.created_at = created;
            adopted.push_back(std::move(entry));
        }
    }

    {
        std::lock_guard<std::mutex> lk(mtx_);
        for (auto& entry : adopted)
        {
            entry.id = "upl-" + std::to_string(++next_id_);
            entries_.emplace(entry.id, std::move(entry));
        }
    }
    // Converge immediately instead of waiting for the first sweep tick.
    Sweep();
}

void UploadStore::SweepLoop()
{
    std::unique_lock<std::mutex> lk(mtx_);
    const auto period = std::chrono::seconds(std::max(1, options_.sweep_interval_seconds));
    while (!stop_)
    {
        if (cv_.wait_for(lk, period, [this] { return stop_; }))
        {
            break;
        }
        lk.unlock();
        Sweep();
        lk.lock();
    }
}

std::vector<UploadStore::Entry> UploadStore::EvictLocked(long long now, std::size_t incoming_count,
                                                         long long incoming_bytes)
{
    std::vector<Entry> evicted;

    if (ByTime() && options_.ttl_seconds > 0)
    {
        for (auto it = entries_.begin(); it != entries_.end();)
        {
            const Entry& entry = it->second;
            if (!entry.in_use)
            {
                const long long base = (entry.terminal_at != 0) ? entry.terminal_at : entry.created_at;
                if (now - base >= options_.ttl_seconds)
                {
                    evicted.push_back(entry);
                    it = entries_.erase(it);
                    continue;
                }
            }
            ++it;
        }
    }

    const long long cap_bytes = options_.max_total_mb * 1024 * 1024;
    const std::size_t cap_files = static_cast<std::size_t>(std::max(1, options_.max_files));
    const auto over_count = [&] { return ByCount() && entries_.size() + incoming_count > cap_files; };
    const auto over_size = [&] { return ByTime() && cap_bytes > 0 && TotalBytesLocked() + incoming_bytes > cap_bytes; };
    if (!over_count() && !over_size())
    {
        return evicted;
    }

    // Oldest first. Entries still referenced by a live job are skipped so an active
    // slice never loses the model it is reading.
    std::vector<std::pair<long long, std::string>> order;
    order.reserve(entries_.size());
    for (const auto& [id, entry] : entries_)
    {
        if (!entry.in_use)
        {
            order.emplace_back(entry.created_at, id);
        }
    }
    std::sort(order.begin(), order.end());
    for (const auto& [created_at, id] : order)
    {
        if (!over_count() && !over_size())
        {
            break;
        }
        const auto it = entries_.find(id);
        if (it == entries_.end())
        {
            continue;
        }
        evicted.push_back(it->second);
        entries_.erase(it);
    }
    return evicted;
}

long long UploadStore::TotalBytesLocked() const
{
    long long total = 0;
    for (const auto& [id, entry] : entries_)
    {
        total += entry.size_bytes;
    }
    return total;
}

void UploadStore::RemoveFiles(const std::vector<Entry>& entries)
{
    std::error_code ec;
    const std::filesystem::path root = std::filesystem::u8path(options_.dir);
    for (const Entry& entry : entries)
    {
        const std::filesystem::path file = std::filesystem::u8path(entry.path);
        std::filesystem::remove(file, ec);
        const std::filesystem::path parent = file.parent_path();
        // remove() only succeeds on an empty directory, so this collapses the
        // per-job folder without ever touching the pool root.
        if (!parent.empty() && parent != root)
        {
            std::filesystem::remove(parent, ec);
        }
    }
}

UploadInfo UploadStore::ToInfo(const Entry& entry) const
{
    UploadInfo info;
    info.id = entry.id;
    info.job_id = entry.job_id;
    info.file_name = entry.file_name;
    info.original_name = entry.original_name;
    info.path = entry.path;
    info.state = entry.in_use ? "in_use" : (entry.terminal_at != 0 ? "terminal" : "pending");
    info.size_bytes = entry.size_bytes;
    info.created_at = entry.created_at;
    // A file still referenced by a live job is exempt from the age clock, so it is
    // reported without an expiry instead of a stale one.
    if (!entry.in_use && options_.ttl_seconds > 0)
    {
        const long long base = (entry.terminal_at != 0) ? entry.terminal_at : entry.created_at;
        info.expires_at = base + options_.ttl_seconds;
        const long long remaining = info.expires_at - NowSeconds();
        info.remaining_seconds = remaining > 0 ? remaining : 0;
    }
    return info;
}

}  // namespace HsBa::Slicer::RestApi
