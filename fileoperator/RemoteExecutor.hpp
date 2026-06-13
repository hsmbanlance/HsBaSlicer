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
class RemoteExecutorConnection
{
public:
    RemoteExecutorConnection(std::string_view host, std::string_view port);
    ~RemoteExecutorConnection();
    RemoteExecutorConnection(const RemoteExecutorConnection&) = delete;
    RemoteExecutorConnection& operator=(const RemoteExecutorConnection&) = delete;
    RemoteExecutorConnection(RemoteExecutorConnection&&) = delete;
    RemoteExecutorConnection& operator=(RemoteExecutorConnection&&) = delete;

    void SendFile(const std::filesystem::path& filePath);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

class RemoteExecutorConnectionPool
{
public:
    static constexpr std::size_t MaxConnections = 16;

    RemoteExecutorConnectionPool(std::string_view host, std::string_view port, std::size_t poolSize = MaxConnections);
    ~RemoteExecutorConnectionPool();

    void SendFile(const std::filesystem::path& filePath);
    void SendFiles(const std::vector<std::filesystem::path>& filePaths);

private:
    using ConnectionPtr = std::shared_ptr<RemoteExecutorConnection>;

    ConnectionPtr AcquireConnection();

    Utils::InplaceVector<ConnectionPtr, MaxConnections> connections_;
    std::mutex connectionMutex_;
    std::string host_;
    std::string port_;
    std::size_t poolSize_ = 0;
    std::size_t nextConnection_ = 0;
};

}  // namespace HsBa::Slicer

#endif  // HSBA_SLICER_REMOTE_EXECUTOR_HPP
