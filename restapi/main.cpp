/** @file main.cpp
 * @brief Entry point for HsBaSlicerRestServer: parse args, load ini, start the server.
 * @author HsBa
 */
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

#include "restapi.hpp"
#include "server_config.hpp"

namespace
{
void PrintUsage(const char* argv0)
{
    std::fprintf(stderr,
                 "Usage: %s [options]\n"
                 "  --config <file>   Path to restapi.ini (default: ./restapi.ini)\n"
                 "  --host <addr>     Override the bind address from the ini\n"
                 "  --port <n>        Override the listen port from the ini\n"
                 "  --auth-token <t>  Shared token required on non-loopback binds\n"
                 "  --tls-cert <f>    PEM certificate enabling HTTPS\n"
                 "  --tls-key <f>     PEM private key matching --tls-cert\n"
                 "  --require-tls     Refuse to start unless a certificate/key pair is set\n"
                 "  --help            Show this message\n"
                 "Environment fallbacks (used only when the ini leaves the value empty):\n"
                 "  HSBA_REST_AUTH_TOKEN, HSBA_REST_TLS_CERT, HSBA_REST_TLS_KEY\n",
                 argv0);
}

// Credential material is read from the environment only when the ini did not carry it, so a
// container image can keep restapi.ini token-free and inject the secret at run time.
void ApplyEnvFallbacks(HsBa::Slicer::RestApi::ServerConfig& cfg)
{
    const auto fill_from_env = [](const char* var_name, std::string& target) -> bool
    {
        if (!target.empty())
        {
            return false;
        }
        const char* const value = std::getenv(var_name);
        if (value == nullptr || *value == '\0')
        {
            return false;
        }
        target = value;
        return true;
    };
    if (fill_from_env("HSBA_REST_AUTH_TOKEN", cfg.auth_token))
    {
        cfg.auth_enabled = true;
    }
    fill_from_env("HSBA_REST_TLS_CERT", cfg.tls_cert);
    fill_from_env("HSBA_REST_TLS_KEY", cfg.tls_key);
}
}  // namespace

int main(int argc, char** argv)
{
    std::string config_path = "restapi.ini";
    std::string host_override;
    std::string token_override;
    std::string cert_override;
    std::string key_override;
    int port_override = 0;
    bool require_tls = false;

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h")
        {
            PrintUsage(argv[0]);
            return 0;
        }
        if (arg == "--require-tls")
        {
            require_tls = true;
        }
        else if ((arg == "--config" || arg == "-c") && i + 1 < argc)
        {
            config_path = argv[++i];
        }
        else if (arg == "--host" && i + 1 < argc)
        {
            host_override = argv[++i];
        }
        else if (arg == "--auth-token" && i + 1 < argc)
        {
            token_override = argv[++i];
        }
        else if (arg == "--tls-cert" && i + 1 < argc)
        {
            cert_override = argv[++i];
        }
        else if (arg == "--tls-key" && i + 1 < argc)
        {
            key_override = argv[++i];
        }
        else if (arg == "--port" && i + 1 < argc)
        {
            port_override = std::atoi(argv[++i]);
        }
        else
        {
            std::fprintf(stderr, "Unrecognized option: %s\n\n", arg.c_str());
            PrintUsage(argv[0]);
            return 2;
        }
    }

    HsBa::Slicer::RestApi::ServerConfig cfg;
    try
    {
        cfg = HsBa::Slicer::RestApi::LoadServerConfig(config_path);
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "Failed to parse %s: %s\n", std::filesystem::u8path(config_path).string().c_str(),
                     e.what());
        return 1;
    }

    ApplyEnvFallbacks(cfg);

    if (!host_override.empty())
    {
        cfg.host = host_override;
    }
    if (port_override > 0)
    {
        cfg.port = port_override;
    }
    // Command line outranks ini and environment; a non-empty --auth-token always arms the gate.
    if (!token_override.empty())
    {
        cfg.auth_token = token_override;
        cfg.auth_enabled = true;
    }
    if (!cert_override.empty())
    {
        cfg.tls_cert = cert_override;
    }
    if (!key_override.empty())
    {
        cfg.tls_key = key_override;
    }
    cfg.require_tls = cfg.require_tls || require_tls;

    return HsBa::Slicer::RestApi::RunServer(cfg);
}
