/** @file routes.hpp
 * @brief Route-registration seam shared by RunServer and the HTTP-level endpoint tests.
 *
 * The handlers themselves stay private to routes.cpp; only the context they borrow and the
 * single entry point that installs them are exposed here, so tests can drive the real route
 * table over a real socket without starting HsBaSlicerRestServer (see
 * tests/RestApi/endpoint_http_test.cpp). The cpp-httplib type is forward-declared to keep
 * this header free of the third-party include.
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_RESTAPI_ROUTES_HPP
#define HSBA_SLICER_RESTAPI_ROUTES_HPP

#include <chrono>

namespace httplib
{
class Server;
}

namespace HsBa::Slicer::RestApi
{
struct ServerConfig;
class JobManager;
class UploadStore;

/**
 * @brief Shared handler context.
 *
 * Every registered lambda borrows these objects, so a Ctx (and what it references) must
 * outlive the server: RunServer keeps it on the stack for the whole listen() loop.
 */
struct Ctx
{
    const ServerConfig& cfg;
    JobManager& jobs;
    UploadStore& uploads;
    std::chrono::steady_clock::time_point start;
};

/**
 * @brief Install the authorization pre-routing gate, the payload cap and every /api endpoint on
 *        @p svr. Transport-agnostic: an HTTPS listener is the same httplib::Server subclass.
 */
void RegisterRoutes(httplib::Server& svr, Ctx& ctx);

}  // namespace HsBa::Slicer::RestApi

#endif  // HSBA_SLICER_RESTAPI_ROUTES_HPP
