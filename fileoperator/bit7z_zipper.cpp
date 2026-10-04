#include "bit7z_zipper.hpp"

#include <format>
#include <fstream>
#include <memory>

#ifdef HSBA_USE_BIT7Z
#include <bit7z/bitexception.hpp>
#include <bit7z/bitfilecompressor.hpp>
#include <bit7z/bitfileextractor.hpp>
#endif  // HSBA_USE_BIT7Z

#include "base/encoding_convert.hpp"
#include "base/error.hpp"
#include "base/template_helper.hpp"

namespace HsBa::Slicer
{
#ifdef HSBA_USE_BIT7Z
namespace
{
// 7z sees only the compression layer of .tar.gz/.tar.xz (a single .tar entry): unpack the inner
// tar into a temporary file and open it, so the real archive files are accessible in one step.
// Returns nullptr when the archive is not a real compressed tar (temp file already removed).
std::unique_ptr<bit7z::BitArchiveReader> OpenInnerTarReader(const bit7z::Bit7zLibrary& lib, const std::string& archive,
                                                            const std::string& password,
                                                            /*out*/ std::string& out_temp_tar)
{
    std::filesystem::path temp_tar = MakeInnerTarTempPath(archive);
    try
    {
        bit7z::BitArchiveReader outer{lib, archive, bit7z::BitFormat::Auto, password};
        std::ofstream ofs(temp_tar, std::ios_base::out | std::ios_base::binary);
        outer.extractTo(ofs, 0u);
        ofs.close();
        out_temp_tar = temp_tar.string();
        return std::make_unique<bit7z::BitArchiveReader>(lib, out_temp_tar, bit7z::BitFormat::Tar, password);
    }
    catch (const bit7z::BitException&)
    {
        std::filesystem::remove(temp_tar);
        return nullptr;
    }
}
}  // namespace

void Bit7zExtract(const std::string& archive, const std::string& outdir, const std::string& password,
                  const std::string& dll_path)
{
    bit7z::Bit7zLibrary lib{dll_path};
    if (IsCompressedTarPath(archive))
    {
        std::string temp_tar;
        auto inner = OpenInnerTarReader(lib, archive, password, temp_tar);
        if (inner)
        {
            try
            {
                inner->extractTo(outdir);
            }
            catch (...)
            {
                inner.reset();
                std::filesystem::remove(temp_tar);
                throw;
            }
            inner.reset();
            std::filesystem::remove(temp_tar);
            return;
        }
        // Not a real compressed tar: fall back to the plain extraction below.
    }
    bit7z::BitFileExtractor ex{lib, bit7z::BitFormat::SevenZip};
    if (!password.empty())
    {
        ex.setPassword(password);
    }
    ex.extract(archive, outdir);
}
void Bit7zExtract(const std::string& archive, std::map<std::string, std::vector<bit7z::byte_t>>& bufs,
                  const std::string& password, const std::string& dll_path)
{
    bit7z::Bit7zLibrary lib{dll_path};
    if (IsCompressedTarPath(archive))
    {
        std::string temp_tar;
        auto inner = OpenInnerTarReader(lib, archive, password, temp_tar);
        if (inner)
        {
            try
            {
                inner->extractTo(bufs);
            }
            catch (...)
            {
                inner.reset();
                std::filesystem::remove(temp_tar);
                throw;
            }
            inner.reset();
            std::filesystem::remove(temp_tar);
            return;
        }
        // Not a real compressed tar: fall back to the plain extraction below.
    }
    bit7z::BitFileExtractor ex{lib, bit7z::BitFormat::SevenZip};
    if (!password.empty())
    {
        ex.setPassword(password);
    }
    ex.extract(archive, bufs);
}

void Bit7zZipper::AddByteFile(std::string_view name, const std::vector<bit7z::byte_t>& data)
{
    auto [add, add_res] = byteFilesWaitCompress_.emplace(name, data);
    if (!add_res)
    {
        throw InvalidArgumentError("Duplicate name files");
    }
}

void Bit7zZipper::AddByteFile(std::string_view name, const std::string& data)
{
    std::vector<bit7z::byte_t> bytes(data.size());
    for (size_t i = 0; i != data.size(); ++i)
    {
        bytes[i] = static_cast<bit7z::byte_t>(data[i]);
    }
    AddByteFile(name, bytes);
}

void Bit7zZipper::AddFile(std::string_view name, std::string_view path)
{
    auto [add, add_res] = byteFilesWaitCompress_.emplace(name, std::string(path));
    if (!add_res)
    {
        throw InvalidArgumentError("Duplicate name files");
    }
}

void Bit7zZipper::AddByteFileIgnoreDuplicate(std::string_view name, const std::vector<bit7z::byte_t>& data)
{
    if (byteFilesWaitCompress_.count(std::string{name}))
    {
        byteFilesWaitCompress_.emplace((std::string(name) + duplicate_addition), data);
    }
    else
    {
        byteFilesWaitCompress_.emplace(name, Bytes{data});
    }
}

void Bit7zZipper::AddByteFileIgnoreDuplicate(std::string_view name, const std::string& data)
{
    std::vector<bit7z::byte_t> bytes(data.size());
    for (size_t i = 0; i != data.size(); ++i)
    {
        bytes[i] = static_cast<bit7z::byte_t>(data[i]);
    }
    AddByteFile(name, bytes);
}

void Bit7zZipper::AddFileIgnoreDuplicate(std::string_view name, std::string_view path)
{
    if (byteFilesWaitCompress_.count(std::string{name}))
    {
        byteFilesWaitCompress_.emplace((std::string(name) + duplicate_addition), std::string(path));
    }
    else
    {
        byteFilesWaitCompress_.emplace(name, std::string(path));
    }
}

void Bit7zZipper::Save(std::string_view filePath)
{
    std::string path = std::filesystem::path(filePath).make_preferred().string();
    path = utf8_to_local(path);
    try
    {
        bit7z::Bit7zLibrary lib{dll_path_};
        switch (format_)
        {
        case HsBa::Slicer::ZipperFormat::Zip:
        {
            bit7z::BitArchiveWriter compress(lib, bit7z::BitFormat::Zip);
            SaveAllFile(compress, path);
            break;
        }
        case HsBa::Slicer::ZipperFormat::SevenZip:
        {
            bit7z::BitArchiveWriter compress(lib, bit7z::BitFormat::SevenZip);
            SaveAllFile(compress, path);
            break;
        }
        case HsBa::Slicer::ZipperFormat::XZ:
        {
            bit7z::BitArchiveWriter compress(lib, bit7z::BitFormat::Xz);
            SaveAllFile(compress, path);
            break;
        }
        case HsBa::Slicer::ZipperFormat::BZIP2:
        {
            bit7z::BitArchiveWriter compress(lib, bit7z::BitFormat::BZip2);
            SaveAllFile(compress, path);
            break;
        }
        case HsBa::Slicer::ZipperFormat::GZIP:
        {
            bit7z::BitArchiveWriter compress(lib, bit7z::BitFormat::GZip);
            SaveAllFile(compress, path);
            break;
        }
        case HsBa::Slicer::ZipperFormat::TAR:
        {
            bit7z::BitArchiveWriter compress(lib, bit7z::BitFormat::Tar);
            SaveAllFile(compress, path);
            break;
        }
        case HsBa::Slicer::ZipperFormat::TarGz:
        {
            SaveCompressedTar(lib, bit7z::BitFormat::GZip, path);
            break;
        }
        case HsBa::Slicer::ZipperFormat::TarXz:
        {
            SaveCompressedTar(lib, bit7z::BitFormat::Xz, path);
            break;
        }
        default:
            throw NotSupportedError("Unsupported format");
            break;
        }
    }
    catch (const bit7z::BitException& e)
    {
        throw IOError(std::format("Failed to save zipper file, see {}", e.what()));
    }
}

void Bit7zZipper::AddAllWaitFiles(bit7z::BitArchiveWriter& compress)
{
    size_t fileCount = byteFilesWaitCompress_.size();
    size_t currentFileIndex = 0;
    compress.setPassword(password_);
    compress.setOverwriteMode(bit7z::OverwriteMode::Overwrite);
    for (const auto& [name, bytes] : byteFilesWaitCompress_)
    {
        std::visit(Utils::Overloaded{[&name, &compress](const std::string& arg) { compress.addFile(arg, name); },
                                     [&name, &compress](const Bytes& arg) { compress.addFile(arg, name); }},
                   bytes);
        double progress = static_cast<double>(currentFileIndex) / fileCount;
        RaiseEvent(progress, name);
        ++currentFileIndex;
    }
}

void Bit7zZipper::SaveAllFile(bit7z::BitArchiveWriter& compress, const std::string& path)
{
    AddAllWaitFiles(compress);
    compress.compressTo(path);
}

void Bit7zZipper::SaveAllFile(bit7z::BitArchiveWriter& compress, bit7z::buffer_t& out_buffer)
{
    AddAllWaitFiles(compress);
    compress.compressTo(out_buffer);
}

void Bit7zZipper::SaveCompressedTar(const bit7z::Bit7zLibrary& lib, const bit7z::BitInOutFormat& compress_format,
                                    const std::string& path)
{
    // Two-stage packaging performed fully in memory: the wait-to-compress items are first packed
    // into a tar archive, then the tar bytes are compressed into the final .tar.gz/.tar.xz file.
    bit7z::BitArchiveWriter tar{lib, bit7z::BitFormat::Tar};
    bit7z::buffer_t tar_buffer;
    SaveAllFile(tar, tar_buffer);

    bit7z::BitArchiveWriter compressed{lib, compress_format};
    compressed.setPassword(password_);
    compressed.setOverwriteMode(bit7z::OverwriteMode::Overwrite);
    compressed.addFile(tar_buffer, "archive.tar");
    compressed.compressTo(path);
}

#endif  // HSBA_USE_BIT7Z
}  // namespace HsBa::Slicer