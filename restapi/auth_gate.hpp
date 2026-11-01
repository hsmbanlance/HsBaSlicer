/** @file auth_gate.hpp
 * @brief Bearer-token authorization and TLS/loopback policy decisions for HsBaSlicerRestServer.
 *
 * These are pure string decisions with no HTTP dependency, so routes.cpp stays the only
 * translation unit that needs cpp-httplib and this gate can be unit-tested directly (see
 * tests/RestApi/auth_gate_test.cpp).
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_RESTAPI_AUTH_GATE_HPP
#define HSBA_SLICER_RESTAPI_AUTH_GATE_HPP

#include <string>
#include <string_view>

#include "server_config.hpp"

namespace HsBa::Slicer::RestApi
{
/** @brief Result of evaluating one request's credentials. */
enum class AuthOutcome
{
    Allowed,             ///< Credentials match the configured token, or authorization is not required.
    MissingCredentials,  ///< Authorization is required but the request carried no credentials.
    InvalidCredentials,  ///< Credentials were present but did not match the configured token.
};

/**
 * @brief Whether @p boundHost refers to the loopback interface.
 *
 * Covers 127.0.0.0/8, ::1, ip6-localhost, bracketed IPv6 literals and the v4-mapped
 * ::ffff:127.x.x.x form. "localhost" is matched by name only: it is the operator's
 * explicit choice, and resolving it is not what a bind string promises. An empty host
 * is treated as non-loopback so the gate fails closed.
 */
bool IsLoopbackHost(std::string_view boundHost);

/** @brief Endpoints that stay reachable without credentials (liveness probe and root index). */
bool IsPublicPath(std::string_view path);

/**
 * @brief Whether requests on @p boundHost must carry credentials.
 *
 * Authorization is deliberately skipped on loopback bindings (trusted local model, keeps
 * curl/localhost workflows and container HEALTHCHECKs token-free).
 */
bool AuthRequired(const ServerConfig& cfg, std::string_view boundHost);

/**
 * @brief Match one request against the configured token.
 *
 * Accepts `Authorization: Bearer <token>` (scheme case-insensitive) and `X-API-Key: <token>`.
 * Comparison is constant-time over the token length. Returns Allowed when authorization is
 * not required, or when @p path is public.
 */
AuthOutcome Authorize(const ServerConfig& cfg, std::string_view path, const std::string& boundHost,
                      const std::string& authorizationHeader, const std::string& apiKeyHeader);

/** @brief Whether both a certificate and a private key were configured. */
bool TlsConfigured(const ServerConfig& cfg);

/**
 * @brief Startup consistency check of the [auth]/[server] TLS settings.
 * @return Empty string when the configuration is usable, otherwise a human-readable error that
 *         must abort startup (enabled auth without a token, half-specified cert/key, require_tls
 *         without a usable pair).
 */
std::string ValidateSecurityConfig(const ServerConfig& cfg);

}  // namespace HsBa::Slicer::RestApi

#endif  // HSBA_SLICER_RESTAPI_AUTH_GATE_HPP
