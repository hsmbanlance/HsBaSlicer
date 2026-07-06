#pragma once
#ifndef HSBA_SLICER_REMOTE_EXECUTOR_HPP
#define HSBA_SLICER_REMOTE_EXECUTOR_HPP

#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "base/InplaceVector.hpp"
#include "base/error.hpp"

namespace HsBa::Slicer
{
/**
 * @brief Connection to a remote executor service.
 *
 * This class manages a connection to a remote service for executing tasks and transferring files.
 */
class RemoteExecutorConnection
{
public:
    /**
     * @brief Construct a new connection.
     * @param host Remote host address.
     * @param port Remote service port.
     */
    RemoteExecutorConnection(std::string_view host, std::string_view port);

    ~RemoteExecutorConnection();

    RemoteExecutorConnection(const RemoteExecutorConnection&) = delete;
    RemoteExecutorConnection& operator=(const RemoteExecutorConnection&) = delete;
    RemoteExecutorConnection(RemoteExecutorConnection&&) = delete;
    RemoteExecutorConnection& operator=(RemoteExecutorConnection&&) = delete;

    /**
     * @brief Send a file to the remote executor.
     * @param filePath Path to the file to send.
     */
    void SendFile(const std::filesystem::path& filePath);

private:
    struct Impl;  ///< Implementation details (PIMPL pattern)
    std::unique_ptr<Impl> impl_;
};

/**
 * @brief Connection pool for remote executor services.
 *
 * This class manages a pool of connections to improve performance when sending multiple files.
 */
class RemoteExecutorConnectionPool
{
public:
    static constexpr std::size_t MaxConnections = 16;  ///< Maximum number of connections in the pool

    /**
     * @brief Construct a connection pool.
     * @param host Remote host address.
     * @param port Remote service port.
     * @param poolSize Number of connections in the pool (default: MaxConnections).
     */
    RemoteExecutorConnectionPool(std::string_view host, std::string_view port, std::size_t poolSize = MaxConnections);

    ~RemoteExecutorConnectionPool();

    /**
     * @brief Send a single file using a connection from the pool.
     * @param filePath Path to the file to send.
     */
    void SendFile(const std::filesystem::path& filePath);

    /**
     * @brief Send multiple files using connections from the pool.
     * @param filePaths Vector of file paths to send.
     */
    void SendFiles(const std::vector<std::filesystem::path>& filePaths);

private:
    using ConnectionPtr = std::shared_ptr<RemoteExecutorConnection>;

    /**
     * @brief Acquire a connection from the pool (round-robin).
     * @return Shared pointer to a connection.
     */
    ConnectionPtr AcquireConnection();

    Utils::InplaceVector<ConnectionPtr, MaxConnections> connections_;  ///< Pool of connections
    std::mutex connectionMutex_;                                       ///< Mutex for thread-safe access
    std::string host_;                                                 ///< Remote host address
    std::string port_;                                                 ///< Remote service port
    std::size_t poolSize_ = 0;                                         ///< Current pool size
    std::size_t nextConnection_ = 0;                                   ///< Next connection index for round-robin
};

}  // namespace HsBa::Slicer

#endif  // HSBA_SLICER_REMOTE_EXECUTOR_HPP
