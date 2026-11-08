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
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <httplib.h>

#include "DllHsBaSlicer/geometry_export.h"
#include "DllHsBaSlicer/initialize.h"
#include "DllHsBaSlicer/param_store_pipeline.h"
#include "DllHsBaSlicer/version_info.h"

#include "auth_gate.hpp"
#include "config_bridge.hpp"
#include "content_negotiate.hpp"
#include "job_manager.hpp"
#include "package_reader.hpp"
#include "pipeline_executor.hpp"
#include "routes.hpp"
#include "serialize_value.hpp"
#include "server_config.hpp"
#include "upload_bridge.hpp"
#include "upload_store.hpp"

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

/** @brief Serializable upload-pool listing (GET /api/uploads). */
struct UploadList
{
    std::vector<UploadInfo> uploads;
    long long count = 0;
};

/** @brief Serializable geometry overview (GET /api/jobs/{id}/geometry). */
struct GeometrySummaryView
{
    std::string id;                    ///< Job identifier.
    long long geometry_id = 0;         ///< Snapshot handle a native host may query directly.
    int layer_count = 0;               ///< Layers held by the snapshot.
    std::vector<double> bounds;        ///< min_x, min_y, max_x, max_y over every layer.
    std::vector<double> z_heights;     ///< Per-layer platform height, only with ?full=1.
    std::vector<double> layer_bounds;  ///< Four values per layer, only with ?full=1.
    int has_path = 0;                  ///< 1 = a continuous path can be read from /geometry/path.
};

/** @brief One contour/infill/support ring of a layer (?xy is flat to keep the body small). */
struct RingView
{
    int point_count = 0;  ///< Vertices stored in @ref xy.
    int closed = 0;       ///< 1 = closed contour, 0 = open polyline.
    std::vector<double> xy;
};

/** @brief Serializable single-layer geometry (GET /api/jobs/{id}/geometry/{layer}). */
struct LayerGeometryView
{
    std::string id;              ///< Job identifier.
    int layer_index = 0;         ///< Zero-based layer.
    double z_height = 0.0;       ///< Platform height of the layer (mm).
    std::vector<double> bounds;  ///< min_x, min_y, max_x, max_y of this layer.
    std::vector<RingView> outlines;
    std::vector<RingView> fills;
    std::vector<RingView> supports;
};

/** @brief Serializable continuous toolpath (GET /api/jobs/{id}/geometry/path). */
struct GeometryPathView
{
    std::string id;
    long long geometry_id = 0;
    int point_count = 0;
    std::vector<double> xyz;      ///< Flat x,y,z triples in millimetres.
    std::vector<double> bounds;   ///< min_x, min_y, max_x, max_y of the polyline.
    std::vector<double> z_range;  ///< min_z, max_z.
};

/** @brief One file of a result package, with the content type this server will answer with. */
struct PackageEntryView
{
    std::string path;                 ///< Name inside the archive, as stored.
    long long size = 0;               ///< Bytes inside the archive.
    long long uncompressed_size = 0;  ///< Bytes after expansion.
    std::string media_type;           ///< Type GET /api/jobs/{id}/package/{entry} reports.
};

/** @brief Serializable entry listing of a result package (GET /api/jobs/{id}/package). */
struct PackageListingView
{
    std::string id;       ///< Job identifier.
    std::string name;     ///< Archive file name, without the server-side directory.
    int entry_count = 0;  ///< Size of @ref entries.
    std::vector<PackageEntryView> entries;
};

/** @brief Serializable cancellation acknowledgement (POST /api/jobs/{id}/cancel). */
struct CancelView
{
    std::string id;
    std::string state;   ///< State the job is heading to ("cancelled").
    std::string detail;  ///< What the server actually did.
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

/**
 * @brief Last path component of a server-side file, i.e. the name a client should save it as.
 * @param path UTF-8 path reported by a pipeline result.
 * @return The bare file name, or an empty string when it cannot be quoted into a header.
 */
std::string BaseFileName(const std::string& path)
{
    const std::size_t sep = path.find_last_of("/\\");
    const std::string name = (sep == std::string::npos) ? path : path.substr(sep + 1);
    // A double quote would end the quoted-string early and a backslash or control character
    // would corrupt it, so such names are dropped instead of escaped into the header.
    return name.find_first_of("\"\\\r\n") == std::string::npos ? name : std::string();
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

// Persist a multipart part verbatim; the parent directory must already exist.
bool WriteWholeFile(const std::string& path, const std::string& content)
{
    std::ofstream out(std::filesystem::u8path(path), std::ios::binary | std::ios::trunc);
    if (!out)
    {
        return false;
    }
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    return out.good();
}

// One multipart part, whichever bucket cpp-httplib filed it in: a part that carries a
// filename lands in MultipartFormData::files, a plain text field in ::fields.
struct MultipartPart
{
    std::string content;
    std::string filename;
    std::string content_type;
};

// First multipart part with the given name, or nullopt. The typed accessors keep this
// independent of how a given cpp-httplib version stores parts (Request::files in older
// releases, Request::form.files/fields since the MultipartFormData split).
std::optional<MultipartPart> FindPart(const Request& req, const std::string& name)
{
    if (req.form.has_file(name))
    {
        const httplib::FormData file = req.form.get_file(name);
        return MultipartPart{file.content, file.filename, file.content_type};
    }
    if (req.form.has_field(name))
    {
        // Text fields carry no filename and usually no content type (RFC 7578).
        return MultipartPart{req.form.get_field(name), "", ""};
    }
    return std::nullopt;
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

// Strict unsigned decimal parse for a path segment (a layer index must not silently
// degrade to 0, otherwise a typo would return the wrong layer instead of an error).
bool ParseIndex(const std::string& raw, int& out)
{
    if (raw.empty() || raw.size() > 9)
    {
        return false;
    }
    int value = 0;
    for (char c : raw)
    {
        if (c < '0' || c > '9')
        {
            return false;
        }
        value = value * 10 + (c - '0');
    }
    out = value;
    return true;
}

// Whether ?geometry=1 (or =true) was asked for on a submission request.
bool WantsGeometry(const Request& req)
{
    const std::string raw = req.get_param_value("geometry");
    return raw == "1" || raw == "true";
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

// ---------------------------- multipart job submission ----------------------------
// One request carries both halves of a slicing job: a "config" part (JSON/XML) and a
// "model" part (the mesh/brep file). The file is stored under upload_dir/<job-id>/ and
// its path is injected into the config's reflected model_path field, so the pipeline
// sees exactly what a locally-launched run would.
void SubmitMultipartJob(const Request& req, Response& res, Ctx& ctx, HsBaPipelineKind kind, DataFormat outfmt)
{
    if (!ctx.uploads.Enabled())
    {
        ReplyError(res, 503, "model upload disabled", "set [upload] dir in restapi.ini", outfmt);
        return;
    }
    const std::optional<MultipartPart> config_part = FindPart(req, "config");
    if (!config_part)
    {
        ReplyError(res, 400, "missing config part", R"(expected a multipart part named "config")", outfmt);
        return;
    }
    const std::optional<MultipartPart> model_part = FindPart(req, "model");
    if (!model_part)
    {
        ReplyError(res, 400, "missing model part", R"(expected a multipart part named "model")", outfmt);
        return;
    }

    const long long max_file_bytes = static_cast<long long>(ctx.cfg.upload_max_file_mb) * 1024 * 1024;
    if (static_cast<long long>(model_part->content.size()) > max_file_bytes)
    {
        ReplyError(res, 413, "model file too large", "exceeds [upload] max_file_mb", outfmt);
        return;
    }

    const std::string job_id = ctx.jobs.NextId();
    const std::string safe_name = SanitizeUploadName(model_part->filename);
    const std::string path = RenderUploadPath(ctx.cfg.upload_dir, job_id, safe_name);

    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::u8path(path).parent_path(), ec);
    if (ec || !WriteWholeFile(path, model_part->content))
    {
        const std::filesystem::path stored = std::filesystem::u8path(path);
        std::filesystem::remove(stored, ec);
        std::filesystem::remove(stored.parent_path(), ec);
        // Report only the file name: the directory the server stores uploads into is none of the
        // caller's business, matching the BaseFileName posture used elsewhere in this file.
        ReplyError(res, 500, "could not store model file", BaseFileName(path), outfmt);
        return;
    }

    const std::string upload_id = ctx.uploads.Adopt(job_id, model_part->filename, safe_name, path,
                                                    static_cast<long long>(model_part->content.size()));
    if (upload_id.empty())
    {
        // Nothing was registered, so clean up the file (and the now-empty per-job
        // directory) ourselves; remove() only succeeds on an empty directory.
        const std::filesystem::path stored = std::filesystem::u8path(path);
        std::filesystem::remove(stored, ec);
        std::filesystem::remove(stored.parent_path(), ec);
        ReplyError(res, 503, "upload pool full", "raise [upload] max_files or delete old uploads", outfmt);
        return;
    }

    const DataFormat infmt = RequestBodyFormat(config_part->content_type);
    void* config = CreateDefaultConfig(kind);
    StringArena arena;
    bool bound = false;
    try
    {
        DeserializeConfig(ConfigTypeInfo(kind), config_part->content, infmt, config, arena);
        bound = BindUploadedModel(ConfigTypeInfo(kind), config, path, model_part->filename, arena);
    }
    catch (const std::exception& e)
    {
        DestroyConfig(kind, config);
        ctx.uploads.Remove(upload_id);
        ReplyError(res, 400, "invalid config body", e.what(), outfmt);
        return;
    }
    if (!bound)
    {
        DestroyConfig(kind, config);
        ctx.uploads.Remove(upload_id);
        ReplyError(res, 400, "pipeline kind accepts no model file", std::string(KindName(kind)), outfmt);
        return;
    }

    // Submit takes ownership of config + arena (and frees them if the queue is full).
    if (WantsGeometry(req))
    {
        // Capturing layer geometry is a runtime switch rather than a reflected parameter,
        // so it goes straight into the struct tail and never reaches the config JSON,
        // the proto message or the parameter wide table.
        SetCaptureGeometry(kind, config, 1);
    }
    // Pin the file before handing the job to the queue: once Submit returns, a worker may run
    // the job straight to a terminal state whose MarkTerminal already cleared the reference,
    // so pinning afterwards would re-pin a finished file and orphan it forever.
    ctx.uploads.MarkInUse(job_id);
    const std::string id = ctx.jobs.Submit(kind, config, std::move(arena), job_id);
    if (id.empty())
    {
        // Queue full: the job never got enqueued, so release the pin before discarding the upload.
        ctx.uploads.ReleaseByJob(job_id);
        ctx.uploads.Remove(upload_id);
        ReplyError(res, 429, "job queue full", "max_jobs reached", outfmt);
        return;
    }

    JobStatusView view;
    ctx.jobs.Status(id, view);
    res.set_header("Location", ("/api/jobs/" + id).c_str());
    ReplyValue(res, 202, view, outfmt, "Job");
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

    if (req.is_multipart_form_data())
    {
        SubmitMultipartJob(req, res, ctx, kind, outfmt);
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
    if (WantsGeometry(req))
    {
        SetCaptureGeometry(kind, config, 1);
    }
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
        else if (JobStatusView status; ctx.jobs.Status(id, status) && status.state == "cancelled")
        {
            // A cancelled run produced partial work at best, so it is reported apart
            // from a plain pipeline failure instead of as "not finished successfully".
            ReplyError(res, 409, "job was cancelled", id, fmt);
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
            // Only the file name goes into the header: the server-side directory must not leak
            // into the download, and an unusable name falls back to the job id.
            const std::string download_name = BaseFileName(outcome.output_path);
            res.set_header("Content-Disposition",
                           "attachment; filename=\"" + (download_name.empty() ? id : download_name) + "\"");
            return;
        }
        ReplyError(res, 404, "output file not readable", outcome.output_path, fmt);
        return;
    }
    ReplyError(res, 404, "no downloadable result for this pipeline", id, fmt);
}

// ---------------------------- layer geometry and cancellation ----------------------------
// Both geometry endpoints are thin adapters over the same C ABI the native GUIs call:
// the snapshot pool stays inside the DLL and this layer only turns rings into
// JSON/XML. Geometry exists for a job once its run reached the final stages, which is
// exactly when the job turns terminal, so a still-queued or running job has nothing.

// Turn one C ring array into serializable views; the arrays live in the single block
// handed out by HsBaGetLayerGeometry, so the copies below must precede its release.
void AppendRings(std::vector<RingView>& dst, const HsBaRing_t* rings, int count)
{
    if (count > 0)
    {
        dst.reserve(dst.size() + static_cast<std::size_t>(count));
    }
    for (int i = 0; i < count; ++i)
    {
        // A ring without coordinates is reported as empty rather than dropped, so the
        // caller can still align its index with the snapshot's ring array.
        const int points = (rings[i].xy && rings[i].point_count > 0) ? rings[i].point_count : 0;
        RingView ring;
        ring.point_count = points;
        ring.closed = points > 0 ? rings[i].closed : 0;
        if (points > 0)
        {
            ring.xy.assign(rings[i].xy, rings[i].xy + 2ull * static_cast<std::size_t>(points));
        }
        dst.push_back(std::move(ring));
    }
}

void HandleJobGeometry(const Request& req, Response& res, Ctx& ctx, const std::string& id)
{
    const DataFormat fmt = TargetFormat(req, ctx.cfg);
    long long geometry_id = 0;
    if (!ctx.jobs.GeometryId(id, geometry_id))
    {
        ReplyError(res, 404, ctx.jobs.Exists(id) ? "job captured no layer geometry" : "job not found",
                   ctx.jobs.Exists(id) ? "submit with ?geometry=1" : id, fmt);
        return;
    }
    const int layers = HsBaGeometryLayerCount(geometry_id);
    if (layers <= 0)
    {
        ReplyError(res, 404, "geometry snapshot is no longer available", id, fmt);
        return;
    }

    const bool full = req.get_param_value("full") == "1" || req.get_param_value("full") == "true";
    GeometrySummaryView view;
    view.id = id;
    view.geometry_id = geometry_id;
    view.layer_count = layers;
    // Telling the client whether a path exists costs one pool lookup and saves it a
    // request that can only ever answer 404 for a non-spiral run.
    view.has_path = HsBaGeometryPathPointCount(geometry_id) > 0 ? 1 : 0;
    double lo[2] = {std::numeric_limits<double>::max(), std::numeric_limits<double>::max()};
    double hi[2] = {std::numeric_limits<double>::lowest(), std::numeric_limits<double>::lowest()};
    bool saw_bounds = false;
    for (int i = 0; i < layers; ++i)
    {
        HsBaLayerGeometry_t geom{};
        if (!HsBaGetLayerGeometry(geometry_id, i, &geom))
        {
            continue;
        }
        for (int k = 0; k < 2; ++k)
        {
            lo[k] = (std::min)(lo[k], geom.bounds_min[k]);
            hi[k] = (std::max)(hi[k], geom.bounds_max[k]);
        }
        saw_bounds = true;
        if (full)
        {
            view.z_heights.push_back(geom.z_height);
            const double per_layer[4] = {geom.bounds_min[0], geom.bounds_min[1], geom.bounds_max[0],
                                         geom.bounds_max[1]};
            view.layer_bounds.insert(view.layer_bounds.end(), std::begin(per_layer), std::end(per_layer));
        }
        HsBaFreeLayerGeometry(&geom);
    }
    if (saw_bounds)
    {
        view.bounds = {lo[0], lo[1], hi[0], hi[1]};
    }
    ReplyValue(res, 200, view, fmt, "Geometry");
}

void HandleJobLayerGeometry(const Request& req, Response& res, Ctx& ctx, const std::string& id,
                            const std::string& layer_raw)
{
    const DataFormat fmt = TargetFormat(req, ctx.cfg);
    int layer = 0;
    if (!ParseIndex(layer_raw, layer))
    {
        ReplyError(res, 400, "layer index must be a non-negative integer", layer_raw, fmt);
        return;
    }
    long long geometry_id = 0;
    if (!ctx.jobs.GeometryId(id, geometry_id))
    {
        ReplyError(res, 404, ctx.jobs.Exists(id) ? "job captured no layer geometry" : "job not found",
                   ctx.jobs.Exists(id) ? "submit with ?geometry=1" : id, fmt);
        return;
    }
    const int layers = HsBaGeometryLayerCount(geometry_id);
    if (layers <= 0)
    {
        ReplyError(res, 404, "geometry snapshot is no longer available", id, fmt);
        return;
    }
    if (layer >= layers)
    {
        ReplyError(res, 404, "layer index out of range", "layer_count=" + std::to_string(layers), fmt);
        return;
    }
    HsBaLayerGeometry_t geom{};
    if (!HsBaGetLayerGeometry(geometry_id, layer, &geom))
    {
        ReplyError(res, 404, "geometry snapshot is no longer available", id, fmt);
        return;
    }
    LayerGeometryView view;
    view.id = id;
    view.layer_index = geom.layer_index;
    view.z_height = geom.z_height;
    view.bounds = {geom.bounds_min[0], geom.bounds_min[1], geom.bounds_max[0], geom.bounds_max[1]};
    AppendRings(view.outlines, geom.outlines, geom.outline_count);
    AppendRings(view.fills, geom.fills, geom.fill_count);
    AppendRings(view.supports, geom.supports, geom.support_count);
    HsBaFreeLayerGeometry(&geom);
    ReplyValue(res, 200, view, fmt, "LayerGeometry");
}

void HandleJobGeometryPath(const Request& req, Response& res, Ctx& ctx, const std::string& id)
{
    const DataFormat fmt = TargetFormat(req, ctx.cfg);
    long long geometry_id = 0;
    if (!ctx.jobs.GeometryId(id, geometry_id))
    {
        ReplyError(res, 404, ctx.jobs.Exists(id) ? "job captured no layer geometry" : "job not found",
                   ctx.jobs.Exists(id) ? "submit with ?geometry=1" : id, fmt);
        return;
    }
    if (HsBaGeometryLayerCount(geometry_id) <= 0)
    {
        ReplyError(res, 404, "geometry snapshot is no longer available", id, fmt);
        return;
    }
    HsBaGeometryPath_t path{};
    if (!HsBaGetGeometryPath(geometry_id, &path))
    {
        // The run only deposited per-layer rings, so there is no polyline to hand out.
        ReplyError(res, 404, "job emitted no continuous path", "spiral_mode=1", fmt);
        return;
    }
    GeometryPathView view;
    view.id = id;
    view.geometry_id = geometry_id;
    view.point_count = path.point_count;
    // The coordinates live inside the block HsBaGetGeometryPath allocated, so the copy
    // must happen before the release below.
    view.xyz.assign(path.xyz, path.xyz + 3ull * static_cast<std::size_t>(path.point_count));
    view.bounds = {path.bounds_min[0], path.bounds_min[1], path.bounds_max[0], path.bounds_max[1]};
    view.z_range = {path.z_min, path.z_max};
    HsBaFreeGeometryPath(&path);
    ReplyValue(res, 200, view, fmt, "GeometryPath");
}

// ---------------------------- result package entries ----------------------------
// A package is whatever a pipeline wrote to output_path as an archive (SLA layer images, the
// SLS/SLM/LOM/TDP zip bundles). Reading it costs nothing in the job machinery: the archive is on
// disk and already finished, so these routes open it per request and keep no state.

/**
 * @brief Shared front of both package routes: the finished archive of @p id, or a reply.
 * @return true only when @p outcome holds a non-empty output_path a package can be read from.
 *
 * The failure rungs are deliberately the ones of HandleJobResult, in the same order, so a client
 * that already handled /result needs no new error vocabulary for /package.
 */
bool PackageSource(const Request& req, Response& res, Ctx& ctx, const std::string& id, SliceOutcome& outcome)
{
    const DataFormat fmt = TargetFormat(req, ctx.cfg);
    if (!ctx.jobs.Outcome(id, outcome))
    {
        if (!ctx.jobs.Exists(id))
        {
            ReplyError(res, 404, "job not found", id, fmt);
        }
        else if (JobStatusView status; ctx.jobs.Status(id, status) && status.state == "cancelled")
        {
            ReplyError(res, 409, "job was cancelled", id, fmt);
        }
        else
        {
            ReplyError(res, 409, "job not finished successfully", id, fmt);
        }
        return false;
    }
    if (outcome.output_path.empty())
    {
        // An inline G-code answer, or no answer at all: there is no archive to open either way.
        ReplyError(res, 404, "result is not a package", outcome.inline_content.empty() ? id : "inline result", fmt);
        return false;
    }
    return true;
}

void HandleJobPackage(const Request& req, Response& res, Ctx& ctx, const std::string& id)
{
    const DataFormat fmt = TargetFormat(req, ctx.cfg);
    SliceOutcome outcome;
    if (!PackageSource(req, res, ctx, id, outcome))
    {
        return;
    }
    std::vector<PackageEntryInfo> entries;
    std::string error;
    if (!ReadPackageEntries(outcome.output_path, entries, error))
    {
        // The reason names nothing a client may not already see, and the detail stays the bare file
        // name: the directory the server slices into is none of the caller's business.
        ReplyError(res, 404, "package not readable", BaseFileName(outcome.output_path), fmt);
        return;
    }
    PackageListingView view;
    view.id = id;
    view.name = BaseFileName(outcome.output_path);
    view.entry_count = static_cast<int>(entries.size());
    view.entries.reserve(entries.size());
    for (const PackageEntryInfo& entry : entries)
    {
        view.entries.push_back(
            PackageEntryView{entry.path, entry.size, entry.uncompressed_size, PackageMediaType(entry.path)});
    }
    ReplyValue(res, 200, view, fmt, "Package");
}

void HandleJobPackageEntry(const Request& req, Response& res, Ctx& ctx, const std::string& id, const std::string& entry)
{
    const DataFormat fmt = TargetFormat(req, ctx.cfg);
    if (!IsSafeEntryName(entry))
    {
        // Checked before the job is even looked up: an unusable name must never reach the archive
        // layer, whatever the rest of the request would have been worth.
        ReplyError(res, 400, "invalid package entry name", entry, fmt);
        return;
    }
    SliceOutcome outcome;
    if (!PackageSource(req, res, ctx, id, outcome))
    {
        return;
    }
    std::string data;
    std::string error;
    if (!ReadPackageEntry(outcome.output_path, entry, data, error))
    {
        if (error == PACKAGE_ENTRY_NOT_FOUND)
        {
            ReplyError(res, 404, std::string(PACKAGE_ENTRY_NOT_FOUND), entry, fmt);
        }
        else if (error == PACKAGE_ENTRY_TOO_LARGE)
        {
            ReplyError(res, 409, std::string(PACKAGE_ENTRY_TOO_LARGE), entry, fmt);
        }
        else
        {
            ReplyError(res, 404, "package not readable", BaseFileName(outcome.output_path), fmt);
        }
        return;
    }
    const std::string media = PackageMediaType(entry);
    res.status = 200;
    res.set_content(std::move(data), media);
    res.set_header("X-Content-Type-Options", "nosniff");
    if (media == "image/svg+xml")
    {
        // A package SVG comes out of an exporter script, so it is about as trusted as any other
        // result byte. Browsers do not run script inside an <img>, and this header is the backstop
        // for every context that does.
        res.set_header("Content-Security-Policy", "default-src 'none'");
    }
}

void HandleCancelJob(const Request& req, Response& res, Ctx& ctx, const std::string& id)
{
    const DataFormat fmt = TargetFormat(req, ctx.cfg);
    switch (ctx.jobs.Cancel(id))
    {
    case CancelResult::Accepted:
        // 202, not 200: the run is still unwinding to its next stage or layer checkpoint.
        ReplyValue(res, 202, CancelView{id, "cancelled", "cancellation requested"}, fmt, "Job");
        break;
    case CancelResult::Removed:
        // The queued job was erased without ever reaching Execute, so release its upload
        // reference here; otherwise the pinned file would never be reclaimed.
        ctx.uploads.ReleaseByJob(id);
        ReplyValue(res, 202, CancelView{id, "cancelled", "queued job removed before it started"}, fmt, "Job");
        break;
    case CancelResult::AlreadyTerminal:
        ReplyError(res, 409, "job already finished", id, fmt);
        break;
    case CancelResult::NotFound:
        ReplyError(res, 404, "job not found", id, fmt);
        break;
    }
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
        // Release the pool reference and detach the association: the uploaded model then follows
        // the upload pool's own retention schedule rather than the (much shorter) job result TTL.
        // A job deleted while still queued never reported a terminal state, so this is also what
        // frees its pinned file from the in_use exemption (Unlink alone would leave it stuck).
        ctx.uploads.ReleaseByJob(id);
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

// ---------------------------- upload pool inspection ----------------------------
void HandleListUploads(const Request& req, Response& res, const Ctx& ctx)
{
    const DataFormat fmt = TargetFormat(req, ctx.cfg);
    if (!ctx.uploads.Enabled())
    {
        ReplyError(res, 503, "model upload disabled", "set [upload] dir in restapi.ini", fmt);
        return;
    }
    UploadList list;
    list.uploads = ctx.uploads.List(req.get_param_value("state"), ParseSizeParam(req, "offset", 0),
                                    ParseSizeParam(req, "limit", 0));
    list.count = static_cast<long long>(list.uploads.size());
    ReplyValue(res, 200, list, fmt, "Uploads");
}

void HandleUploadStatus(const Request& req, Response& res, const Ctx& ctx, const std::string& id)
{
    const DataFormat fmt = TargetFormat(req, ctx.cfg);
    if (!ctx.uploads.Enabled())
    {
        ReplyError(res, 503, "model upload disabled", "set [upload] dir in restapi.ini", fmt);
        return;
    }
    UploadInfo info;
    if (!ctx.uploads.Get(id, info))
    {
        ReplyError(res, 404, "upload not found", id, fmt);
        return;
    }
    ReplyValue(res, 200, info, fmt, "Upload");
}

void HandleDeleteUpload(const Request& req, Response& res, Ctx& ctx, const std::string& id)
{
    const DataFormat fmt = TargetFormat(req, ctx.cfg);
    if (!ctx.uploads.Enabled())
    {
        ReplyError(res, 503, "model upload disabled", "set [upload] dir in restapi.ini", fmt);
        return;
    }
    switch (ctx.uploads.Remove(id))
    {
    case UploadRemoveResult::Removed:
        res.status = 204;  // no body on success
        break;
    case UploadRemoveResult::NotFound:
        ReplyError(res, 404, "upload not found", id, fmt);
        break;
    case UploadRemoveResult::InUse:
        ReplyError(res, 409, "upload is referenced by a queued or running job", id, fmt);
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

// ---------------------------- authorization ----------------------------
// A pre-routing hook rather than per-route wrappers: the credential check has to cover every
// endpoint, including future ones and the regex-routed ones.
void RegisterAuthGate(httplib::Server& svr, Ctx& ctx)
{
    svr.set_pre_routing_handler(
        [&ctx](const Request& req, Response& res) -> httplib::Server::HandlerResponse
        {
            const AuthOutcome outcome =
                Authorize(ctx.cfg, req.path, ctx.cfg.host, req.get_header_value("Authorization"),
                          req.get_header_value("X-API-Key"));
            if (outcome == AuthOutcome::Allowed)
            {
                return httplib::Server::HandlerResponse::Unhandled;
            }
            const bool missing = (outcome == AuthOutcome::MissingCredentials);
            res.set_header("WWW-Authenticate", "Bearer realm=\"HsBaSlicer\"");
            ReplyError(res, 401, missing ? "missing credentials" : "invalid credentials",
                       missing ? R"(send "Authorization: Bearer <token>" or "X-API-Key: <token>")"
                               : "the presented token does not match the configured one",
                       TargetFormat(req, ctx.cfg));
            return httplib::Server::HandlerResponse::Handled;
        });
}

// ---------------------------- server construction ----------------------------
// Builds the HTTP(S) listener; @p problem receives the reason of a null return. An SSLServer is
// a httplib::Server, so route registration and listen() below stay transport-agnostic.
std::unique_ptr<httplib::Server> MakeHttpServer(const ServerConfig& cfg, std::string& problem)
{
    if (!TlsConfigured(cfg))
    {
        return std::make_unique<httplib::Server>();
    }
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
    for (const std::string& file : {cfg.tls_cert, cfg.tls_key})
    {
        if (!std::filesystem::exists(std::filesystem::u8path(file)))
        {
            problem = "tls: certificate/key file not found: " + file;
            return nullptr;
        }
    }
    auto ssl = std::make_unique<httplib::SSLServer>(cfg.tls_cert.c_str(), cfg.tls_key.c_str());
    if (!ssl->is_valid())
    {
        problem = "tls: could not initialize an SSL context from " + cfg.tls_cert + " / " + cfg.tls_key;
        return nullptr;
    }
    return ssl;
#else
    problem = "tls: this build has no HTTPS support; rebuild cpp-httplib with its openssl port feature "
              "(enabled by the rest-api manifest feature)";
    return nullptr;
#endif
}

}  // namespace

// Declared in routes.hpp: the same entry point RunServer uses, so the HTTP-level tests can
// install the real route table on their own listener.
void RegisterRoutes(httplib::Server& svr, Ctx& ctx)
{
    // With an upload pool configured, [upload] max_request_mb must cover the model file
    // plus multipart boundaries; otherwise the historical 64 MB cap for embedded configs
    // stays in effect.
    const std::size_t max_body_bytes = ctx.cfg.upload_enabled
                                           ? static_cast<std::size_t>(ctx.cfg.upload_max_request_mb) * 1024u * 1024u
                                           : 64u * 1024u * 1024u;
    svr.set_payload_max_length(max_body_bytes);
    RegisterAuthGate(svr, ctx);

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
    // httplib matches regex routes in registration order, so every more specific
    // /api/jobs/... pattern below must stay ahead of the catch-all job routes.
    svr.Post(R"(/api/jobs/([^/]+)/cancel)",
             [&ctx](const Request& req, Response& res) { HandleCancelJob(req, res, ctx, req.matches[1].str()); });
    // "path" is a reserved segment in front of the layer route: only a numeric segment
    // may reach the single-layer handler below, so this literal route has to be first.
    svr.Get(R"(/api/jobs/([^/]+)/geometry/path)",
            [&ctx](const Request& req, Response& res) { HandleJobGeometryPath(req, res, ctx, req.matches[1].str()); });
    svr.Get(R"(/api/jobs/([^/]+)/geometry/([^/]+))", [&ctx](const Request& req, Response& res)
            { HandleJobLayerGeometry(req, res, ctx, req.matches[1].str(), req.matches[2].str()); });
    svr.Get(R"(/api/jobs/([^/]+)/geometry)",
            [&ctx](const Request& req, Response& res) { HandleJobGeometry(req, res, ctx, req.matches[1].str()); });
    // A package entry name carries slashes ("layers/layer_0.png"), so the tail can only be captured
    // by a greedy (.+) behind an explicit /package segment; nothing above it may swallow that prefix.
    svr.Get(R"(/api/jobs/([^/]+)/package)",
            [&ctx](const Request& req, Response& res) { HandleJobPackage(req, res, ctx, req.matches[1].str()); });
    svr.Get(R"(/api/jobs/([^/]+)/package/(.+))", [&ctx](const Request& req, Response& res)
            { HandleJobPackageEntry(req, res, ctx, req.matches[1].str(), req.matches[2].str()); });
    svr.Post(R"(/api/jobs/([^/]+))",
             [&ctx](const Request& req, Response& res) { HandleSubmitJob(req, res, ctx, req.matches[1].str()); });
    svr.Get("/api/jobs", [&ctx](const Request& req, Response& res) { HandleListJobs(req, res, ctx); });
    svr.Get(R"(/api/jobs/([^/]+)/result)",
            [&ctx](const Request& req, Response& res) { HandleJobResult(req, res, ctx, req.matches[1].str()); });
    svr.Get(R"(/api/jobs/([^/]+))",
            [&ctx](const Request& req, Response& res) { HandleJobStatus(req, res, ctx, req.matches[1].str()); });
    svr.Delete(R"(/api/jobs/([^/]+))",
               [&ctx](const Request& req, Response& res) { HandleDeleteJob(req, res, ctx, req.matches[1].str()); });
    // Upload-pool inspection and manual cleanup (the pool also reclaims on its own timer).
    svr.Get("/api/uploads", [&ctx](const Request& req, Response& res) { HandleListUploads(req, res, ctx); });
    svr.Get(R"(/api/uploads/([^/]+))",
            [&ctx](const Request& req, Response& res) { HandleUploadStatus(req, res, ctx, req.matches[1].str()); });
    svr.Delete(R"(/api/uploads/([^/]+))",
               [&ctx](const Request& req, Response& res) { HandleDeleteUpload(req, res, ctx, req.matches[1].str()); });
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

namespace
{
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
    if (const std::string problem = ValidateSecurityConfig(cfg); !problem.empty())
    {
        std::fprintf(stderr, "HsBaSlicerRestServer: configuration error: %s\n", problem.c_str());
        return 2;
    }

    initialize();

    std::string tls_problem;
    std::unique_ptr<httplib::Server> server = MakeHttpServer(cfg, tls_problem);
    if (!server)
    {
        std::fprintf(stderr, "HsBaSlicerRestServer: configuration error: %s\n", tls_problem.c_str());
        return 2;
    }
    httplib::Server& svr = *server;

    // Declared before JobManager so it outlives the worker threads: a finishing job calls
    // back into the pool to start the upload retention clock.
    UploadOptions upload_options;
    upload_options.dir = cfg.upload_dir;
    upload_options.max_files = cfg.upload_max_files;
    upload_options.ttl_seconds = cfg.upload_ttl_seconds;
    upload_options.evict_mode = cfg.upload_evict_mode;
    upload_options.max_total_mb = cfg.upload_max_total_mb;
    upload_options.sweep_interval_seconds = cfg.upload_sweep_interval_seconds;
    UploadStore uploads(upload_options);

    JobManager jobs(cfg.max_concurrent_jobs, cfg.max_jobs, std::chrono::seconds(cfg.result_ttl_seconds), {},
                    [&uploads](const std::string& job_id) { uploads.MarkTerminal(job_id); });
    Ctx ctx{cfg, jobs, uploads, std::chrono::steady_clock::now()};
    RegisterRoutes(svr, ctx);

    g_server.store(&svr);
    std::signal(SIGINT, SignalHandler);
#ifdef SIGTERM
    std::signal(SIGTERM, SignalHandler);
#endif

    std::fprintf(stderr, "HsBaSlicerRestServer listening on %s://%s:%d (default format: %s)\n",
                 TlsConfigured(cfg) ? "https" : "http", cfg.host.c_str(), cfg.port, cfg.default_format.c_str());
    if (cfg.auth_enabled && !cfg.auth_token.empty())
    {
        if (IsLoopbackHost(cfg.host))
        {
            std::fprintf(stderr,
                         "auth: token configured but NOT enforced on loopback bind '%s'; "
                         "local clients stay token-free by design\n",
                         cfg.host.c_str());
        }
        else
        {
            std::fprintf(stderr, "auth: credentials required on all non-public endpoints "
                                 "(Authorization: Bearer <token> or X-API-Key: <token>)\n");
        }
    }
    else if (!IsLoopbackHost(cfg.host))
    {
        std::fprintf(stderr,
                     "warning: '%s' is reachable from other hosts with NO authorization%s; set [auth] "
                     "token and [server] tls_cert/tls_key\n",
                     cfg.host.c_str(), TlsConfigured(cfg) ? "" : " over plaintext HTTP");
    }

    if (!svr.listen(cfg.host.c_str(), cfg.port))
    {
        g_server.store(nullptr);
        return 1;
    }
    g_server.store(nullptr);
    return 0;
}

}  // namespace HsBa::Slicer::RestApi
