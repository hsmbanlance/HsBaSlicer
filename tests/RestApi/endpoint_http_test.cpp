/** @file endpoint_http_test.cpp
 * @brief HTTP-level contract tests for the real route table of restapi/routes.cpp.
 * @author HsBa
 *
 * This is the only restapi suite that opens a socket: it installs the production
 * RegisterRoutes() on a cpp-httplib listener bound to an ephemeral loopback port and
 * talks to it with a real HTTP client, so everything a client can observe is checked
 * here - status codes, Content-Type/Content-Disposition, the multipart model upload,
 * the auth gate and the JSON/XML negotiation. The pipelines themselves are replaced by
 * the JobManager runner seam, which keeps the suite fast and model-free; running a real
 * slice end to end is covered by webui/test/e2e.test.js against HsBaSlicerRestServer.
 */
#include <atomic>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

// cpp-httplib has to be read before anything that can pull in the legacy winsock.h
// (Boost.Test does on Windows); winsock2.h and winsock.h cannot coexist in one TU.
#include <httplib.h>

#define BOOST_TEST_MODULE RestApiEndpointHttpTest
#include <boost/test/included/unit_test.hpp>

#include "pipelinetypes/pipeline_types.h"

#include "../../restapi/job_manager.hpp"
#include "../../restapi/pipeline_executor.hpp"
#include "../../restapi/routes.hpp"
#include "../../restapi/server_config.hpp"
#include "../../restapi/upload_store.hpp"

using namespace HsBa::Slicer;
using namespace HsBa::Slicer::RestApi;
using namespace std::chrono_literals;

namespace
{
/** @brief Body a ".gcode" job answers with inline (the FDM result shape). */
constexpr const char* kInlineGCode = "G21\nG90\nG1 X1.0 Y2.0 E3.0 F1200\n";

/** @brief Body a job with any other output name writes to disk (the export result shape). */
constexpr const char* kArtifactBytes = "artifact-bytes\n";

/** @brief A tiny ASCII STL, used only as multipart upload payload. */
constexpr const char* kTinyStl = "solid cube\nfacet normal 0 0 1\nouter loop\nendloop\nendfacet\nendsolid cube\n";

/** @brief Self-cleaning throwaway directory; one per EndpointServer instance. */
class TempDir
{
public:
    explicit TempDir(const std::string& tag)
    {
        static int counter = 0;
        path_ = std::filesystem::temp_directory_path() / ("hsba_endpoint_" + tag + "_" + std::to_string(counter++));
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

    /**
     * @brief Slash-separated path for use inside JSON bodies.
     *
     * Native Windows separators would have to be escaped as \\ in every config body, and an
     * unescaped one is a JSON parse error, so the tests only ever send forward slashes.
     */
    std::string jsonPath(const std::string& file) const
    {
        const std::u8string joined = (path_ / std::filesystem::u8path(file)).generic_u8string();
        return std::string(reinterpret_cast<const char*>(joined.data()), joined.size());
    }

    std::filesystem::path native(const std::string& file) const { return path_ / std::filesystem::u8path(file); }

private:
    std::filesystem::path path_;
};

/** @brief Whether a client call got a response at all (httplib Result has an explicit bool). */
bool Responded(const httplib::Result& res)
{
    return static_cast<bool>(res);
}

/** @brief Whole content of a file on disk, for byte-for-byte comparisons with a response body. */
std::string ReadAll(const std::filesystem::path& file)
{
    std::ifstream in(file, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

/** @brief Whether a header value names @p type (httplib appends a charset to text-ish types). */
bool HeaderIs(const httplib::Result& res, const std::string& key, const std::string& type)
{
    return res->get_header_value(key).rfind(type, 0) == 0;
}

/** @brief Read a flat JSON string member; an empty result means absent or not a string. */
std::string JsonMember(const std::string& body, const std::string& key)
{
    const std::string name = "\"" + key + "\"";
    std::size_t pos = body.find(name);
    if (pos == std::string::npos)
    {
        return {};
    }
    pos = body.find(':', pos + name.size());
    if (pos == std::string::npos)
    {
        return {};
    }
    ++pos;
    while (pos < body.size() && (body[pos] == ' ' || body[pos] == '\t' || body[pos] == '\n' || body[pos] == '\r'))
    {
        ++pos;
    }
    if (pos >= body.size() || body[pos] != '"')
    {
        return {};
    }
    const std::size_t end = body.find('"', pos + 1);
    return end == std::string::npos ? std::string() : body.substr(pos + 1, end - pos - 1);
}

/** @brief Read a flat JSON number member (@p found receives whether one was present). */
long long JsonNumber(const std::string& body, const std::string& key, bool& found)
{
    found = false;
    const std::string name = "\"" + key + "\"";
    std::size_t pos = body.find(name);
    if (pos == std::string::npos)
    {
        return 0;
    }
    pos = body.find(':', pos + name.size());
    if (pos == std::string::npos)
    {
        return 0;
    }
    ++pos;
    while (pos < body.size() && (body[pos] == ' ' || body[pos] == '\t' || body[pos] == '\n' || body[pos] == '\r'))
    {
        ++pos;
    }
    const std::size_t digits = body.find_first_of("-0123456789", pos);
    if (digits != pos)
    {
        return 0;
    }
    std::size_t length = 1;
    while (pos + length < body.size() && std::isdigit(static_cast<unsigned char>(body[pos + length])) != 0)
    {
        ++length;
    }
    found = true;
    return std::stoll(body.substr(pos, length));
}

/**
 * @brief Pipeline stand-in that produces exactly what the result endpoint branches on.
 *
 * The configured output_path is written to disk like a real pipeline would. A ".gcode"
 * target additionally comes back as inline text (the FDM shape); every other target is a
 * file only (the export shape), so the download branch of GET /api/jobs/{id}/result is
 * reachable without any slicing kernel.
 */
JobManager::Runner ArtifactPipeline(std::string content)
{
    return [content = std::move(content)](HsBaPipelineKind, const void* config, HsBaProgressCallback cb, void* ud)
    {
        SliceOutcome outcome;
        outcome.success = 1;
        outcome.total_layers = 3;
        outcome.elapsed_seconds = 0.01;

        const auto* fdm = static_cast<const HsBaFdmPipelineConfig_t*>(config);
        const std::string target = fdm->output_path ? fdm->output_path : std::string();
        if (!target.empty())
        {
            const std::filesystem::path file = std::filesystem::u8path(target);
            std::error_code ec;
            std::filesystem::create_directories(file.parent_path(), ec);
            std::ofstream out(file, std::ios::binary | std::ios::trunc);
            out << content;
            out.close();
            outcome.output_path = target;
            if (target.size() >= 6 && target.compare(target.size() - 6, 6, ".gcode") == 0)
            {
                outcome.inline_content = content;  // the real FDM keeps the G-code in memory too
            }
        }
        if (cb)
        {
            cb(100, "artifact written", ud);
        }
        return outcome;
    };
}

/**
 * @brief Pipeline stand-in that parks in the running state, making the pre-terminal
 *        states observable over HTTP.
 *
 * With a single worker one job holds the worker and every later job stays queued, so
 * 409 (not finished) and 429 (queue full) are deterministic. The wait is bounded to five
 * seconds so a forgotten Release() fails an assertion instead of hanging the suite.
 */
class BlockingPipeline
{
public:
    JobManager::Runner AsRunner()
    {
        return [this](HsBaPipelineKind, const void*, HsBaProgressCallback cb, void* ud)
        {
            started_.store(true);
            for (int i = 0; i < 1000 && !released_.load(); ++i)
            {
                std::this_thread::sleep_for(5ms);
            }
            if (cb)
            {
                cb(100, "released", ud);
            }
            SliceOutcome outcome;
            outcome.success = 1;
            outcome.total_layers = 1;
            outcome.inline_content = kInlineGCode;
            return outcome;
        };
    }

    /** @brief Whether a worker entered the runner (and so whether the job is "running"). */
    bool WaitUntilRunning() const
    {
        for (int i = 0; i < 400 && !started_.load(); ++i)
        {
            std::this_thread::sleep_for(5ms);
        }
        return started_.load();
    }

    void Release() { released_.store(true); }

    /** @brief Let the same instance park a second job. */
    void Rewarm()
    {
        Release();
        started_.store(false);
        released_.store(false);
    }

private:
    std::atomic<bool> started_{false};
    std::atomic<bool> released_{false};
};

/** @brief Knobs an EndpointServer instance needs before its members are built. */
struct ServerOptions
{
    int workers = 1;           ///< Concurrent job slots (1 keeps the queue observable).
    int max_jobs = 8;          ///< Tracked-job cap, for the 429 case.
    bool uploads = true;       ///< Whether [upload] dir is configured (multipart enabled).
    std::string token;         ///< [auth] token.
    bool remote_bind = false;  ///< Pretend the listener is bound to a non-loopback address.
};

/**
 * @brief The production route table on a real listener, plus an HTTP client for it.
 *
 * The socket is always bound to loopback. A "remote" binding is simulated by setting
 * [server] host, which is exactly what the authorization gate reads in RegisterRoutes, so
 * the token rules are exercised without ever exposing a port to the network.
 */
class EndpointServer
{
public:
    EndpointServer(JobManager::Runner runner, ServerOptions opts = {})
        : uploads_dir_("srv"), cfg_(MakeConfig(opts, uploads_dir_)), uploads_(MakeUploadOptions(cfg_)),
          jobs_(opts.workers, opts.max_jobs, std::chrono::seconds(cfg_.result_ttl_seconds), std::move(runner),
                [this](const std::string& job_id) { uploads_.MarkTerminal(job_id); }),
          ctx_{cfg_, jobs_, uploads_, std::chrono::steady_clock::now()}
    {
        RegisterRoutes(svr_, ctx_);
        port_ = svr_.bind_to_any_port("127.0.0.1");
        BOOST_REQUIRE_GT(port_, 0);
        thread_ = std::thread([this] { svr_.listen_after_bind(); });
        for (int i = 0; i < 400 && !svr_.is_running(); ++i)
        {
            std::this_thread::sleep_for(5ms);
        }
        BOOST_REQUIRE(svr_.is_running());
    }

    ~EndpointServer()
    {
        svr_.stop();
        if (thread_.joinable())
        {
            thread_.join();
        }
    }

    EndpointServer(const EndpointServer&) = delete;
    EndpointServer& operator=(const EndpointServer&) = delete;

    httplib::Result Get(const std::string& path, const httplib::Headers& headers = {}) const
    {
        return Client().Get(path, headers);
    }

    httplib::Result Post(const std::string& path, const std::string& body, const std::string& content_type,
                         const httplib::Headers& headers = {}) const
    {
        return Client().Post(path, headers, body, content_type);
    }

    httplib::Result PostMultipart(const std::string& path, const httplib::UploadFormDataItems& items) const
    {
        return Client().Post(path, items);
    }

    httplib::Result Put(const std::string& path, const std::string& body, const std::string& content_type) const
    {
        return Client().Put(path, body, content_type);
    }

    httplib::Result Delete(const std::string& path) const { return Client().Delete(path); }

    /** @brief Job id of an accepted submission, taken from the 202 body. */
    static std::string AcceptedJobId(const httplib::Result& res)
    {
        BOOST_REQUIRE(Responded(res));
        BOOST_REQUIRE_EQUAL(res->status, 202);
        const std::string id = JsonMember(res->body, "id");
        BOOST_REQUIRE(!id.empty());
        return id;
    }

    /**
     * @brief Poll GET /api/jobs/{id} until the state reads @p wanted.
     * @return The state actually observed, so a timeout fails with a useful message.
     */
    std::string WaitJobState(const std::string& id, const std::string& wanted,
                             std::chrono::milliseconds timeout = 20s) const
    {
        std::string state;
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        do
        {
            const httplib::Result res = Get("/api/jobs/" + id);
            state = (Responded(res) && res->status == 200) ? JsonMember(res->body, "state") : std::string();
            if (state == wanted)
            {
                return state;
            }
            std::this_thread::sleep_for(10ms);
        } while (std::chrono::steady_clock::now() < deadline);
        return state;
    }

    /** @brief Submit an fdm job writing to @p output_path inside the scratch directory. */
    std::string SubmitArtifactJob(const TempDir& scratch, const std::string& output_name) const
    {
        const std::string body =
            R"({"layer_height": 2, "enable_support": 0, "output_path": ")" + scratch.jsonPath(output_name) + R"("})";
        return AcceptedJobId(Post("/api/jobs/fdm", body, "application/json"));
    }

    const std::string& uploadRoot() const { return cfg_.upload_dir; }

private:
    httplib::Client Client() const { return httplib::Client("127.0.0.1", port_); }

    static ServerConfig MakeConfig(const ServerOptions& opts, const TempDir& uploads_dir)
    {
        ServerConfig cfg;
        cfg.host = opts.remote_bind ? "0.0.0.0" : "127.0.0.1";
        cfg.port = 0;  // the real port comes from bind_to_any_port()
        cfg.default_format = "json";
        cfg.max_concurrent_jobs = opts.workers;
        cfg.max_jobs = opts.max_jobs;
        cfg.result_ttl_seconds = 3600;
        cfg.auth_enabled = !opts.token.empty();
        cfg.auth_token = opts.token;
        if (opts.uploads)
        {
            cfg.upload_enabled = true;
            cfg.upload_dir = uploads_dir.jsonPath("pool");
            cfg.upload_max_files = 8;
            cfg.upload_max_file_mb = 16;
            cfg.upload_max_request_mb = 20;
            cfg.upload_ttl_seconds = 3600;
            cfg.upload_sweep_interval_seconds = 3600;  // keep the reclaimer out of the assertions
        }
        return cfg;
    }

    static UploadOptions MakeUploadOptions(const ServerConfig& cfg)
    {
        UploadOptions options;
        options.dir = cfg.upload_dir;
        options.max_files = cfg.upload_max_files;
        options.ttl_seconds = cfg.upload_ttl_seconds;
        options.evict_mode = cfg.upload_evict_mode;
        options.max_total_mb = cfg.upload_max_total_mb;
        options.sweep_interval_seconds = cfg.upload_sweep_interval_seconds;
        return options;
    }

    // Declaration order matters: Ctx borrows the three objects above it and the route
    // lambdas borrow Ctx, so nothing here may be destroyed before the listener stops.
    TempDir uploads_dir_;
    ServerConfig cfg_;
    UploadStore uploads_;
    JobManager jobs_;
    Ctx ctx_;
    httplib::Server svr_;
    std::thread thread_;
    int port_ = 0;
};

/** @brief The upload pool file of @p job_id, following the server's own layout. */
std::filesystem::path UploadedModel(const EndpointServer& srv, const std::string& job_id)
{
    return std::filesystem::u8path(srv.uploadRoot()) / job_id / "cube.stl";
}
}  // namespace

BOOST_AUTO_TEST_SUITE(restapi_endpoint_http)

BOOST_AUTO_TEST_CASE(health_index_and_pipelines_answer_both_formats)
{
    EndpointServer srv{ArtifactPipeline(kInlineGCode)};

    const httplib::Result health = srv.Get("/api/health");
    BOOST_REQUIRE(Responded(health));
    BOOST_CHECK_EQUAL(health->status, 200);
    BOOST_CHECK(HeaderIs(health, "Content-Type", "application/json"));
    BOOST_CHECK_EQUAL(JsonMember(health->body, "status"), "ok");

    // ?format= wins over the Accept header, and both must reach the XML serializer.
    const httplib::Result xml = srv.Get("/api/health?format=xml");
    BOOST_REQUIRE(Responded(xml));
    BOOST_CHECK_EQUAL(xml->status, 200);
    BOOST_CHECK(HeaderIs(xml, "Content-Type", "application/xml"));
    BOOST_CHECK(xml->body.find("<Health") != std::string::npos);
    BOOST_CHECK(xml->body.find("<status>ok</status>") != std::string::npos);

    const httplib::Result accept_xml = srv.Get("/api/pipelines", {{"Accept", "application/xml"}});
    BOOST_REQUIRE(Responded(accept_xml));
    BOOST_CHECK(accept_xml->body.find("<Pipelines") != std::string::npos);

    const httplib::Result index = srv.Get("/");
    BOOST_REQUIRE(Responded(index));
    BOOST_CHECK_EQUAL(index->status, 200);
    BOOST_CHECK_EQUAL(JsonMember(index->body, "service"), "HsBaSlicerRestServer");
    BOOST_CHECK_EQUAL(JsonMember(index->body, "jobs_endpoint"), "/api/jobs");

    const httplib::Result kinds = srv.Get("/api/pipelines");
    BOOST_REQUIRE(Responded(kinds));
    BOOST_CHECK(kinds->body.find("\"fdm\"") != std::string::npos);

    const httplib::Result defaults = srv.Get("/api/config/default/fdm");
    BOOST_REQUIRE(Responded(defaults));
    BOOST_CHECK_EQUAL(defaults->status, 200);
    BOOST_CHECK(defaults->body.find("layer_height") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(json_job_is_accepted_and_its_inline_result_is_served)
{
    TempDir scratch{"job"};
    EndpointServer srv{ArtifactPipeline(kInlineGCode)};

    const httplib::Result accepted = srv.Post("/api/jobs/fdm",
                                              R"({"layer_height": 2, "enable_support": 0, "output_path": ")" +
                                                  scratch.jsonPath("out/inline.gcode") + R"("})",
                                              "application/json");
    BOOST_REQUIRE(Responded(accepted));
    BOOST_CHECK_EQUAL(accepted->status, 202);
    const std::string id = JsonMember(accepted->body, "id");
    BOOST_REQUIRE(!id.empty());
    BOOST_CHECK_EQUAL(accepted->get_header_value("Location"), "/api/jobs/" + id);
    BOOST_CHECK_EQUAL(JsonMember(accepted->body, "state"), "queued");
    BOOST_CHECK_EQUAL(JsonMember(accepted->body, "kind"), "fdm");

    BOOST_CHECK_EQUAL(srv.WaitJobState(id, "success"), "success");

    // The finished job must report the file it wrote, which is what the Web console shows.
    const httplib::Result status = srv.Get("/api/jobs/" + id);
    BOOST_REQUIRE(Responded(status));
    BOOST_CHECK_EQUAL(JsonMember(status->body, "output_path"), scratch.jsonPath("out/inline.gcode"));
    bool layered = false;
    BOOST_CHECK_EQUAL(JsonNumber(status->body, "total_layers", layered), 3);
    BOOST_CHECK(layered);

    const httplib::Result result = srv.Get("/api/jobs/" + id + "/result");
    BOOST_REQUIRE(Responded(result));
    BOOST_CHECK_EQUAL(result->status, 200);
    BOOST_CHECK(HeaderIs(result, "Content-Type", "text/plain"));
    BOOST_CHECK_EQUAL(result->body, kInlineGCode);

    // The inline branch and the file on disk must hold the same bytes.
    BOOST_CHECK_EQUAL(ReadAll(scratch.native("out/inline.gcode")), std::string(kInlineGCode));

    const httplib::Result listing = srv.Get("/api/jobs");
    BOOST_REQUIRE(Responded(listing));
    BOOST_CHECK(listing->body.find(id) != std::string::npos);
}

BOOST_AUTO_TEST_CASE(file_only_result_is_downloaded_under_its_bare_name)
{
    TempDir scratch{"file"};
    EndpointServer srv{ArtifactPipeline(kArtifactBytes)};

    const std::string id = srv.SubmitArtifactJob(scratch, "out/deep/nested/artifact.bin");
    BOOST_REQUIRE_EQUAL(srv.WaitJobState(id, "success"), "success");

    const httplib::Result result = srv.Get("/api/jobs/" + id + "/result");
    BOOST_REQUIRE(Responded(result));
    BOOST_CHECK_EQUAL(result->status, 200);
    BOOST_CHECK(HeaderIs(result, "Content-Type", "application/octet-stream"));
    BOOST_CHECK_EQUAL(result->body, kArtifactBytes);

    // A client must learn the file name, never the server-side directory it lives in.
    const std::string disposition = result->get_header_value("Content-Disposition");
    BOOST_CHECK_EQUAL(disposition, "attachment; filename=\"artifact.bin\"");
    BOOST_CHECK(disposition.find('/') == std::string::npos);
    BOOST_CHECK(disposition.find('\\') == std::string::npos);
}

BOOST_AUTO_TEST_CASE(unfinished_and_unknown_jobs_have_distinct_statuses)
{
    BlockingPipeline gate;
    EndpointServer srv{gate.AsRunner(), ServerOptions{/*workers=*/1, /*max_jobs=*/8}};
    TempDir scratch{"states"};

    const std::string running_id = srv.SubmitArtifactJob(scratch, "running.gcode");
    BOOST_CHECK(gate.WaitUntilRunning());
    const std::string queued_id = srv.SubmitArtifactJob(scratch, "queued.gcode");

    BOOST_CHECK_EQUAL(JsonMember(srv.Get("/api/jobs/" + running_id)->body, "state"), "running");
    BOOST_CHECK_EQUAL(JsonMember(srv.Get("/api/jobs/" + queued_id)->body, "state"), "queued");

    // A job that exists but has no usable result is a conflict, not a missing file.
    const httplib::Result pending_result = srv.Get("/api/jobs/" + queued_id + "/result");
    BOOST_REQUIRE(pending_result);
    BOOST_CHECK_EQUAL(pending_result->status, 409);
    BOOST_CHECK_EQUAL(JsonMember(pending_result->body, "message"), "job not finished successfully");

    // Running work is not deletable, while the queue itself refuses a third job at max_jobs.
    BOOST_CHECK_EQUAL(srv.Delete("/api/jobs/" + running_id)->status, 409);

    const httplib::Result missing = srv.Get("/api/jobs/job-does-not-exist");
    BOOST_REQUIRE(Responded(missing));
    BOOST_CHECK_EQUAL(missing->status, 404);
    BOOST_CHECK_EQUAL(JsonMember(missing->body, "message"), "job not found");
    BOOST_CHECK_EQUAL(srv.Get("/api/jobs/job-does-not-exist/result")->status, 404);

    const httplib::Result missing_xml = srv.Get("/api/jobs/job-does-not-exist?format=xml");
    BOOST_REQUIRE(Responded(missing_xml));
    BOOST_CHECK_EQUAL(missing_xml->status, 404);
    BOOST_CHECK(missing_xml->body.find("<Error") != std::string::npos);
    BOOST_CHECK(missing_xml->body.find("<code>404</code>") != std::string::npos);

    gate.Release();
    BOOST_CHECK_EQUAL(srv.WaitJobState(queued_id, "success"), "success");

    // After deletion the job is gone from status and result alike (204 = deleted, no body).
    const httplib::Result deleted = srv.Delete("/api/jobs/" + running_id);
    BOOST_REQUIRE(Responded(deleted));
    BOOST_CHECK_EQUAL(deleted->status, 204);
    BOOST_CHECK_EQUAL(srv.Get("/api/jobs/" + running_id)->status, 404);
}

BOOST_AUTO_TEST_CASE(queue_full_is_reported_as_429)
{
    BlockingPipeline gate;
    EndpointServer srv{gate.AsRunner(), ServerOptions{/*workers=*/1, /*max_jobs=*/1}};
    TempDir scratch{"full"};

    const std::string first = srv.SubmitArtifactJob(scratch, "first.gcode");
    BOOST_CHECK(gate.WaitUntilRunning());

    const httplib::Result refused = srv.Post(
        "/api/jobs/fdm", R"({"output_path": ")" + scratch.jsonPath("second.gcode") + R"("})", "application/json");
    BOOST_REQUIRE(Responded(refused));
    BOOST_CHECK_EQUAL(refused->status, 429);
    BOOST_CHECK_EQUAL(JsonMember(refused->body, "message"), "job queue full");

    gate.Release();
    BOOST_CHECK_EQUAL(srv.WaitJobState(first, "success"), "success");
}

BOOST_AUTO_TEST_CASE(bad_kind_and_bad_body_are_rejected)
{
    EndpointServer srv{ArtifactPipeline(kInlineGCode)};

    const httplib::Result bad_kind = srv.Post("/api/jobs/nonsense", "{}", "application/json");
    BOOST_REQUIRE(Responded(bad_kind));
    BOOST_CHECK_EQUAL(bad_kind->status, 400);
    BOOST_CHECK_EQUAL(JsonMember(bad_kind->body, "message"), "missing or unknown pipeline kind");

    // The ?kind= form of POST /api/jobs takes the same path, so it shares the rejection.
    BOOST_CHECK_EQUAL(srv.Post("/api/jobs?kind=nonsense", "{}", "application/json")->status, 400);

    const httplib::Result bad_body = srv.Post("/api/jobs/fdm", "{ not json", "application/json");
    BOOST_REQUIRE(Responded(bad_body));
    BOOST_CHECK_EQUAL(bad_body->status, 400);
    BOOST_CHECK_EQUAL(JsonMember(bad_body->body, "message"), "invalid config body");

    BOOST_CHECK_EQUAL(srv.Get("/api/config/default/nonsense")->status, 404);

    // Without [paramstore] configuration the persistence endpoints stay unavailable.
    BOOST_CHECK_EQUAL(srv.Put("/api/params/fdm/demo", "{}", "application/json")->status, 503);
    BOOST_CHECK_EQUAL(srv.Get("/api/params/fdm/demo")->status, 503);
}

BOOST_AUTO_TEST_CASE(multipart_model_reaches_the_pool_and_the_job)
{
    TempDir scratch{"upload"};
    EndpointServer srv{ArtifactPipeline(kInlineGCode)};

    const httplib::UploadFormDataItems parts = {
        {"config", R"({"layer_height": 2, "output_path": ")" + scratch.jsonPath("uploaded.gcode") + R"("})",
         "config.json", "application/json"},
        {"model", kTinyStl, "cube.stl", "model/stl"},
    };
    const httplib::Result accepted = srv.PostMultipart("/api/jobs/fdm", parts);
    BOOST_REQUIRE(Responded(accepted));
    BOOST_CHECK_EQUAL(accepted->status, 202);
    const std::string id = JsonMember(accepted->body, "id");
    BOOST_REQUIRE(!id.empty());

    // The stored model is listed, tied to the job, and readable from disk.
    const httplib::Result listing = srv.Get("/api/uploads");
    BOOST_REQUIRE(Responded(listing));
    BOOST_CHECK_EQUAL(listing->status, 200);
    BOOST_CHECK(listing->body.find(id) != std::string::npos);
    BOOST_CHECK(listing->body.find("cube.stl") != std::string::npos);
    bool counted = false;
    BOOST_CHECK_EQUAL(JsonNumber(listing->body, "count", counted), 1);
    BOOST_CHECK(counted);
    BOOST_CHECK(std::filesystem::exists(UploadedModel(srv, id)));

    BOOST_CHECK_EQUAL(srv.WaitJobState(id, "success"), "success");

    const httplib::Result detail = srv.Get("/api/uploads/" + JsonMember(listing->body, "id"));
    BOOST_REQUIRE(Responded(detail));
    BOOST_CHECK_EQUAL(detail->status, 200);
    // The job finished, so the retention clock started and the file is deletable again.
    BOOST_CHECK_EQUAL(JsonMember(detail->body, "state"), "terminal");

    const std::string upload_id = JsonMember(detail->body, "id");
    BOOST_CHECK_EQUAL(srv.Delete("/api/uploads/" + upload_id)->status, 204);
    BOOST_CHECK(!std::filesystem::exists(UploadedModel(srv, id)));
    BOOST_CHECK_EQUAL(srv.Delete("/api/uploads/" + upload_id)->status, 404);
}

BOOST_AUTO_TEST_CASE(multipart_without_config_or_for_a_model_less_kind_is_refused)
{
    EndpointServer srv{ArtifactPipeline(kInlineGCode)};

    // No model part at all: multipart submissions always need both parts.
    const httplib::Result no_model =
        srv.PostMultipart("/api/jobs/fdm", {{"config", R"({"layer_height": 2})", "config.json", "application/json"}});
    BOOST_REQUIRE(Responded(no_model));
    BOOST_CHECK_EQUAL(no_model->status, 400);
    BOOST_CHECK_EQUAL(JsonMember(no_model->body, "message"), "missing model part");

    // filetransfer has no model_path field, so a model part cannot be bound to it.
    const httplib::Result wrong_kind =
        srv.PostMultipart("/api/jobs/filetransfer", {{"config", "{}", "config.json", "application/json"},
                                                     {"model", kTinyStl, "cube.stl", "model/stl"}});
    BOOST_REQUIRE(Responded(wrong_kind));
    BOOST_CHECK_EQUAL(wrong_kind->status, 400);
    BOOST_CHECK_EQUAL(JsonMember(wrong_kind->body, "message"), "pipeline kind accepts no model file");
}

BOOST_AUTO_TEST_CASE(upload_pool_absent_answers_503)
{
    EndpointServer srv{ArtifactPipeline(kInlineGCode), ServerOptions{/*workers=*/1, /*max_jobs=*/8, /*uploads=*/false}};

    BOOST_CHECK_EQUAL(srv.Get("/api/uploads")->status, 503);
    const httplib::Result multipart =
        srv.PostMultipart("/api/jobs/fdm", {{"config", "{}", "config.json", "application/json"},
                                            {"model", kTinyStl, "cube.stl", "model/stl"}});
    BOOST_REQUIRE(Responded(multipart));
    BOOST_CHECK_EQUAL(multipart->status, 503);
    BOOST_CHECK_EQUAL(JsonMember(multipart->body, "message"), "model upload disabled");
}

BOOST_AUTO_TEST_CASE(token_gate_protects_a_non_loopback_binding)
{
    ServerOptions opts;
    opts.token = "s3cret-token";
    opts.remote_bind = true;
    EndpointServer srv{ArtifactPipeline(kInlineGCode), opts};

    const httplib::Result anonymous = srv.Get("/api/jobs");
    BOOST_REQUIRE(Responded(anonymous));
    BOOST_CHECK_EQUAL(anonymous->status, 401);
    BOOST_CHECK_EQUAL(anonymous->get_header_value("WWW-Authenticate"), "Bearer realm=\"HsBaSlicer\"");
    BOOST_CHECK_EQUAL(JsonMember(anonymous->body, "message"), "missing credentials");

    const httplib::Result wrong = srv.Get("/api/jobs", {{"X-API-Key", "wrong-token"}});
    BOOST_REQUIRE(Responded(wrong));
    BOOST_CHECK_EQUAL(wrong->status, 401);
    BOOST_CHECK_EQUAL(JsonMember(wrong->body, "message"), "invalid credentials");

    // Both accepted credential shapes reach the job listing, and the liveness probes
    // stay token-free so a monitoring agent can still poll /api/health.
    BOOST_CHECK_EQUAL(srv.Get("/api/jobs", {{"X-API-Key", "s3cret-token"}})->status, 200);
    BOOST_CHECK_EQUAL(srv.Get("/api/jobs", {{"Authorization", "Bearer s3cret-token"}})->status, 200);
    BOOST_CHECK_EQUAL(srv.Get("/api/health")->status, 200);
    BOOST_CHECK_EQUAL(srv.Get("/")->status, 200);
}

BOOST_AUTO_TEST_CASE(proto_conversion_round_trips_over_http)
{
    EndpointServer srv{ArtifactPipeline(kInlineGCode)};

    // The default config is JSON text; ?to=proto encodes it, and the response is protobuf bytes.
    const std::string json = srv.Get("/api/config/default/fdm")->body;
    BOOST_REQUIRE(!json.empty());
    const httplib::Result encoded = srv.Post("/api/config/convert?kind=fdm&to=proto", json, "application/json");
    BOOST_REQUIRE(Responded(encoded));
    BOOST_CHECK_EQUAL(encoded->status, 200);
    BOOST_CHECK(HeaderIs(encoded, "Content-Type", "application/x-protobuf"));
    BOOST_CHECK(!encoded->body.empty());

    // Feeding those bytes back renders the same configuration as text again.
    const httplib::Result decoded = srv.Post("/api/config/convert?kind=fdm", encoded->body, "application/x-protobuf");
    BOOST_REQUIRE(Responded(decoded));
    BOOST_CHECK_EQUAL(decoded->status, 200);
    BOOST_CHECK(decoded->body.find("layer_height") != std::string::npos);

    const httplib::Result bad_kind = srv.Post("/api/config/convert?kind=nonsense", "{}", "application/json");
    BOOST_REQUIRE(Responded(bad_kind));
    BOOST_CHECK_EQUAL(bad_kind->status, 400);
}

BOOST_AUTO_TEST_SUITE_END()
