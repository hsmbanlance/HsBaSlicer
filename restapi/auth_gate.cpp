/** @file auth_gate.cpp
 * @brief Implements the token-authorization and TLS policy decisions; see auth_gate.hpp.
 * @author HsBa
 */
#include "auth_gate.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

#include "base/string_helper.hpp"

namespace HsBa::Slicer::RestApi
{
namespace
{
using HsBa::Slicer::Utils::to_lower;
using HsBa::Slicer::Utils::trim;

constexpr std::string_view kBearerScheme = "bearer";
constexpr std::string_view kV4MappedPrefix = "::ffff:";

/** @brief Compare two secrets without branching per character, so timing follows the length only. */
bool SecureEquals(std::string_view a, std::string_view b)
{
    if (a.size() != b.size())
    {
        return false;
    }
    volatile unsigned char diff = 0;
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        diff |= static_cast<unsigned char>(a[i]) ^ static_cast<unsigned char>(b[i]);
    }
    return diff == 0;
}

/** @brief Strip surrounding HTTP whitespace (space/tab) from a header value. */
std::string_view TrimHttpWhitespace(std::string_view s)
{
    const auto not_ws = [](char c) { return c != ' ' && c != '\t'; };
    while (!s.empty() && !not_ws(s.front()))
    {
        s.remove_prefix(1);
    }
    while (!s.empty() && !not_ws(s.back()))
    {
        s.remove_suffix(1);
    }
    return s;
}

/** @brief Whether @p host is a bare 127.0.0.0/8 dotted-quad literal. */
bool IsIpv4Loopback(std::string_view host)
{
    if (!host.starts_with("127.") || host.size() == 4)
    {
        return false;
    }
    return std::all_of(host.begin() + 4, host.end(),
                       [](char c) { return c == '.' || std::isdigit(static_cast<unsigned char>(c)) != 0; });
}

/**
 * @brief The credential a client presented, or an empty string when it presented none.
 *
 * `Authorization` wins over `X-API-Key`; the "bearer" scheme prefix is recognized
 * case-insensitively (and only when followed by whitespace or the end of the value, so
 * "Bearer" alone reads as "no credential"), and a scheme-less Authorization value is compared
 * verbatim so plain token headers from simple clients keep working.
 */
std::string_view PresentedCredential(const std::string& authorizationHeader, const std::string& apiKeyHeader)
{
    const std::string_view auth = TrimHttpWhitespace(authorizationHeader);
    if (!auth.empty())
    {
        const bool schemeOnly = (auth.size() == kBearerScheme.size());
        const bool hasToken = (auth.size() > kBearerScheme.size());
        const char after = hasToken ? auth[kBearerScheme.size()] : '\0';
        if ((schemeOnly || (hasToken && (after == ' ' || after == '\t'))) &&
            to_lower(auth.substr(0, kBearerScheme.size())) == kBearerScheme)
        {
            return TrimHttpWhitespace(auth.substr(kBearerScheme.size()));
        }
        return auth;
    }
    return TrimHttpWhitespace(apiKeyHeader);
}
}  // namespace

bool IsLoopbackHost(std::string_view boundHost)
{
    std::string host = to_lower(trim(boundHost));
    if (host.size() >= 2 && host.front() == '[' && host.back() == ']')
    {
        host = host.substr(1, host.size() - 2);
    }
    if (host == "localhost" || host == "::1" || host == "ip6-localhost" || host == "ip6-loopback")
    {
        return true;
    }
    if (host.starts_with(kV4MappedPrefix))
    {
        return IsLoopbackHost(std::string_view(host).substr(kV4MappedPrefix.size()));
    }
    return IsIpv4Loopback(host);
}

bool IsPublicPath(std::string_view path)
{
    return path == "/" || path == "/api/health";
}

bool AuthRequired(const ServerConfig& cfg, std::string_view boundHost)
{
    return cfg.auth_enabled && !cfg.auth_token.empty() && !IsLoopbackHost(boundHost);
}

AuthOutcome Authorize(const ServerConfig& cfg, std::string_view path, const std::string& boundHost,
                      const std::string& authorizationHeader, const std::string& apiKeyHeader)
{
    if (IsPublicPath(path) || !AuthRequired(cfg, boundHost))
    {
        return AuthOutcome::Allowed;
    }
    const std::string_view presented = PresentedCredential(authorizationHeader, apiKeyHeader);
    if (presented.empty())
    {
        return AuthOutcome::MissingCredentials;
    }
    return SecureEquals(presented, cfg.auth_token) ? AuthOutcome::Allowed : AuthOutcome::InvalidCredentials;
}

bool TlsConfigured(const ServerConfig& cfg)
{
    return !cfg.tls_cert.empty() && !cfg.tls_key.empty();
}

std::string ValidateSecurityConfig(const ServerConfig& cfg)
{
    const bool hasCert = !cfg.tls_cert.empty();
    const bool hasKey = !cfg.tls_key.empty();
    if (hasCert != hasKey)
    {
        return "tls: [server] tls_cert and tls_key must be configured together";
    }
    if (cfg.require_tls && !hasCert)
    {
        return "tls: [server] require_tls=true needs tls_cert and tls_key";
    }
    if (cfg.auth_enabled && cfg.auth_token.empty())
    {
        return "auth: [auth] enabled=true needs a non-empty token";
    }
    return {};
}

}  // namespace HsBa::Slicer::RestApi
