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
                 "  --help            Show this message\n",
                 argv0);
}
}  // namespace

int main(int argc, char** argv)
{
    std::string config_path = "restapi.ini";
    std::string host_override;
    int port_override = 0;

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h")
        {
            PrintUsage(argv[0]);
            return 0;
        }
        if ((arg == "--config" || arg == "-c") && i + 1 < argc)
        {
            config_path = argv[++i];
        }
        else if (arg == "--host" && i + 1 < argc)
        {
            host_override = argv[++i];
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

    if (!host_override.empty())
    {
        cfg.host = host_override;
    }
    if (port_override > 0)
    {
        cfg.port = port_override;
    }

    return HsBa::Slicer::RestApi::RunServer(cfg);
}
