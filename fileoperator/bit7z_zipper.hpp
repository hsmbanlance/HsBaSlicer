/**
 * @file bit7z_zipper.hpp
 * @brief Declares the bit7z-based compressor (@ref Bit7zZipper) and standalone extraction helpers.
 */
#pragma once
#ifndef HSBA_SLICER_BIT7Z_ZIPPER_HPP
#define HSBA_SLICER_BIT7Z_ZIPPER_HPP

#include <map>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#ifdef HSBA_USE_BIT7Z
#include <bit7z/bit7z.hpp>
#endif  // HSBA_USE_BIT7Z

#include "IZipper.hpp"
#include "base/delegate.hpp"
#include "bit7z_def.hpp"

namespace HsBa::Slicer
{
#ifdef HSBA_USE_BIT7Z
/**
 * @brief Extract an archive into a destination directory.
 * @param archive Path to the archive file.
 * @param outdir Destination directory.
 * @param password Optional password for encrypted archives.
 * @param dll_path Path to the 7z DLL/shared library.
 *
 * Compressed tar archives are unpacked in one step via the nested inner-tar reader.
 */
void Bit7zExtract(const std::string& archive, const std::string& outdir, const std::string& password = "",
                  const std::string& dll_path = HSBA_7Z_DLL);
/**
 * @brief Extract an archive into an in-memory name-to-bytes map.
 * @param archive Path to the archive file.
 * @param bufs Output map receiving each entry name and its uncompressed bytes.
 * @param password Optional password for encrypted archives.
 * @param dll_path Path to the 7z DLL/shared library.
 */
void Bit7zExtract(const std::string& archive,
                  /*out*/ std::map<std::string, std::vector<bit7z::byte_t>>& bufs, const std::string& password = "",
                  const std::string& dll_path = HSBA_7Z_DLL);

/**
 * @brief Archive container formats supported by @ref Bit7zZipper.
 *
 * Zip, SevenZip, XZ, BZIP2, GZIP, TAR, TarGz and TarXz support both compression and extraction;
 * RAR, ISO and Z are extraction-only. Unknown/Undefine select no valid format.
 */
enum class ZipperFormat
{
    Undefine,
    // support all

    Zip,
    SevenZip,
    XZ,
    BZIP2,
    GZIP,
    TAR,
    TarGz,  ///< tar archive compressed with gzip (.tar.gz / .tgz)
    TarXz,  ///< tar archive compressed with xz (.tar.xz / .txz)
    // only support extract
    RAR,
    ISO,
    Z,
    Unknown
};

/**
 * @brief bit7z-based archive compressor supporting multiple container formats.
 *
 * Stages files or in-memory buffers, then writes them to a ZIP/7z/XZ/... archive; TarGz and TarXz
 * are produced through two-stage in-memory packaging (items to tar, then tar to the compression).
 * Reports per-entry progress through the EventSource mechanism.
 */
class Bit7zZipper final : public IZipper, public Utils::EventSource<Bit7zZipper, void, double, std::string_view>
{
public:
    /// @brief Construct a Zipper defaulting to the 7z format and the platform 7z library.
    Bit7zZipper() : format_{ZipperFormat::SevenZip}, dll_path_{HSBA_7Z_DLL} {}
    /**
     * @brief Construct a Zipper with an explicit library path, output format, and password.
     * @param dll_path Path to the 7z DLL/shared library.
     * @param format Target archive format.
     * @param password Password for encrypted output archives.
     */
    Bit7zZipper(std::string_view dll_path, ZipperFormat format, std::string_view password)
        : dll_path_{dll_path}, format_{format}, password_{password}
    {
    }
    /// @brief Stage an in-memory byte buffer under @p name (fails on duplicate names).
    void AddByteFile(std::string_view name, const std::vector<bit7z::byte_t>& data);
    /// @brief Stage an in-memory string under @p name (fails on duplicate names).
    void AddByteFile(std::string_view name, const std::string& data) override;
    /// @brief Stage an on-disk file under @p name (fails on duplicate names).
    void AddFile(std::string_view name, std::string_view path) override;
    /// @brief Stage an in-memory byte buffer, renaming to "<name>_duplicate" on collision.
    void AddByteFileIgnoreDuplicate(std::string_view name, const std::vector<bit7z::byte_t>& data);
    /// @brief Stage an in-memory string, renaming to "<name>_duplicate" on collision.
    void AddByteFileIgnoreDuplicate(std::string_view name, const std::string& data) override;
    /// @brief Stage an on-disk file, renaming to "<name>_duplicate" on collision.
    void AddFileIgnoreDuplicate(std::string_view name, std::string_view path) override;
    /// @brief Compress all staged entries into the archive at @p filePath using the configured format.
    void Save(std::string_view filePath) override;

private:
    using Bytes = std::vector<bit7z::byte_t>;
    using BytesFileName = std::variant<Bytes, std::string>;
    using ByteFiles = std::map<std::string, BytesFileName>;
    ByteFiles byteFilesWaitCompress_;
    const std::string duplicate_addition = "_duplicate";
    std::string dll_path_;
    ZipperFormat format_;
    std::string password_;
    void AddAllWaitFiles(bit7z::BitArchiveWriter& compress);
    void SaveAllFile(bit7z::BitArchiveWriter& compress, const std::string& path);
    void SaveAllFile(bit7z::BitArchiveWriter& compress, bit7z::buffer_t& out_buffer);
    // Two-stage packaging: items -> tar (memory) -> gzip/xz (file), producing .tar.gz/.tar.xz directly.
    void SaveCompressedTar(const bit7z::Bit7zLibrary& lib, const bit7z::BitInOutFormat& compress_format,
                           const std::string& path);
};
#endif  // HSBA_USE_BIT7Z
}  // namespace HsBa::Slicer
#endif  // !HSBA_SLICER_BIT7Z_ZIPPER_HPP
