/** @file routes.cpp
 * @brief HTTP route registration and the DllHsBaSlicer-facing request handlers.
 * @author HsBa
 */
#include "restapi.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <httplib.h>

#include "DllHsBaSlicer/initialize.h"
#include "DllHsBaSlicer/param_store_pipeline.h"
#include "DllHsBaSlicer/version_info.h"

#include "config_bridge.hpp"
#include "content_negotiate.hpp"
#include "job_manager.hpp"
#include "pipeline_executor.hpp"
#include "serialize_value.hpp"
#include "server_config.hpp"

namespace HsBa::Slicer::RestApi
{
// MSVC's boost::pfr treats types inside an unnamed namespace as local types
// (error C7631), so every struct handed to SerializeValue lives at this
// named-namespace scope instead of the anonymous namespace below.

/** @brief Serializable liveness probe payload. */
struct HealthView
{
    std::string status;            ///< Always "ok" while the process is serving.
    long long uptime_seconds = 0;  ///< Seconds since the server started listening.
};

/** @brief Serializable pipeline-kind list item. */
struct KindEntry
{
    std::string name;
};
struct KindList
{
    std::vector<KindEntry> kinds;
};

/** @brief Serializable ParamStore save/load result view. */
struct ParamView
{
    int success = 0;
    long long param_id = 0;
    std::string error_message;
    double elapsed_seconds = 0.0;
};

/** @brief Serializable job-collection listing (GET /api/jobs). */
struct JobList
{
    std::vector<JobStatusView> jobs;
    long long count = 0;
};

/** @brief Serializable root-index payload. */
struct Index
{
    std::string service;
    std::string version_endpoint;
    std::string pipelines_endpoint;
    std::string jobs_endpoint;
};

namespace
{
using httplib::Request;
using httplib::Response;

/** @brief Shared handler context. */
struct Ctx
{
    const ServerConfig& cfg;
    JobManager& jobs;
    std::chrono::steady_clock::time_point start;
};

// ---------------------------- reply helpers ----------------------------
void ReplyBody(Response& res, int status, const std::string& body, DataFormat fmt)
{
    res.status = status;
    res.set_content(body, std::string(ContentType(fmt)));
}

template <typename T>
void ReplyValue(Response& res, int status, const T& value, DataFormat fmt, const char* root)
{
    ReplyBody(res, status, SerializeValue(value, fmt, root), fmt);
}

void ReplyError(Response& res, int status, const std::string& message, const std::string& detail, DataFormat fmt)
{
    ReplyValue(res, status, ErrorView{status, message, detail}, fmt, "Error");
}

DataFormat TargetFormat(const Request& req, const ServerConfig& cfg)
{
    return ResponseFormat(req.get_param_value("format"), req.get_header_value("Accept"), cfg.default_format);
}

// Intentionally returns const char*: the result feeds const char* fields of the
// C ABI HsBaParamStoreConn_t in the same statement and only borrows from the
// argument, so no C++-style owning return type applies here.
const char* NullIfEmpty(const std::string& s)
{
    return s.empty() ? nullptr : s.c_str();
}

void FillConn(const ServerConfig& cfg, HsBaParamStoreConn_t& conn)
{
    conn.backend = cfg.paramstore_backend;
    conn.sqlite_path = NullIfEmpty(cfg.paramstore_sqlite_path);
    conn.host = NullIfEmpty(cfg.paramstore_host);
    conn.user = NullIfEmpty(cfg.paramstore_user);
    conn.password = NullIfEmpty(cfg.paramstore_password);
    conn.database = NullIfEmpty(cfg.paramstore_database);
    conn.port = static_cast<unsigned int>(cfg.paramstore_port);
    conn.crypto_password = NullIfEmpty(cfg.paramstore_crypto_password);
}

bool ReadFileBytes(const std::string& path, std::string& out)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        return false;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

// Parse an optional non-negative integer query value; falls back to @p def.
std::size_t ParseSizeParam(const Request& req, const char* name, std::size_t def)
{
    const std::string raw = req.get_param_value(name);
    if (raw.empty())
    {
        return def;
    }
    char* end = nullptr;
    const unsigned long parsed = std::strtoul(raw.c_str(), &end, 10);
    if (end == raw.c_str() || (end != nullptr && *end != '\0'))
    {
        return def;
    }
    return static_cast<std::size_t>(parsed);
}

// ---------------------------- handlers ----------------------------
void HandleHealth(const Request& req, Response& res, const Ctx& ctx)
{
    const DataFormat fmt = TargetFormat(req, ctx.cfg);
    const auto uptime = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - ctx.start);
    ReplyValue(res, 200, HealthView{"ok", static_cast<long long>(uptime.count())}, fmt, "Health");
}

void HandleVersion(const Request& req, Response& res, const Ctx& ctx)
{
    const DataFormat fmt = TargetFormat(req, ctx.cfg);
    char* raw = IsXml(fmt) ? HsBaGetVersionXml() : HsBaGetVersionJson();
    if (!raw)
    {
        ReplyError(res, 500, "version unavailable", "", fmt);
        return;
    }
    const std::string body(raw);
    HsBaFreeVersionString(raw);
    ReplyBody(res, 200, body, fmt);
}

void HandleListPipelines(const Request& req, Response& res, const Ctx& ctx)
{
    const DataFormat fmt = TargetFormat(req, ctx.cfg);
    KindList list;
    for (int i = 0; i < HSBA_PIPELINE_KIND_COUNT; ++i)
    {
        list.kinds.push_back(KindEntry{std::string(KindName(static_cast<HsBaPipelineKind>(i)))});
    }
    ReplyValue(res, 200, list, fmt, "Pipelines");
}

void HandleDefaultConfig(const Request& req, Response& res, const Ctx& ctx, const std::string& kind_name)
{
    const DataFormat fmt = TargetFormat(req, ctx.cfg);
    const HsBaPipelineKind kind = ParseKind(kind_name);
    if (!IsValidKind(kind))
    {
        ReplyError(res, 404, "unknown pipeline kind", kind_name, fmt);
        return;
    }
    void* config = CreateDefaultConfig(kind);
    const std::string body = SerializeConfig(ConfigTypeInfo(kind), config, fmt);
    DestroyConfig(kind, config);
    ReplyBody(res, 200, body, fmt);
}

void HandleSubmitJob(const Request& req, Response& res, Ctx& ctx, const std::string& kind_name)
{
    const DataFormat outfmt = TargetFormat(req, ctx.cfg);
    // The kind may come from the path (/api/jobs/{kind}) or the ?kind= query.
    const std::string kind_arg = kind_name.empty() ? req.get_param_value("kind") : kind_name;
    const HsBaPipelineKind kind = ParseKind(kind_arg);
    if (!IsValidKind(kind))
    {
        ReplyError(res, 400, "missing or unknown pipeline kind", kind_arg, outfmt);
        return;
    }

    const DataFormat infmt = RequestBodyFormat(req.get_header_value("Content-Type"));
    void* config = CreateDefaultConfig(kind);
    StringArena arena;
    try
    {
        DeserializeConfig(ConfigTypeInfo(kind), req.body, infmt, config, arena);
    }
    catch (const std::exception& e)
    {
        DestroyConfig(kind, config);
        ReplyError(res, 400, "invalid config body", e.what(), outfmt);
        return;
    }

    // Submit takes ownership of config + arena (and frees them if the queue is full).
    const std::string id = ctx.jobs.Submit(kind, config, std::move(arena));
    if (id.empty())
    {
        ReplyError(res, 429, "job queue full", "max_jobs reached", outfmt);
        return;
    }
    JobStatusView view;
    ctx.jobs.Status(id, view);
    res.set_header("Location", ("/api/jobs/" + id).c_str());
    ReplyValue(res, 202, view, outfmt, "Job");
}

void HandleJobStatus(const Request& req, Response& res, Ctx& ctx, const std::string& id)
{
    const DataFormat fmt = TargetFormat(req, ctx.cfg);
    JobStatusView view;
    if (!ctx.jobs.Status(id, view))
    {
        ReplyError(res, 404, "job not found", id, fmt);
        return;
    }
    ReplyValue(res, 200, view, fmt, "Job");
}

void HandleJobResult(const Request& req, Response& res, Ctx& ctx, const std::string& id)
{
    const DataFormat fmt = TargetFormat(req, ctx.cfg);
    SliceOutcome outcome;
    if (!ctx.jobs.Outcome(id, outcome))
    {
        if (!ctx.jobs.Exists(id))
        {
            ReplyError(res, 404, "job not found", id, fmt);
        }
        else
        {
            ReplyError(res, 409, "job not finished successfully", id, fmt);
        }
        return;
    }
    if (!outcome.inline_content.empty())
    {
        res.status = 200;
        res.set_content(outcome.inline_content, "text/plain; charset=utf-8");
        return;
    }
    if (!outcome.output_path.empty())
    {
        std::string data;
        if (ReadFileBytes(outcome.output_path, data))
        {
            res.status = 200;
            res.set_content(data, "application/octet-stream");
            res.set_header("Content-Disposition", ("attachment; filename=\"" + outcome.output_path + "\"").c_str());
            return;
        }
        ReplyError(res, 404, "output file not readable", outcome.output_path, fmt);
        return;
    }
    ReplyError(res, 404, "no downloadable result for this pipeline", id, fmt);
}

void HandleConfigConvert(const Request& req, Response& res, Ctx& ctx)
{
    const DataFormat outfmt = TargetFormat(req, ctx.cfg);
    const std::string kind_arg = req.get_param_value("kind");
    const HsBaPipelineKind kind = ParseKind(kind_arg);
    if (!IsValidKind(kind))
    {
        ReplyError(res, 400, "missing or unknown pipeline kind", kind_arg, outfmt);
        return;
    }
    // "?to=proto" flips direction: a JSON/XML body is encoded into proto bytes;
    // otherwise the request body is treated as proto bytes and rendered as text.
    const std::string to = req.get_param_value("to");
    try
    {
        if (to == "proto")
        {
            const DataFormat infmt = RequestBodyFormat(req.get_header_value("Content-Type"));
            const std::vector<unsigned char> bytes = ConfigToProto(kind, req.body, infmt);
            res.status = 200;
            res.set_content(reinterpret_cast<const char*>(bytes.data()), bytes.size(), "application/x-protobuf");
            return;
        }
        const std::string text = ConfigFromProto(kind, req.body.data(), req.body.size(), outfmt);
        ReplyBody(res, 200, text, outfmt);
    }
    catch (const std::exception& e)
    {
        ReplyError(res, 400, "config conversion failed", e.what(), outfmt);
    }
}

void HandleListJobs(const Request& req, Response& res, Ctx& ctx)
{
    const DataFormat fmt = TargetFormat(req, ctx.cfg);
    const std::string state = req.get_param_value("state");
    const std::size_t offset = ParseSizeParam(req, "offset", 0);
    const std::size_t limit = ParseSizeParam(req, "limit", 0);
    JobList list;
    list.jobs = ctx.jobs.List(state, offset, limit);
    list.count = static_cast<long long>(list.jobs.size());
    ReplyValue(res, 200, list, fmt, "Jobs");
}

void HandleDeleteJob(const Request& req, Response& res, Ctx& ctx, const std::string& id)
{
    const DataFormat fmt = TargetFormat(req, ctx.cfg);
    switch (ctx.jobs.Remove(id))
    {
    case JobManager::RemoveResult::Removed:
        res.status = 204;  // no body on success
        break;
    case JobManager::RemoveResult::NotFound:
        ReplyError(res, 404, "job not found", id, fmt);
        break;
    case JobManager::RemoveResult::Running:
        ReplyError(res, 409, "job is running and cannot be deleted", id, fmt);
        break;
    }
}

void HandleParamSave(const Request& req, Response& res, Ctx& ctx, const std::string& kind_name, const std::string& key)
{
    const DataFormat outfmt = TargetFormat(req, ctx.cfg);
    if (!ctx.cfg.paramstore_enabled)
    {
        ReplyError(res, 503, "paramstore not configured", "", outfmt);
        return;
    }
    const HsBaPipelineKind kind = ParseKind(kind_name);
    if (!IsValidKind(kind))
    {
        ReplyError(res, 400, "unknown pipeline kind", kind_name, outfmt);
        return;
    }
    const DataFormat infmt = RequestBodyFormat(req.get_header_value("Content-Type"));
    void* config = CreateDefaultConfig(kind);
    StringArena arena;
    try
    {
        DeserializeConfig(ConfigTypeInfo(kind), req.body, infmt, config, arena);
    }
    catch (const std::exception& e)
    {
        DestroyConfig(kind, config);
        ReplyError(res, 400, "invalid config body", e.what(), outfmt);
        return;
    }

    HsBaParamStoreConn_t conn{};
    FillConn(ctx.cfg, conn);
    HsBaParamStoreResult_t r = HsBaSavePipelineParams(&conn, kind, nullptr, key.c_str(), config);
    ParamView view;
    view.success = r.success;
    view.param_id = r.param_id;
    view.error_message = r.error_message ? r.error_message : "";
    view.elapsed_seconds = r.elapsed_seconds;
    HsBaFreeParamStoreResult(&r);
    DestroyConfig(kind, config);  // string fields are arena-owned; not HsBaFree'd
    ReplyValue(res, view.success ? 200 : 400, view, outfmt, "ParamResult");
}

void HandleParamLoad(const Request& req, Response& res, Ctx& ctx, const std::string& kind_name, const std::string& key)
{
    const DataFormat fmt = TargetFormat(req, ctx.cfg);
    if (!ctx.cfg.paramstore_enabled)
    {
        ReplyError(res, 503, "paramstore not configured", "", fmt);
        return;
    }
    const HsBaPipelineKind kind = ParseKind(kind_name);
    if (!IsValidKind(kind))
    {
        ReplyError(res, 400, "unknown pipeline kind", kind_name, fmt);
        return;
    }
    void* config = CreateDefaultConfig(kind);
    HsBaParamStoreConn_t conn{};
    FillConn(ctx.cfg, conn);
    HsBaParamStoreResult_t r = HsBaLoadPipelineParams(&conn, kind, nullptr, key.c_str(), config);
    if (r.success)
    {
        const std::string body = SerializeConfig(ConfigTypeInfo(kind), config, fmt);
        HsBaFreeParamStoreResult(&r);
        HsBaFreeLoadedPipelineConfig(kind, config);  // frees the malloc strings Load produced
        DestroyConfig(kind, config);
        ReplyBody(res, 200, body, fmt);
        return;
    }
    ParamView view;
    view.success = 0;
    view.error_message = r.error_message ? r.error_message : "key not found";
    view.elapsed_seconds = r.elapsed_seconds;
    HsBaFreeParamStoreResult(&r);
    HsBaFreeLoadedPipelineConfig(kind, config);
    DestroyConfig(kind, config);
    ReplyValue(res, 404, view, fmt, "ParamResult");
}

void RegisterRoutes(httplib::Server& svr, Ctx& ctx)
{
    svr.set_payload_max_length(64ull * 1024 * 1024);  // generous cap for embedded model configs

    svr.Get("/api/health", [&ctx](const Request& req, Response& res) { HandleHealth(req, res, ctx); });
    svr.Get("/api/version", [&ctx](const Request& req, Response& res) { HandleVersion(req, res, ctx); });
    svr.Get("/api/pipelines", [&ctx](const Request& req, Response& res) { HandleListPipelines(req, res, ctx); });
    // Spec-verbatim alias for the pipeline-kind listing (see the phase-C endpoint table).
    svr.Get("/api/kinds", [&ctx](const Request& req, Response& res) { HandleListPipelines(req, res, ctx); });
    svr.Get(R"(/api/pipelines/([^/]+)/default-config)",
            [&ctx](const Request& req, Response& res) { HandleDefaultConfig(req, res, ctx, req.matches[1].str()); });
    // Spec-verbatim alias: GET /api/config/default/{kind}.
    svr.Get(R"(/api/config/default/([^/]+))",
            [&ctx](const Request& req, Response& res) { HandleDefaultConfig(req, res, ctx, req.matches[1].str()); });
    svr.Post("/api/config/convert", [&ctx](const Request& req, Response& res) { HandleConfigConvert(req, res, ctx); });
    svr.Post("/api/jobs", [&ctx](const Request& req, Response& res) { HandleSubmitJob(req, res, ctx, std::string()); });
    svr.Post(R"(/api/jobs/([^/]+))",
             [&ctx](const Request& req, Response& res) { HandleSubmitJob(req, res, ctx, req.matches[1].str()); });
    svr.Get("/api/jobs", [&ctx](const Request& req, Response& res) { HandleListJobs(req, res, ctx); });
    svr.Get(R"(/api/jobs/([^/]+)/result)",
            [&ctx](const Request& req, Response& res) { HandleJobResult(req, res, ctx, req.matches[1].str()); });
    svr.Get(R"(/api/jobs/([^/]+))",
            [&ctx](const Request& req, Response& res) { HandleJobStatus(req, res, ctx, req.matches[1].str()); });
    svr.Delete(R"(/api/jobs/([^/]+))",
               [&ctx](const Request& req, Response& res) { HandleDeleteJob(req, res, ctx, req.matches[1].str()); });
    svr.Put(R"(/api/params/([^/]+)/([^/]+))", [&ctx](const Request& req, Response& res)
            { HandleParamSave(req, res, ctx, req.matches[1].str(), req.matches[2].str()); });
    // Spec-verbatim alias: POST /api/params/{kind}?key=  (upsert, key from query).
    svr.Post(R"(/api/params/([^/]+))",
             [&ctx](const Request& req, Response& res)
             {
                 const DataFormat fmt = TargetFormat(req, ctx.cfg);
                 const std::string key = req.get_param_value("key");
                 if (key.empty())
                 {
                     ReplyError(res, 400, "missing ?key= query parameter", req.matches[1].str(), fmt);
                     return;
                 }
                 HandleParamSave(req, res, ctx, req.matches[1].str(), key);
             });
    svr.Get(R"(/api/params/([^/]+)/([^/]+))", [&ctx](const Request& req, Response& res)
            { HandleParamLoad(req, res, ctx, req.matches[1].str(), req.matches[2].str()); });

    // Root: a tiny machine-readable index (keeps the JSON/XML contract).
    svr.Get("/",
            [&ctx](const Request& req, Response& res)
            {
                const DataFormat fmt = TargetFormat(req, ctx.cfg);
                ReplyValue(res, 200, Index{"HsBaSlicerRestServer", "/api/version", "/api/pipelines", "/api/jobs"}, fmt,
                           "Index");
            });
}

std::atomic<httplib::Server*> g_server{};

void SignalHandler(int)
{
    if (httplib::Server* s = g_server.load())
    {
        s->stop();
    }
}
}  // namespace

int RunServer(const ServerConfig& cfg)
{
    initialize();

    httplib::Server svr;
    JobManager jobs(cfg.max_concurrent_jobs, cfg.max_jobs, std::chrono::seconds(cfg.result_ttl_seconds));
    Ctx ctx{cfg, jobs, std::chrono::steady_clock::now()};
    RegisterRoutes(svr, ctx);

    g_server.store(&svr);
    std::signal(SIGINT, SignalHandler);
#ifdef SIGTERM
    std::signal(SIGTERM, SignalHandler);
#endif

    std::fprintf(stderr, "HsBaSlicerRestServer listening on http://%s:%d (default format: %s)\n", cfg.host.c_str(),
                 cfg.port, cfg.default_format.c_str());

    if (!svr.listen(cfg.host.c_str(), cfg.port))
    {
        g_server.store(nullptr);
        return 1;
    }
    g_server.store(nullptr);
    return 0;
}

}  // namespace HsBa::Slicer::RestApi
