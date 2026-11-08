/** @file auth_gate_test.cpp
 * @brief Unit tests for the REST server authorization gate, TLS policy and their ini settings.
 * @author HsBa
 */
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#define BOOST_TEST_MODULE RestApiAuthGateTest
#include <boost/test/included/unit_test.hpp>

#include "../../restapi/auth_gate.hpp"
#include "../../restapi/server_config.hpp"

using namespace HsBa::Slicer::RestApi;

namespace
{
/** @brief A configuration that requires a token on non-loopback bindings. */
ServerConfig GuardedConfig()
{
    ServerConfig cfg;
    cfg.auth_enabled = true;
    cfg.auth_token = "s3cret-token";
    return cfg;
}

/** @brief Write @p content to a throwaway ini file and return its path (the caller removes it). */
std::string WriteTempIni(const std::string& tag, const std::string& content)
{
    static int counter = 0;
    const std::string name = "hsba_restapi_" + tag + "_" + std::to_string(counter++) + ".ini";
    const auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << content;
    out.close();
    return path.string();
}

/** @brief Remove a throwaway ini file created by WriteTempIni. */
void RemoveTempIni(const std::string& path)
{
    std::filesystem::remove(std::filesystem::u8path(path));
}
}  // namespace

BOOST_AUTO_TEST_SUITE(restapi_auth_gate)

BOOST_AUTO_TEST_CASE(loopback_addresses_are_recognized)
{
    BOOST_CHECK(IsLoopbackHost("127.0.0.1"));
    BOOST_CHECK(IsLoopbackHost("127.5.6.7"));  // whole 127.0.0.0/8
    BOOST_CHECK(IsLoopbackHost("localhost"));
    BOOST_CHECK(IsLoopbackHost("LOCALHOST"));
    BOOST_CHECK(IsLoopbackHost("::1"));
    BOOST_CHECK(IsLoopbackHost("[::1]"));
    BOOST_CHECK(IsLoopbackHost(" 127.0.0.1 "));
    BOOST_CHECK(IsLoopbackHost("::ffff:127.0.0.1"));
}

BOOST_AUTO_TEST_CASE(external_addresses_are_not_loopback)
{
    BOOST_CHECK(!IsLoopbackHost("0.0.0.0"));
    BOOST_CHECK(!IsLoopbackHost("::"));
    BOOST_CHECK(!IsLoopbackHost("192.168.1.20"));
    BOOST_CHECK(!IsLoopbackHost("127"));  // truncated literal, not a dotted quad
    BOOST_CHECK(!IsLoopbackHost("127.0.0.1.evil.com"));
    BOOST_CHECK(!IsLoopbackHost(""));  // fails closed
    BOOST_CHECK(!IsLoopbackHost("localhost.localdomain"));
}

BOOST_AUTO_TEST_CASE(only_health_and_root_are_public)
{
    BOOST_CHECK(IsPublicPath("/"));
    BOOST_CHECK(IsPublicPath("/api/health"));
    BOOST_CHECK(!IsPublicPath("/api/version"));
    BOOST_CHECK(!IsPublicPath("/api/health/"));
    BOOST_CHECK(!IsPublicPath("/api/jobs"));
}

BOOST_AUTO_TEST_CASE(authorization_is_skipped_on_loopback_only)
{
    const ServerConfig cfg = GuardedConfig();
    BOOST_CHECK(!AuthRequired(cfg, "127.0.0.1"));
    BOOST_CHECK(!AuthRequired(cfg, "::1"));
    BOOST_CHECK(AuthRequired(cfg, "0.0.0.0"));
    BOOST_CHECK(AuthRequired(cfg, "192.168.1.20"));
    // A disabled gate never asks for credentials, not even on an exposed bind.
    ServerConfig off = cfg;
    off.auth_enabled = false;
    BOOST_CHECK(!AuthRequired(off, "0.0.0.0"));
    // An enabled gate without a token cannot protect anything: AuthRequired stays false and
    // ValidateSecurityConfig reports it (see security_startup_checks).
    ServerConfig noToken = cfg;
    noToken.auth_token.clear();
    BOOST_CHECK(!AuthRequired(noToken, "0.0.0.0"));
}

BOOST_AUTO_TEST_CASE(token_matching_rules)
{
    const ServerConfig cfg = GuardedConfig();
    const std::string lan = "192.168.1.20";

    BOOST_CHECK(Authorize(cfg, "/api/version", lan, "", "") == AuthOutcome::MissingCredentials);
    BOOST_CHECK(Authorize(cfg, "/api/version", lan, "Bearer s3cret-token", "") == AuthOutcome::Allowed);
    BOOST_CHECK(Authorize(cfg, "/api/version", lan, "bearer s3cret-token", "") == AuthOutcome::Allowed);  // scheme case
    BOOST_CHECK(Authorize(cfg, "/api/version", lan, "Bearer  s3cret-token ", "") == AuthOutcome::Allowed);
    BOOST_CHECK(Authorize(cfg, "/api/version", lan, "s3cret-token", "") == AuthOutcome::Allowed);  // scheme-less
    BOOST_CHECK(Authorize(cfg, "/api/version", lan, "", "s3cret-token") == AuthOutcome::Allowed);  // X-API-Key
    BOOST_CHECK(Authorize(cfg, "/api/version", lan, "Bearer wrong", "") == AuthOutcome::InvalidCredentials);
    BOOST_CHECK(Authorize(cfg, "/api/version", lan, "", "wrong") == AuthOutcome::InvalidCredentials);
    BOOST_CHECK(Authorize(cfg, "/api/version", lan, "", "wrong-token-length") == AuthOutcome::InvalidCredentials);
    // Authorization takes precedence; an unusable Authorization is not patched up by X-API-Key.
    BOOST_CHECK(Authorize(cfg, "/api/version", lan, "Bearer wrong", "s3cret-token") == AuthOutcome::InvalidCredentials);
    BOOST_CHECK(Authorize(cfg, "/api/version", lan, "Bearer", "s3cret-token") == AuthOutcome::MissingCredentials);

    // Exempt paths and loopback binds answer without credentials.
    BOOST_CHECK(Authorize(cfg, "/api/health", lan, "", "") == AuthOutcome::Allowed);
    BOOST_CHECK(Authorize(cfg, "/", lan, "", "") == AuthOutcome::Allowed);
    BOOST_CHECK(Authorize(cfg, "/api/version", "127.0.0.1", "", "") == AuthOutcome::Allowed);
}

BOOST_AUTO_TEST_CASE(tls_configuration_pairing)
{
    ServerConfig cfg;
    BOOST_CHECK(!TlsConfigured(cfg));
    cfg.tls_cert = "server.crt";
    BOOST_CHECK(!TlsConfigured(cfg));
    cfg.tls_key = "server.key";
    BOOST_CHECK(TlsConfigured(cfg));
}

BOOST_AUTO_TEST_CASE(security_startup_checks)
{
    ServerConfig cfg;
    BOOST_CHECK(ValidateSecurityConfig(cfg).empty());  // plain defaults are usable

    cfg.auth_enabled = true;  // gate armed but no secret to check against
    BOOST_CHECK(!ValidateSecurityConfig(cfg).empty());
    cfg.auth_token = "s3cret-token";
    BOOST_CHECK(ValidateSecurityConfig(cfg).empty());

    cfg = ServerConfig{};
    cfg.tls_cert = "server.crt";  // key missing
    BOOST_CHECK(!ValidateSecurityConfig(cfg).empty());
    cfg.tls_key = "server.key";
    BOOST_CHECK(ValidateSecurityConfig(cfg).empty());

    cfg = ServerConfig{};
    cfg.require_tls = true;  // strict mode without a pair
    BOOST_CHECK(!ValidateSecurityConfig(cfg).empty());
    cfg.tls_cert = "server.crt";
    cfg.tls_key = "server.key";
    BOOST_CHECK(ValidateSecurityConfig(cfg).empty());
}

BOOST_AUTO_TEST_CASE(ini_security_keys_are_parsed)
{
    const std::string path = WriteTempIni("security", "[server]\n"
                                                      "host = 0.0.0.0\n"
                                                      "tls_cert = server.crt\n"
                                                      "tls_key = server.key\n"
                                                      "require_tls = on\n"
                                                      "[auth]\n"
                                                      "enabled =\n"
                                                      "token = s3cret-token\n");
    const ServerConfig cfg = LoadServerConfig(path);
    RemoveTempIni(path);

    BOOST_CHECK(cfg.host == "0.0.0.0");
    BOOST_CHECK(cfg.tls_cert == "server.crt");
    BOOST_CHECK(cfg.tls_key == "server.key");
    BOOST_CHECK(cfg.require_tls);
    BOOST_CHECK(cfg.auth_enabled);  // a token without an explicit switch still arms the gate
    BOOST_CHECK(cfg.auth_token == "s3cret-token");
    BOOST_CHECK(AuthRequired(cfg, cfg.host));
}

BOOST_AUTO_TEST_CASE(ini_can_force_authorization_off)
{
    const std::string path =
        WriteTempIni("disabled", "[server]\nhost = 0.0.0.0\n[auth]\nenabled = false\ntoken = s3cret-token\n");
    const ServerConfig cfg = LoadServerConfig(path);
    RemoveTempIni(path);

    BOOST_CHECK(!cfg.auth_enabled);  // an explicit switch always wins
    BOOST_CHECK(cfg.auth_token == "s3cret-token");
    BOOST_CHECK(!AuthRequired(cfg, cfg.host));
}

BOOST_AUTO_TEST_CASE(missing_ini_keeps_insecure_but_usable_defaults)
{
    const ServerConfig cfg = LoadServerConfig("hsba_restapi_does_not_exist.ini");
    BOOST_CHECK(!cfg.auth_enabled);
    BOOST_CHECK(cfg.auth_token.empty());
    BOOST_CHECK(cfg.host == "127.0.0.1");
    BOOST_CHECK(!cfg.require_tls);
    BOOST_CHECK(!TlsConfigured(cfg));
    BOOST_CHECK(ValidateSecurityConfig(cfg).empty());
}

BOOST_AUTO_TEST_CASE(shipped_ini_stays_parseable_and_off_by_default)
{
    // Guards the template that ships next to the executable: boost ptree quirks with empty
    // values would otherwise break every fresh checkout.
    std::error_code ec;
    const auto ini = std::filesystem::path(__FILE__).parent_path() / ".." / ".." / "restapi" / "restapi.ini";
    if (!std::filesystem::exists(ini, ec))
    {
        BOOST_TEST_MESSAGE("skip: restapi.ini not found at " << ini.string());
        return;
    }
    ServerConfig cfg;
    BOOST_CHECK_NO_THROW(cfg = LoadServerConfig(ini.string()));
    BOOST_CHECK(cfg.host == "127.0.0.1");
    BOOST_CHECK(!cfg.auth_enabled);
    BOOST_CHECK(ValidateSecurityConfig(cfg).empty());
}

BOOST_AUTO_TEST_SUITE_END()
