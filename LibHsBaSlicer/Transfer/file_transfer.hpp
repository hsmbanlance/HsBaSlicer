#pragma once
#ifndef HSBA_SLICER_LIB_FILE_TRANSFER_HPP
#define HSBA_SLICER_LIB_FILE_TRANSFER_HPP

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "../export.h"

namespace HsBa::Slicer
{

/**
 * @brief File transfer configuration.
 */
struct FileTransferConfig
{
    std::string host;                          ///< Remote host address
    std::string port;                          ///< Remote service port
    std::size_t pool_size = 4;                 ///< Connection pool size [1, 16]
    std::vector<std::filesystem::path> files;  ///< Files to transfer
};

/**
 * @brief File transfer result.
 */
struct FileTransferResult
{
    bool success = false;       ///< Whether all files were transferred successfully
    int files_transferred = 0;  ///< Number of files successfully transferred
    int total_files = 0;        ///< Total number of files requested
    std::string error_message;  ///< Error message (empty on success)
};

/**
 * @brief Progress callback for file transfer.
 * @param percent Progress percentage (0-100).
 * @param stage Current stage description.
 */
using FileTransferProgressFunc = std::function<void(int percent, std::string_view stage)>;

/**
 * @brief Transfer files to a remote executor service.
 *
 * Validates file existence, establishes a connection pool, and sends
 * all files sequentially with progress reporting.
 *
 * @param config Transfer configuration (host, port, files).
 * @param progress Optional progress callback (can be nullptr).
 * @return Transfer result with success status and counts.
 */
HSBA_SLICER_LIB_API FileTransferResult TransferFiles(const FileTransferConfig& config,
                                                     FileTransferProgressFunc progress = nullptr);

}  // namespace HsBa::Slicer

#endif  // !HSBA_SLICER_LIB_FILE_TRANSFER_HPP
