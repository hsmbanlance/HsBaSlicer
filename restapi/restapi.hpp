/** @file restapi.hpp
 * @brief Public entry points of the REST API server translation layer.
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_RESTAPI_HPP
#define HSBA_SLICER_RESTAPI_HPP

#include "server_config.hpp"

namespace HsBa::Slicer::RestApi
{
/**
 * @brief Build the HTTP server, register all routes and block on listen().
 *
 * Initializes the DllHsBaSlicer runtime, starts the job worker pool sized from
 * @p cfg, and serves requests until the server is stopped (e.g. by a SIGINT
 * handler installed by the caller). Returns the process-style exit code.
 *
 * @param cfg Server configuration (from restapi.ini).
 * @return 0 on clean shutdown, non-zero on startup failure.
 */
int RunServer(const ServerConfig& cfg);

}  // namespace HsBa::Slicer::RestApi

#endif  // HSBA_SLICER_RESTAPI_HPP
