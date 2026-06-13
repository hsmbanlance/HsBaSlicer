#include "RemoteExecutor.hpp"

#include <algorithm>
#include <bit>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <limits>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace HsBa::Slicer
{
namespace
{
#ifdef _WIN32
void EnsureSocketInitialized()
{
    static bool initialized = []() -> bool
    {
        WSADATA wsaData;
        return WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;
    }();

    if (!initialized)
    {
        throw IOError("WSAStartup failed");
    }
}

std::string GetLastSocketError()
{
    int error = WSAGetLastError();
    char message[256] = {};
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, error,
                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), message, static_cast<DWORD>(sizeof(message)), nullptr);
    return std::string(message[0] != '\0' ? message : "Unknown socket error");
}
#else
void EnsureSocketInitialized() noexcept
{
}

std::string GetLastSocketError()
{
    return std::strerror(errno ? errno : 0);
}
#endif

std::string GetAddressInfoError(int error)
{
#ifdef _WIN32
    const char* msg = gai_strerrorA(error);
#else
    const char* msg = gai_strerror(error);
#endif
    return msg ? std::string(msg) : "Unknown address info error";
}

#ifdef _WIN32
using SocketHandle = SOCKET;
static constexpr SocketHandle InvalidSocketHandle = INVALID_SOCKET;
#else
using SocketHandle = int;
static constexpr SocketHandle InvalidSocketHandle = -1;
#endif

void SendAll(SocketHandle socket, const void* data, std::size_t size)
{
    const char* buffer = static_cast<const char*>(data);
    std::size_t remaining = size;

    while (remaining > 0)
    {
        int toSend = static_cast<int>(
            std::min<std::size_t>(remaining, static_cast<std::size_t>(std::numeric_limits<int>::max())));
        int result = ::send(socket, buffer, toSend, 0);

        if (result < 0)
        {
            throw IOError(std::string("RemoteExecutorConnection send failed: ") + GetLastSocketError());
        }
        else if (result == 0)
        {
            throw IOError("RemoteExecutorConnection send failed: connection closed by peer");
        }

        buffer += result;
        remaining -= static_cast<std::size_t>(result);
    }
}


std::uint64_t ByteSwap64(std::uint64_t value) noexcept
{
#if defined(__clang__) || defined(__GNUC__)
    return __builtin_bswap64(value);
#elif defined(_MSC_VER)
    return _byteswap_uint64(value);
#else
    return ((value & 0x00000000000000FFULL) << 56) | ((value & 0x000000000000FF00ULL) << 40) |
           ((value & 0x0000000000FF0000ULL) << 24) | ((value & 0x00000000FF000000ULL) << 8) |
           ((value & 0x000000FF00000000ULL) >> 8) | ((value & 0x0000FF0000000000ULL) >> 24) |
           ((value & 0x00FF000000000000ULL) >> 40) | ((value & 0xFF00000000000000ULL) >> 56);
#endif
}

static std::uint64_t HostToNetwork64(std::uint64_t value) noexcept
{
    if constexpr (std::endian::native == std::endian::big)
    {
        return value;
    }
    else
    {
        return ByteSwap64(value);
    }
}
}  // namespace

struct RemoteExecutorConnection::Impl
{
    Impl(std::string_view host, std::string_view port) : host_(host), port_(port) {}

    ~Impl() { CloseSocket(); }

    void SendFile(const std::filesystem::path& filePath)
    {
        EnsureConnected();

        std::ifstream input(filePath, std::ios::binary);
        if (!input)
        {
            throw IOError(std::string("Cannot open file: ") + filePath.string());
        }

        input.seekg(0, std::ios::end);
        auto pos = input.tellg();
        if (pos == std::streampos(-1))
        {
            throw IOError(std::string("Cannot determine file size: ") + filePath.string());
        }
        const auto fileSize = static_cast<std::uint64_t>(pos);
        input.seekg(0, std::ios::beg);

        std::string fileName = filePath.filename().string();
        std::uint64_t nameSize = static_cast<std::uint64_t>(fileName.size());

        std::uint64_t nameSizeNet = HostToNetwork64(nameSize);
        SendAll(socket_, &nameSizeNet, sizeof(nameSizeNet));
        SendAll(socket_, fileName.data(), fileName.size());

        std::uint64_t fileSizeNet = HostToNetwork64(fileSize);
        SendAll(socket_, &fileSizeNet, sizeof(fileSizeNet));

        constexpr std::size_t chunkSize = 64 * 1024;
        std::vector<char> buffer(chunkSize);
        while (input)
        {
            input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            std::streamsize readSize = input.gcount();
            if (readSize <= 0)
            {
                break;
            }
            SendAll(socket_, buffer.data(), static_cast<std::size_t>(readSize));
        }
    }

private:
    void EnsureConnected()
    {
        if (connected_ && socket_ != InvalidSocketHandle)
        {
            return;
        }

        CloseSocket();
#ifdef _WIN32
        EnsureSocketInitialized();
#endif

        addrinfo hints{};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        addrinfo* result = nullptr;
        int error = ::getaddrinfo(host_.c_str(), port_.c_str(), &hints, &result);
        if (error != 0)
        {
            throw IOError(std::string("RemoteExecutorConnection resolve failed: ") + GetAddressInfoError(error));
        }

        for (addrinfo* ptr = result; ptr != nullptr; ptr = ptr->ai_next)
        {
            socket_ = ::socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
            if (socket_ == InvalidSocketHandle)
            {
                continue;
            }

            if (::connect(socket_, ptr->ai_addr, static_cast<int>(ptr->ai_addrlen)) == 0)
            {
                connected_ = true;
                break;
            }

            CloseSocket();
        }

        freeaddrinfo(result);

        if (!connected_)
        {
            throw IOError(std::string("RemoteExecutorConnection connect failed: ") + GetLastSocketError());
        }
    }

    void CloseSocket() noexcept
    {
        if (socket_ == InvalidSocketHandle)
        {
            return;
        }

#ifdef _WIN32
        ::closesocket(socket_);
#else
        ::close(socket_);
#endif

        socket_ = InvalidSocketHandle;
        connected_ = false;
    }

    SocketHandle socket_ = InvalidSocketHandle;
    std::string host_;
    std::string port_;
    bool connected_ = false;
};

RemoteExecutorConnection::RemoteExecutorConnection(std::string_view host, std::string_view port)
    : impl_(std::make_unique<Impl>(host, port))
{
}

RemoteExecutorConnection::~RemoteExecutorConnection() = default;

void RemoteExecutorConnection::SendFile(const std::filesystem::path& filePath)
{
    impl_->SendFile(filePath);
}

RemoteExecutorConnectionPool::RemoteExecutorConnectionPool(std::string_view host, std::string_view port,
                                                           std::size_t poolSize)
    : host_(host), port_(port), poolSize_(poolSize > MaxConnections ? MaxConnections : poolSize)
{
    if (poolSize_ == 0)
    {
        throw InvalidArgumentError("RemoteExecutorConnectionPool requires at least one connection");
    }

    for (std::size_t i = 0; i < poolSize_; ++i)
    {
        connections_.emplace_back(std::make_shared<RemoteExecutorConnection>(host_, port_));
    }
}

RemoteExecutorConnectionPool::~RemoteExecutorConnectionPool() = default;

RemoteExecutorConnectionPool::ConnectionPtr RemoteExecutorConnectionPool::AcquireConnection()
{
    std::lock_guard lock(connectionMutex_);
    auto& connection = connections_[nextConnection_];
    nextConnection_ = (nextConnection_ + 1) % poolSize_;
    return connection;
}

void RemoteExecutorConnectionPool::SendFile(const std::filesystem::path& filePath)
{
    auto connection = AcquireConnection();
    if (!connection)
    {
        throw IOError("Invalid remote executor connection");
    }
    connection->SendFile(filePath);
}

void RemoteExecutorConnectionPool::SendFiles(const std::vector<std::filesystem::path>& filePaths)
{
    for (auto const& path : filePaths)
    {
        SendFile(path);
    }
}

}  // namespace HsBa::Slicer