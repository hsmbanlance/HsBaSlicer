/** @file upload_store_test.cpp
 * @brief Model-upload pool, name sanitizing and [upload] ini normalization tests.
 * @author HsBa
 *
 * The upload store is deliberately HTTP-free, so it is compiled straight into this
 * executable together with its companion translation units (the same approach as
 * config_roundtrip_test). Every case works inside a throwaway directory under
 * temp_directory_path() and removes it afterwards.
 */
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#define BOOST_TEST_MODULE RestApiUploadStoreTest
#include <boost/test/included/unit_test.hpp>

#include "pipelinetypes/pipeline_types.h"

// server_config.cpp applies the same fallback, so both translation units agree on
// the value even when the top-level build did not define the macro.
#ifndef HSBA_MODEL_POOL_SIZE
#define HSBA_MODEL_POOL_SIZE 1
#endif

#include "../../restapi/pipeline_executor.hpp"
#include "../../restapi/server_config.hpp"
#include "../../restapi/upload_bridge.hpp"
#include "../../restapi/upload_store.hpp"

using namespace HsBa::Slicer;
using namespace HsBa::Slicer::RestApi;

namespace
{
/** @brief Self-cleaning temporary directory used as the upload root. */
class TempDir
{
public:
    explicit TempDir(const std::string& tag)
    {
        static int counter = 0;
        path_ = std::filesystem::temp_directory_path() / ("hsba_upload_" + tag + "_" + std::to_string(counter++));
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
        std::filesystem::create_directories(path_, ec);
    }

    ~TempDir()
    {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }

    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    const std::filesystem::path& path() const { return path_; }
    std::string str() const { return path_.string(); }

private:
    std::filesystem::path path_;
};

void WriteFile(const std::filesystem::path& file, const std::string& content)
{
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    out << content;
}

bool Exists(const std::string& utf8_path)
{
    return std::filesystem::exists(std::filesystem::u8path(utf8_path));
}

/** @brief Pool options that match the plan defaults but keep the sweep thread idle. */
UploadOptions MakeOptions(const std::string& dir, int max_files, long long ttl, const std::string& mode)
{
    UploadOptions options;
    options.dir = dir;
    options.max_files = max_files;
    options.ttl_seconds = ttl;
    options.evict_mode = mode;
    options.sweep_interval_seconds = 3600;  // the background tick must not race the assertions
    return options;
}

/** @brief Write one model file and hand it to the pool; returns the pool id (empty = refused). */
std::string AdoptFile(UploadStore& store, const TempDir& dir, const std::string& job_id, const std::string& name,
                      const std::string& payload)
{
    const std::string safe = SanitizeUploadName(name);
    const std::string path = RenderUploadPath(dir.str(), job_id, safe);
    WriteFile(std::filesystem::u8path(path), payload);
    return store.Adopt(job_id, name, safe, path, static_cast<long long>(payload.size()));
}
}  // namespace

BOOST_AUTO_TEST_SUITE(restapi_upload_bridge)

BOOST_AUTO_TEST_CASE(sanitize_strips_directories_and_traversal)
{
    BOOST_CHECK_EQUAL(SanitizeUploadName("../../etc/passwd"), "passwd");
    BOOST_CHECK_EQUAL(SanitizeUploadName("C:\\models\\cube.stl"), "cube.stl");
    BOOST_CHECK_EQUAL(SanitizeUploadName("models/cube.stl"), "cube.stl");
    BOOST_CHECK_EQUAL(SanitizeUploadName(".."), kUploadFallbackName);
    BOOST_CHECK_EQUAL(SanitizeUploadName("."), kUploadFallbackName);
    BOOST_CHECK_EQUAL(SanitizeUploadName(""), kUploadFallbackName);
    BOOST_CHECK_EQUAL(SanitizeUploadName("   "), kUploadFallbackName);
}

BOOST_AUTO_TEST_CASE(sanitize_truncates_and_drops_unicode)
{
    const std::string long_name(400, 'a');
    BOOST_CHECK_EQUAL(SanitizeUploadName(long_name).size(), kUploadNameMaxBytes);

    // Non-ASCII bytes are not in the allowlist; the extension is what survives.
    const std::string cjk = SanitizeUploadName("模型 立方体.stl");
    BOOST_CHECK(cjk.ends_with(".stl"));
    BOOST_CHECK(cjk.find('/') == std::string::npos);
    BOOST_CHECK(cjk.find('\\') == std::string::npos);
}

BOOST_AUTO_TEST_CASE(render_path_joins_root_job_and_name)
{
    const std::string joined = RenderUploadPath("uploads", "job-7", "cube.stl");
    const std::filesystem::path path = std::filesystem::u8path(joined);
    BOOST_CHECK_EQUAL(path.filename().string(), "cube.stl");
    BOOST_CHECK_EQUAL(path.parent_path().filename().string(), "job-7");
    BOOST_CHECK_EQUAL(path.parent_path().parent_path().filename().string(), "uploads");
}

BOOST_AUTO_TEST_CASE(bind_uploaded_model_sets_model_path)
{
    void* cfg = CreateDefaultConfig(HSBA_PIPELINE_FDM);
    BOOST_REQUIRE(cfg != nullptr);
    StringArena arena;
    BOOST_CHECK(BindUploadedModel(ConfigTypeInfo(HSBA_PIPELINE_FDM), cfg, "/srv/uploads/job-1/cube.stl", "cube.stl", arena));

    const auto* fdm = static_cast<const HsBaFdmPipelineConfig_t*>(cfg);
    BOOST_REQUIRE(fdm->model_path != nullptr);
    BOOST_CHECK_EQUAL(std::string(fdm->model_path), "/srv/uploads/job-1/cube.stl");
    DestroyConfig(HSBA_PIPELINE_FDM, cfg);
}

BOOST_AUTO_TEST_CASE(bind_uploaded_model_rejects_kind_without_model_path)
{
    // The file-transfer config has no model_path, so a multipart submit must be
    // answered with 400 instead of silently dropping the uploaded file.
    void* cfg = CreateDefaultConfig(HSBA_PIPELINE_FILETRANSFER);
    BOOST_REQUIRE(cfg != nullptr);
    StringArena arena;
    BOOST_CHECK(!BindUploadedModel(ConfigTypeInfo(HSBA_PIPELINE_FILETRANSFER), cfg, "somewhere", "cube.stl", arena));
    BOOST_CHECK(arena.empty());
    DestroyConfig(HSBA_PIPELINE_FILETRANSFER, cfg);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(restapi_upload_store)

BOOST_AUTO_TEST_CASE(store_is_disabled_without_dir)
{
    UploadStore store(MakeOptions("", 8, 3600, "both"));
    BOOST_CHECK(!store.Enabled());
    BOOST_CHECK(store.Adopt("job-1", "cube.stl", "cube.stl", "cube.stl", 3).empty());
    BOOST_CHECK(store.List("", 0, 0).empty());
}

BOOST_AUTO_TEST_CASE(adopted_upload_is_queryable)
{
    TempDir dir("adopt");
    UploadStore store(MakeOptions(dir.str(), 8, 3600, "ttl"));
    BOOST_REQUIRE(store.Enabled());

    const std::string id = AdoptFile(store, dir, "job-1", "cube.stl", "solid");
    BOOST_REQUIRE(!id.empty());

    UploadInfo info;
    BOOST_REQUIRE(store.Get(id, info));
    BOOST_CHECK_EQUAL(info.id, id);
    BOOST_CHECK_EQUAL(info.job_id, "job-1");
    BOOST_CHECK_EQUAL(info.file_name, "cube.stl");
    BOOST_CHECK_EQUAL(info.original_name, "cube.stl");
    BOOST_CHECK_EQUAL(info.state, "pending");
    BOOST_CHECK_EQUAL(info.size_bytes, 5);
    BOOST_CHECK_EQUAL(info.expires_at, info.created_at + 3600);
    BOOST_CHECK(info.remaining_seconds > 0);
    BOOST_CHECK(Exists(info.path));

    const std::vector<UploadInfo> listed = store.List("", 0, 0);
    BOOST_REQUIRE_EQUAL(listed.size(), 1);
    BOOST_CHECK_EQUAL(listed.front().id, id);
    BOOST_CHECK(store.List("in_use", 0, 0).empty());
    BOOST_CHECK_EQUAL(store.List("pending", 0, 0).size(), 1);
    BOOST_CHECK(store.List("pending", 1, 0).empty());  // offset past the end
}

BOOST_AUTO_TEST_CASE(state_follows_the_job_lifecycle)
{
    TempDir dir("lifecycle");
    UploadStore store(MakeOptions(dir.str(), 8, 3600, "ttl"));
    const std::string id = AdoptFile(store, dir, "job-2", "cube.stl", "solid");
    BOOST_REQUIRE(!id.empty());

    store.MarkInUse("job-2");
    UploadInfo info;
    BOOST_REQUIRE(store.Get(id, info));
    BOOST_CHECK_EQUAL(info.state, "in_use");
    // A referenced file is exempt from the age clock, so no expiry is advertised.
    BOOST_CHECK_EQUAL(info.expires_at, 0);
    BOOST_CHECK_EQUAL(info.remaining_seconds, 0);

    store.MarkTerminal("job-2");
    BOOST_REQUIRE(store.Get(id, info));
    BOOST_CHECK_EQUAL(info.state, "terminal");
    BOOST_CHECK(info.expires_at > 0);

    store.Unlink("job-2");
    BOOST_REQUIRE(store.Get(id, info));
    BOOST_CHECK(info.job_id.empty());
    BOOST_CHECK(Exists(info.path));  // unlinking never deletes the file
}

BOOST_AUTO_TEST_CASE(count_policy_evicts_the_oldest_upload)
{
    TempDir dir("count");
    UploadStore store(MakeOptions(dir.str(), 2, 0, "count"));

    std::vector<std::string> ids;
    std::vector<std::string> paths;
    for (int i = 1; i <= 3; ++i)
    {
        const std::string job = "job-" + std::to_string(i);
        const std::string name = "model" + std::to_string(i) + ".stl";
        ids.push_back(AdoptFile(store, dir, job, name, "xyz"));
        BOOST_REQUIRE(!ids.back().empty());
        paths.push_back(RenderUploadPath(dir.str(), job, name));
    }

    BOOST_CHECK(!Exists(paths[0]));  // dropped to stay within max_files
    BOOST_CHECK(Exists(paths[1]));
    BOOST_CHECK(Exists(paths[2]));
    UploadInfo info;
    BOOST_CHECK(!store.Get(ids[0], info));
    BOOST_CHECK_EQUAL(store.List("", 0, 0).size(), 2);
}

BOOST_AUTO_TEST_CASE(in_use_uploads_are_never_evicted)
{
    TempDir dir("in_use");
    UploadStore store(MakeOptions(dir.str(), 1, 0, "count"));

    const std::string first = AdoptFile(store, dir, "job-1", "cube.stl", "solid");
    BOOST_REQUIRE(!first.empty());
    store.MarkInUse("job-1");

    // The single slot is occupied by a live job, so the newcomer is refused
    // instead of sacrificing the model the job is slicing.
    const std::string refused = AdoptFile(store, dir, "job-2", "cube2.stl", "solid");
    BOOST_CHECK(refused.empty());
    UploadInfo info;
    BOOST_CHECK(store.Get(first, info));
    BOOST_CHECK(Exists(info.path));

    // Deleting through the API is refused while the job still holds it.
    BOOST_CHECK(store.Remove(first) == UploadRemoveResult::InUse);

    // Once the job finishes the slot becomes reclaimable again.
    store.MarkTerminal("job-1");
    const std::string second = AdoptFile(store, dir, "job-2", "cube2.stl", "solid");
    BOOST_CHECK(!second.empty());
    BOOST_CHECK(!store.Get(first, info));
    BOOST_CHECK(store.Get(second, info));
}

BOOST_AUTO_TEST_CASE(remove_deletes_the_file)
{
    TempDir dir("remove");
    UploadStore store(MakeOptions(dir.str(), 8, 3600, "both"));
    const std::string id = AdoptFile(store, dir, "job-4", "cube.stl", "solid");
    BOOST_REQUIRE(!id.empty());

    UploadInfo info;
    BOOST_REQUIRE(store.Get(id, info));
    const std::string path = info.path;

    BOOST_CHECK(store.Remove(id) == UploadRemoveResult::Removed);
    BOOST_CHECK(!Exists(path));
    BOOST_CHECK(!std::filesystem::exists(std::filesystem::u8path(path).parent_path()));  // per-job dir collapsed
    BOOST_CHECK(!store.Get(id, info));
    BOOST_CHECK(store.Remove(id) == UploadRemoveResult::NotFound);
}

BOOST_AUTO_TEST_CASE(reconcile_adopts_leftovers_from_disk)
{
    TempDir dir("reconcile");
    const std::filesystem::path job_dir = dir.path() / "job-9";
    WriteFile(job_dir / "kept.stl", "solid");
    // Anything outside a job-* directory belongs to the operator.
    WriteFile(dir.path() / "notes" / "keep.txt", "mine");

    UploadStore store(MakeOptions(dir.str(), 8, 3600, "ttl"));
    const std::vector<UploadInfo> listed = store.List("", 0, 0);
    BOOST_REQUIRE_EQUAL(listed.size(), 1);
    BOOST_CHECK_EQUAL(listed.front().job_id, "job-9");
    BOOST_CHECK_EQUAL(listed.front().file_name, "kept.stl");
    BOOST_CHECK_EQUAL(listed.front().state, "pending");
    BOOST_CHECK(std::filesystem::exists(dir.path() / "notes" / "keep.txt"));
}

BOOST_AUTO_TEST_CASE(reconcile_drops_already_expired_files)
{
    TempDir dir("expired");
    const std::filesystem::path job_dir = dir.path() / "job-3";
    WriteFile(job_dir / "old.stl", "solid");
    // Backdate the job directory: ReconcileDisk uses its mtime as created_at.
    const auto two_days_ago = std::filesystem::file_time_type::clock::now() - std::chrono::hours(48);
    std::filesystem::last_write_time(job_dir, two_days_ago);

    UploadStore store(MakeOptions(dir.str(), 8, 3600, "ttl"));  // 1 hour retention
    BOOST_CHECK(store.List("", 0, 0).empty());
    BOOST_CHECK(!std::filesystem::exists(job_dir / "old.stl"));
}

BOOST_AUTO_TEST_CASE(manual_mode_keeps_everything)
{
    TempDir dir("manual");
    UploadStore store(MakeOptions(dir.str(), 1, 1, "manual"));
    const std::string first = AdoptFile(store, dir, "job-1", "a.stl", "a");
    BOOST_REQUIRE(!first.empty());
    // Neither the age clock nor max_files may reclaim in manual mode.
    const std::string second = AdoptFile(store, dir, "job-2", "b.stl", "b");
    BOOST_CHECK(!second.empty());
    BOOST_CHECK_EQUAL(store.List("", 0, 0).size(), 2);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(restapi_upload_config)

BOOST_AUTO_TEST_CASE(upload_section_disabled_by_default)
{
    const ServerConfig cfg = LoadServerConfig("hsba_upload_missing_section.ini");
    BOOST_CHECK(!cfg.upload_enabled);
    BOOST_CHECK(cfg.upload_dir.empty());
    BOOST_CHECK_EQUAL(cfg.upload_max_files, 256);
    BOOST_CHECK_EQUAL(cfg.upload_ttl_seconds, 604800);
    BOOST_CHECK_EQUAL(cfg.upload_evict_mode, "both");
}

BOOST_AUTO_TEST_CASE(max_files_is_floored_at_model_pool_size)
{
    TempDir dir("ini_pool");
    const std::filesystem::path ini = dir.path() / "restapi.ini";
    WriteFile(ini, "[upload]\ndir = " + dir.str() + "\nmax_files = 1\n");

    const ServerConfig cfg = LoadServerConfig(ini.string());
    BOOST_CHECK(cfg.upload_enabled);
    const int floor = HSBA_MODEL_POOL_SIZE > 0 ? static_cast<int>(HSBA_MODEL_POOL_SIZE) : 1;
    BOOST_CHECK_EQUAL(cfg.upload_max_files, floor);
}

BOOST_AUTO_TEST_CASE(upload_section_is_normalized)
{
    TempDir dir("ini_clamp");
    const std::filesystem::path ini = dir.path() / "restapi.ini";
    WriteFile(ini, "[upload]\ndir = " + dir.str() +
                       "\nevict_mode = bogus\nttl_seconds = 999999999\n"
                       "max_file_mb = 0\nmax_request_mb = 1\nmax_total_mb = -5\n"
                       "sweep_interval_seconds = 1\n");

    const ServerConfig cfg = LoadServerConfig(ini.string());
    BOOST_CHECK_EQUAL(cfg.upload_evict_mode, "both");
    BOOST_CHECK_EQUAL(cfg.upload_ttl_seconds, 2592000);
    BOOST_CHECK_EQUAL(cfg.upload_max_file_mb, 1);
    BOOST_CHECK_EQUAL(cfg.upload_max_request_mb, cfg.upload_max_file_mb + 4);
    BOOST_CHECK_EQUAL(cfg.upload_max_total_mb, 0);
    BOOST_CHECK_EQUAL(cfg.upload_sweep_interval_seconds, 5);
}

BOOST_AUTO_TEST_SUITE_END()
