/** @file server_config.cpp
 * @brief Implements LoadServerConfig; see server_config.hpp.
 * @author HsBa
 */
#include "server_config.hpp"

#include <algorithm>
#include <filesystem>
#include <string>

#include "base/string_helper.hpp"
#include "fileoperator/rw_ptree.hpp"

// Capacity of the in-process ModelLoader pool (top-level CMakeLists.txt sets it
// to 10 with CGAL/OCCT and 1 otherwise). The upload pool must never retain fewer
// model files than this, otherwise a pool-sized workload cannot be re-run from
// the uploaded copies alone.
#ifndef HSBA_MODEL_POOL_SIZE
#define HSBA_MODEL_POOL_SIZE 1
#endif

namespace HsBa::Slicer::RestApi
{
namespace
{
using HsBa::Slicer::Utils::to_lower;
using HsBa::Slicer::Utils::trim;

HsBaParamStoreBackend ParseBackend(const std::string& lower)
{
    if (lower == "mysql")
    {
        return HSBA_PARAM_BACKEND_MYSQL;
    }
    if (lower == "postgresql" || lower == "postgres")
    {
        return HSBA_PARAM_BACKEND_POSTGRESQL;
    }
    return HSBA_PARAM_BACKEND_SQLITE;
}

/** @brief Read a boolean from a raw ini string; @p def applies to empty/unrecognized values. */
bool ParseBool(const std::string& lower, bool def)
{
    if (lower == "1" || lower == "true" || lower == "yes" || lower == "on")
    {
        return true;
    }
    if (lower == "0" || lower == "false" || lower == "no" || lower == "off")
    {
        return false;
    }
    return def;
}
}  // namespace

ServerConfig LoadServerConfig(const std::string& path)
{
    ServerConfig cfg;

    // Missing file is not an error: the built-in defaults are used instead, so a
    // freshly built server can start without an explicit restapi.ini.
    if (!std::filesystem::exists(std::filesystem::u8path(path)))
    {
        return cfg;
    }

    const boost::property_tree::ptree pt = Config::from_ini(path);

    cfg.host = pt.get<std::string>("server.host", cfg.host);
    cfg.port = pt.get<int>("server.port", cfg.port);
    cfg.default_format = to_lower(pt.get<std::string>("server.default_format", cfg.default_format));
    cfg.max_concurrent_jobs = pt.get<int>("server.max_concurrent_jobs", cfg.max_concurrent_jobs);
    cfg.max_jobs = pt.get<int>("server.max_jobs", cfg.max_jobs);
    cfg.result_ttl_seconds = pt.get<int>("server.result_ttl_seconds", cfg.result_ttl_seconds);
    cfg.tls_cert = trim(pt.get<std::string>("server.tls_cert", cfg.tls_cert));
    cfg.tls_key = trim(pt.get<std::string>("server.tls_key", cfg.tls_key));
    cfg.require_tls = ParseBool(to_lower(trim(pt.get<std::string>("server.require_tls", ""))), cfg.require_tls);

    // A token without an explicit switch still enables authorization (silently ignoring an
    // operator-supplied token would leave the service open); an explicit value always wins.
    cfg.auth_token = trim(pt.get<std::string>("auth.token", cfg.auth_token));
    const std::string auth_switch = to_lower(trim(pt.get<std::string>("auth.enabled", "")));
    cfg.auth_enabled = auth_switch.empty() ? !cfg.auth_token.empty() : ParseBool(auth_switch, false);

    const std::string backend = to_lower(pt.get<std::string>("paramstore.backend", ""));
    cfg.paramstore_enabled = !backend.empty() && backend != "none";
    cfg.paramstore_backend = ParseBackend(backend);
    cfg.paramstore_sqlite_path = pt.get<std::string>("paramstore.sqlite_path", cfg.paramstore_sqlite_path);
    cfg.paramstore_host = pt.get<std::string>("paramstore.host", cfg.paramstore_host);
    cfg.paramstore_user = pt.get<std::string>("paramstore.user", cfg.paramstore_user);
    cfg.paramstore_password = pt.get<std::string>("paramstore.password", cfg.paramstore_password);
    cfg.paramstore_database = pt.get<std::string>("paramstore.database", cfg.paramstore_database);
    cfg.paramstore_port = pt.get<int>("paramstore.port", cfg.paramstore_port);
    cfg.paramstore_crypto_password = pt.get<std::string>("paramstore.crypto_password", cfg.paramstore_crypto_password);

    // [upload] Model-file upload pool. An empty dir disables multipart uploads while
    // keeping every other endpoint available.
    cfg.upload_dir = trim(pt.get<std::string>("upload.dir", cfg.upload_dir));
    cfg.upload_enabled = !cfg.upload_dir.empty();
    cfg.upload_max_files = pt.get<int>("upload.max_files", cfg.upload_max_files);
    cfg.upload_ttl_seconds = pt.get<long long>("upload.ttl_seconds", cfg.upload_ttl_seconds);
    cfg.upload_evict_mode = to_lower(trim(pt.get<std::string>("upload.evict_mode", cfg.upload_evict_mode)));
    cfg.upload_max_file_mb = pt.get<int>("upload.max_file_mb", cfg.upload_max_file_mb);
    cfg.upload_max_request_mb = pt.get<int>("upload.max_request_mb", cfg.upload_max_request_mb);
    cfg.upload_max_total_mb = pt.get<long long>("upload.max_total_mb", cfg.upload_max_total_mb);
    cfg.upload_sweep_interval_seconds =
        pt.get<int>("upload.sweep_interval_seconds", cfg.upload_sweep_interval_seconds);

    // Defensive normalization of obviously invalid numeric settings.
    if (cfg.port <= 0 || cfg.port > 65535)
    {
        cfg.port = 8080;
    }
    if (cfg.default_format != "xml")
    {
        cfg.default_format = "json";
    }
    cfg.max_concurrent_jobs = std::max(1, cfg.max_concurrent_jobs);
    cfg.max_jobs = std::max(1, cfg.max_jobs);
    cfg.result_ttl_seconds = std::max(0, cfg.result_ttl_seconds);

    // Upload-pool normalization. max_files is floored at the model-pool capacity so the
    // pool can always be refilled from retained uploads; evict_mode falls back to the
    // conservative "both" for unrecognized values.
    if (cfg.upload_evict_mode != "ttl" && cfg.upload_evict_mode != "count" && cfg.upload_evict_mode != "both" &&
        cfg.upload_evict_mode != "manual")
    {
        cfg.upload_evict_mode = "both";
    }
    cfg.upload_max_files = std::max({cfg.upload_max_files, static_cast<int>(HSBA_MODEL_POOL_SIZE), 1});
    cfg.upload_ttl_seconds = std::clamp<long long>(cfg.upload_ttl_seconds, 0, 2592000);  // 0 .. 30 days
    cfg.upload_max_file_mb = std::clamp(cfg.upload_max_file_mb, 1, 4096);
    cfg.upload_max_request_mb = std::max(cfg.upload_max_request_mb, cfg.upload_max_file_mb + 4);
    cfg.upload_max_total_mb = std::max<long long>(0, cfg.upload_max_total_mb);
    cfg.upload_sweep_interval_seconds = std::clamp(cfg.upload_sweep_interval_seconds, 5, 3600);

    return cfg;
}

}  // namespace HsBa::Slicer::RestApi
