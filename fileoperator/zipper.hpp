#pragma once
#ifndef HSBA_SLICER_ZIPPER_HPP
#define HSBA_SLICER_ZIPPER_HPP

#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

#include <miniz.h>

#include "IZipper.hpp"
#include "base/delegate.hpp"

namespace HsBa::Slicer
{
/**
 * @brief Compression level enumeration for miniz.
 */
enum class MinizCompression
{
    Undefine,  ///< Undefined compression
    No,        ///< No compression (store only)
    Fast,      ///< Fast compression
    Tight,     ///< Maximum compression
    Unknown    ///< Unknown compression level
};

// Zipper use miniz
/**
 * @brief Miniz-based archive compressor implementation.
 *
 * This class provides ZIP archive creation using the miniz library,
 * supporting both file and in-memory data compression.
 */
class Zipper final : public IZipper, public Utils::EventSource<Zipper, void, double, std::string_view>
{
public:
    Zipper() = default;

    /**
     * @brief Construct a Zipper with specified compression level.
     * @param compression Compression level setting.
     */
    Zipper(MinizCompression compression);

    ~Zipper() = default;
    Zipper(const Zipper&) = delete;
    Zipper& operator=(const Zipper&) = delete;
    Zipper(Zipper&&) noexcept = delete;
    Zipper& operator=(Zipper&&) noexcept = delete;

    void AddByteFile(std::string_view name, const std::string& data) override;
    void AddFile(std::string_view name, std::string_view path) override;

    // To add duplicate file, filename add "_duplicate"
    void AddByteFileIgnoreDuplicate(std::string_view name, const std::string& data) override;
    void AddFileIgnoreDuplicate(std::string_view name, std::string_view path) override;
    void Save(std::string_view filePath) override;

private:
    struct Bytes
    {
        std::string data;
    };
    using BytesFileName = std::variant<Bytes, std::string>;
    using ByteFiles = std::unordered_map<std::string, BytesFileName>;
    ByteFiles byteFilesWaitCompress_;
    mz_uint compression_ = MZ_DEFAULT_COMPRESSION;
    const std::string duplicate_addition = "_duplicate";
    mz_bool AddAllToZip(/*in*/ mz_zip_archive& archiver);
    mz_bool ZipAddFile(/*ref*/ mz_zip_archive& archiver, const std::string& name, const std::string& path) const;
    mz_bool ZipAddMember(/*ref*/ mz_zip_archive& archiver, const std::string& name, const Bytes& bytes) const;
};

/**
 * @brief Extract all files from a ZIP archive to a directory.
 * @param archive_path Path to the ZIP archive.
 * @param output_path Output directory path.
 */
void MiniZExtractFile(std::string_view archive_path, std::string_view output_path);

/**
 * @brief Extract all files from a ZIP archive to memory buffers.
 * @param archive_path Path to the ZIP archive.
 * @return Map of filename to file content.
 */
std::unordered_map<std::string, std::string> MiniZExtractFileToBuffer(std::string_view archive_path);
}  // namespace HsBa::Slicer

#endif  // !HSBA_SLICER_ZIPPER_HPP