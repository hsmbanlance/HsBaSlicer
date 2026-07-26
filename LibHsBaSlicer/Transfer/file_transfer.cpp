#include "file_transfer.hpp"

#include "fileoperator/RemoteExecutor.hpp"

namespace HsBa::Slicer
{

namespace
{

void ReportProgress(const FileTransferProgressFunc& progress, int percent, std::string_view stage)
{
    if (progress)
    {
        progress(percent, stage);
    }
}

}  // anonymous namespace

HSBA_SLICER_LIB_API FileTransferResult TransferFiles(const FileTransferConfig& config,
                                                     FileTransferProgressFunc progress)
{
    FileTransferResult result;
    result.total_files = static_cast<int>(config.files.size());

    // ========== Stage 1: Validate ==========
    ReportProgress(progress, 0, "Validating configuration...");

    if (config.host.empty())
    {
        result.error_message = "Host address must not be empty";
        return result;
    }
    if (config.port.empty())
    {
        result.error_message = "Port must not be empty";
        return result;
    }
    if (config.files.empty())
    {
        result.error_message = "No files specified for transfer";
        return result;
    }

    for (const auto& path : config.files)
    {
        if (!std::filesystem::exists(path))
        {
            result.error_message = "File not found: " + path.string();
            return result;
        }
    }
    ReportProgress(progress, 10, "Validation complete");

    // ========== Stage 2: Connect ==========
    ReportProgress(progress, 15, "Establishing connections...");

    std::size_t pool_size = config.pool_size;
    if (pool_size == 0)
    {
        pool_size = 1;
    }
    if (pool_size > RemoteExecutorConnectionPool::MaxConnections)
    {
        pool_size = RemoteExecutorConnectionPool::MaxConnections;
    }

    RemoteExecutorConnectionPool pool(config.host, config.port, pool_size);
    ReportProgress(progress, 20, "Connections established");

    // ========== Stage 3: Transfer Files ==========
    ReportProgress(progress, 25, "Transferring files...");

    const int total = static_cast<int>(config.files.size());
    for (int i = 0; i < total; ++i)
    {
        pool.SendFile(config.files[static_cast<std::size_t>(i)]);
        result.files_transferred = i + 1;

        int pct = 25 + ((i + 1) * 70) / total;
        ReportProgress(progress, pct, "Transferring file");
    }

    result.success = true;
    ReportProgress(progress, 100, "Transfer complete");
    return result;
}

}  // namespace HsBa::Slicer
