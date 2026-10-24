/** @file server_config.hpp
 * @brief Server/ParamStore settings loaded from restapi.ini for HsBaSlicerRestServer.
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_RESTAPI_SERVER_CONFIG_HPP
#define HSBA_SLICER_RESTAPI_SERVER_CONFIG_HPP

#include <string>

#include "pipelinetypes/pipeline_types.h"

namespace HsBa::Slicer::RestApi
{
/**
 * @brief Runtime configuration for the REST server, populated from restapi.ini.
 *
 * All members carry their built-in defaults, so a partially filled (or entirely
 * missing) ini file still yields a usable configuration.
 */
struct ServerConfig
{
    /* [server] */
    std::string host = "127.0.0.1";       ///< Bind address.
    int port = 8080;                      ///< TCP port.
    std::string default_format = "json";  ///< Fallback response format ("json" or "xml").
    int max_concurrent_jobs = 2;          ///< Worker threads executing slicing jobs concurrently.
    int max_jobs = 128;                   ///< Upper bound on tracked jobs (429 when exceeded).
    int result_ttl_seconds = 3600;        ///< Retention time of a finished job result, seconds.

    /* [paramstore] */
    bool paramstore_enabled = false;                                       ///< True when a backend was configured.
    HsBaParamStoreBackend paramstore_backend = HSBA_PARAM_BACKEND_SQLITE;  ///< ParamStore backend.
    std::string paramstore_sqlite_path = "hsba_params.db";                 ///< SQLite file path.
    std::string paramstore_host;                                           ///< MySQL/PG host.
    std::string paramstore_user;                                           ///< MySQL/PG user.
    std::string paramstore_password;                                       ///< MySQL/PG password.
    std::string paramstore_database;                                       ///< MySQL/PG database.
    int paramstore_port = 0;                                               ///< MySQL/PG port (0 = adapter default).
    std::string paramstore_crypto_password;                                ///< Optional at-rest crypto password.
};

/**
 * @brief Load a ServerConfig from an ini file.
 *
 * When @p path does not exist, the built-in defaults are returned unchanged.
 * @param path Path to restapi.ini.
 * @return Populated configuration.
 * @throws boost::property_tree::file_parser_error when the file exists but is malformed.
 */
ServerConfig LoadServerConfig(const std::string& path);

}  // namespace HsBa::Slicer::RestApi

#endif  // HSBA_SLICER_RESTAPI_SERVER_CONFIG_HPP
