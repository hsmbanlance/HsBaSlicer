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

namespace HsBa::Slicer::RestApi
{
namespace
{
using HsBa::Slicer::Utils::to_lower;

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

    return cfg;
}

}  // namespace HsBa::Slicer::RestApi
