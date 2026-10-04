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
void Bit7zExtract(const std::string& archive, const std::string& outdir, const std::string& password = "",
                  const std::string& dll_path = HSBA_7Z_DLL);
void Bit7zExtract(const std::string& archive,
                  /*out*/ std::map<std::string, std::vector<bit7z::byte_t>>& bufs, const std::string& password = "",
                  const std::string& dll_path = HSBA_7Z_DLL);

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
    TarGz,  // tar archive compressed with gzip (.tar.gz / .tgz)
    TarXz,  // tar archive compressed with xz (.tar.xz / .txz)
    // only support extract
    RAR,
    ISO,
    Z,
    Unknown
};

class Bit7zZipper final : public IZipper, public Utils::EventSource<Bit7zZipper, void, double, std::string_view>
{
public:
    Bit7zZipper() : format_{ZipperFormat::SevenZip}, dll_path_{HSBA_7Z_DLL} {}
    Bit7zZipper(std::string_view dll_path, ZipperFormat format, std::string_view password)
        : dll_path_{dll_path}, format_{format}, password_{password}
    {
    }
    void AddByteFile(std::string_view name, const std::vector<bit7z::byte_t>& data);
    void AddByteFile(std::string_view name, const std::string& data) override;
    void AddFile(std::string_view name, std::string_view path) override;
    // To add duplicate file, filename add "_duplicate"
    void AddByteFileIgnoreDuplicate(std::string_view name, const std::vector<bit7z::byte_t>& data);
    void AddByteFileIgnoreDuplicate(std::string_view name, const std::string& data) override;
    void AddFileIgnoreDuplicate(std::string_view name, std::string_view path) override;
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
