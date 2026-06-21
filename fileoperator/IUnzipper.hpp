#pragma once
#ifndef HSBA_SLICER_IUNZIPPER_HPP
#define HSBA_SLICER_IUNZIPPER_HPP

#include <any>
#include <fstream>
#include <istream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <variant>

#include "base/error.hpp"
#include "base/template_helper.hpp"

namespace HsBa::Slicer
{
/**
 * @brief Stream class for reading extracted files from archives.
 *
 * This class provides a std::istream interface for accessing files within compressed archives,
 * supporting both in-memory buffers and temporary file extraction.
 */
class UnzipperStream : public std::istream
{
public:
    /**
     * @brief Default constructor (creates invalid stream).
     */
    UnzipperStream() : std::istream{nullptr} { rdbuf(nullptr); }

    /**
     * @brief Construct from a file path.
     * @param fileName Path to the file.
     * @param openmode File open mode flags.
     * @throws IOError if file cannot be opened.
     */
    UnzipperStream(std::string_view fileName, std::ios_base::openmode openmode)
        : std::istream{nullptr}, stream_{std::ifstream(fileName.data(), openmode)}
    {
        if (std::get_if<std::ifstream>(&stream_)->fail())
        {
            throw IOError("Failed to open file");
        }
        rdbuf(std::get<std::ifstream>(stream_).rdbuf());
    }

    /**
     * @brief Construct from a string data buffer.
     * @param data String data to read from.
     */
    UnzipperStream(std::string_view data) : std::istream{nullptr}, stream_{std::istringstream{std::string{data}}}
    {
        rdbuf(std::get<std::istringstream>(stream_).rdbuf());
    }

    UnzipperStream(const UnzipperStream&) = delete;
    UnzipperStream& operator=(const UnzipperStream&) = delete;

    /**
     * @brief Move constructor.
     */
    UnzipperStream(UnzipperStream&& o) noexcept : std::istream{nullptr}
    {
        stream_ = std::move(o.stream_);
        rdbuf(o.rdbuf());
        o.rdbuf(nullptr);
    }

    /**
     * @brief Move assignment operator.
     */
    UnzipperStream& operator=(UnzipperStream&& o) noexcept
    {
        if (this != &o)
        {
            stream_ = std::move(o.stream_);
            rdbuf(o.rdbuf());
            o.rdbuf(nullptr);
        }
        return *this;
    }
    ~UnzipperStream()
    {
        std::visit(Utils::Overloaded{[](std::ifstream& ifs)
                                     {
                                         if (ifs.is_open())
                                         {
                                             ifs.close();
                                         }
                                     },
                                     [](std::istringstream&) {}},
                   stream_);
    }


    /**
     * @brief Buffer structure for in-memory data storage.
     */
    struct Buffer
    {
        Buffer() = default;

        /**
         * @brief Construct buffer with specified size.
         * @param size Buffer size in bytes.
         */
        Buffer(size_t size)
        {
            data = std::make_shared<char[]>(size);
            this->size = size;
        }
        std::shared_ptr<char[]> data = nullptr;  ///< Shared pointer to buffer data
        size_t size{0};                          ///< Buffer size in bytes
    };

    using BufferOrFile = std::variant<Buffer, std::string>;  ///< Variant holding either buffer or file path

    /**
     * @brief Factory method to create UnzipperStream from buffer or file.
     * @param data BufferOrFile variant containing either buffer or file path.
     * @return Shared pointer to created UnzipperStream.
     */
    inline static std::shared_ptr<UnzipperStream> MakeUnzipperStream(const BufferOrFile& data)
    {
        auto res = std::visit(
            Utils::Overloaded{
                [](const Buffer& buff) -> std::shared_ptr<UnzipperStream>
                { return std::make_shared<UnzipperStream>(std::string{buff.data.get(), buff.size}); },
                [](const std::string& str) -> std::shared_ptr<UnzipperStream>
                { return std::make_shared<UnzipperStream>(str, std::ios_base::binary | std::ios_base::in); }},
            data);
        return res;
    }

    /**
     * @brief Set the unzipper implementation pointer.
     * @tparam T Type of the unzipper implementation.
     * @param ptr Shared pointer to the unzipper.
     */
    template <typename T>
    void SetFrom(std::shared_ptr<T> ptr)
    {
        unzipper_ = ptr;
    }

private:
    std::variant<std::ifstream, std::istringstream> stream_;  ///< Underlying stream source
    // Removed CloseStream and MakeOperator structs since they're now replaced with Overloaded lambdas
    std::any unzipper_;  ///< Type-erased pointer to unzipper implementation
};

/**
 * @brief Interface for archive extraction (unzipper) implementations.
 *
 * This CRTP-based interface provides common functionality for different archive formats.
 * @tparam Derived The concrete implementation class.
 */
template <typename Derived>
class IUnzipper
{
public:
    /**
     * @brief Read archive from file.
     * @param path Path to the archive file.
     * @param reopen Whether to reopen if already open (default: false).
     */
    void ReadFromFile(std::string_view path, bool reopen = false)
    {
        static_cast<Derived*>(this)->ReadFromFileImpl(path, reopen);
    }

    /**
     * @brief Get a stream for a specific file within the archive.
     * @param part_file Name of the file within the archive.
     * @return Shared pointer to UnzipperStream for reading the file.
     */
    std::shared_ptr<UnzipperStream> GetStream(std::string_view part_file)
    {
        return static_cast<Derived*>(this)->GetStreamImpl(part_file);
    }

    IUnzipper(const IUnzipper&) = delete;
    IUnzipper& operator=(const IUnzipper) = delete;
    IUnzipper(IUnzipper&&) = delete;
    IUnzipper& operator=(IUnzipper&&) = delete;

protected:
    IUnzipper() = default;
    ~IUnzipper() = default;
};

}  // namespace HsBa::Slicer

#endif  // !HSBA_SLICER_IUNZIPPER_HPP