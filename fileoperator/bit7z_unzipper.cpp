/**
 * @file bit7z_unzipper.cpp
 * @brief Implements the Bit7z-backed archive extractor (@ref Bit7ZUnzipper).
 *
 * All definitions in this file are compiled only when HSBA_USE_BIT7Z is defined, since they rely
 * on the bit7z library and its archive-reader types.
 */
#include "bit7z_unzipper.hpp"

#include <boost/uuid.hpp>

namespace HsBa::Slicer
{
#ifdef HSBA_USE_BIT7Z
/**
 * @brief Destroys the unzipper and releases every owned resource.
 *
 * Marks the archive as closed, removes the temporary cache directory when one was created for large
 * entries, and deletes any temporary inner-tar file left over from a compressed tar archive.
 */
Bit7ZUnzipper::~Bit7ZUnzipper()
{
    if (is_open_)
    {
        is_open_ = false;
    }
    if (use_cache_dir_)
    {
        if (std::filesystem::exists(cache_dir_))
        {
            std::filesystem::remove_all(cache_dir_);
        }
    }
    ClearInnerTarTempFile();
}

/**
 * @brief Delete the temporary inner-tar file and clear its recorded path.
 *
 * The archive reader is released first so the OS file handle is closed before the deletion, which is
 * required on Windows. The call is a no-op when no temporary file is present.
 */
void Bit7ZUnzipper::ClearInnerTarTempFile()
{
    if (!inner_tar_path_.empty())
    {
        archiver_.reset();  // release the file handle before deleting on Windows
        std::filesystem::remove(inner_tar_path_);
        inner_tar_path_.clear();
    }
}

/**
 * @brief Open the archive located at @p path so its entries can later be streamed.
 * @param path Path to the archive file to open.
 * @param reopen When true the archive is re-opened even if @p path is already open; when false and
 *               the same path is already open the call returns immediately as a no-op.
 *
 * Compressed tar archives (.tar.gz / .tgz / .tar.xz / .txz) are handled transparently: 7z exposes
 * only the compression layer as a single inner .tar entry, so that entry is unpacked to a temporary
 * file which is then re-opened as a TAR archive, making the real member files directly addressable.
 * If the inner-tar fallback fails, the archive is opened normally instead. (Re-)opening also
 * discards the previous memory and cache-directory caches so stale entries are never served.
 *
 * @throws bit7z::BitException if the archive cannot be opened with the configured library/password.
 */
void Bit7ZUnzipper::ReadFromFileImpl(std::string_view path, bool reopen)
{
    if (is_open_)
    {
        if (path == archiver_path_ && !reopen)
        {
            return;
        }
        is_open_ = false;
    }
    bit7z::Bit7zLibrary lib{dll_path_};
    archiver_.reset();
    ClearInnerTarTempFile();
    std::string archive_path = std::string{path};
    if (IsCompressedTarPath(archive_path))
    {
        // 7z sees only the compression layer of .tar.gz/.tar.xz (a single .tar entry): unpack the
        // inner tar to a temporary file and read it, so the real archive files are available directly.
        try
        {
            inner_tar_path_ = MakeInnerTarTempPath(archive_path);
            bit7z::BitArchiveReader outer{lib, archive_path, bit7z::BitFormat::Auto, password_};
            std::ofstream ofs(inner_tar_path_, std::ios_base::out | std::ios_base::binary);
            outer.extractTo(ofs, 0u);
            ofs.close();
            archiver_ = std::make_unique<bit7z::BitArchiveReader>(
                lib, inner_tar_path_.string(), bit7z::ArchiveStartOffset::FileStart, bit7z::BitFormat::Tar, password_);
        }
        catch (const bit7z::BitException&)
        {
            // Not a real compressed tar: read the archive itself instead.
            ClearInnerTarTempFile();
            archiver_ = std::make_unique<bit7z::BitArchiveReader>(
                lib, archive_path, bit7z::ArchiveStartOffset::FileStart, bit7z::BitFormat::Auto, password_);
        }
    }
    else
    {
        archiver_ = std::make_unique<bit7z::BitArchiveReader>(lib, archive_path, bit7z::ArchiveStartOffset::FileStart,
                                                              bit7z::BitFormat::Auto, password_);
    }
    archiver_path_ = archive_path;
    is_open_ = true;
    if (use_cache_dir_)
    {
        if (std::filesystem::exists(cache_dir_))
        {
            std::filesystem::remove_all(cache_dir_);
        }
    }
    use_cache_dir_ = false;
    memory_cache_.clear();
}
/**
 * @brief Create a deterministic temporary cache directory for large extracted entries.
 *
 * The directory lives under the current working path and is named from a UUID derived from the
 * archive path, so the same archive always maps to the same cache folder (any pre-existing folder
 * with that name is removed first). The call is skipped when the archive is not open or a cache
 * directory is already in use.
 */
void Bit7ZUnzipper::CreateBuffDir()
{
    if (!is_open_ || use_cache_dir_)
    {
        return;
    }
    boost::uuids::name_generator_latest gen{boost::uuids::ns::url()};
    auto uuid = gen(archiver_path_.c_str());
    std::filesystem::path cur_path = std::filesystem::current_path();
    std::filesystem::path cache_path = cur_path / boost::uuids::to_string(uuid);
    if (std::filesystem::exists(cache_path))
    {
        std::filesystem::remove_all(cache_path);
    }
    std::filesystem::create_directories(cache_path);
    cache_dir_ = cache_path.string();
    use_cache_dir_ = true;
}

/**
 * @brief Obtain a readable stream for one entry inside the currently opened archive.
 * @param part_file Path/name of the entry within the archive.
 * @return Shared pointer to an UnzipperStream bound to this unzipper via shared_from_this().
 *
 * Raises the on-stream event, then serves the entry from @ref memory_cache_ when it was extracted
 * before. Otherwise it locates the entry, treating an empty entry as an empty stream, and dispatches
 * by uncompressed size: entries no larger than max_mem_size_ are extracted into memory through
 * ReadFileTobuff, while larger ones are extracted to the cache directory through ReadFileToFile.
 *
 * @throws IOError if the archive has not been opened, or if @p part_file is not present in it.
 */
std::shared_ptr<UnzipperStream> Bit7ZUnzipper::GetStreamImpl(std::string_view part_file)
{
    if (!is_open_)
    {
        throw IOError(std::format("Zip file {} is not opened.", archiver_path_));
    }
    RaiseEvent(archiver_path_, part_file);
    if (memory_cache_.find(std::string{part_file}) != memory_cache_.end())
    {
        const auto& cache = memory_cache_.at(std::string{part_file});
        auto stream = UnzipperStream::MakeUnzipperStream(cache);
        stream->SetFrom(shared_from_this());
        return stream;
    }
    const auto it = archiver_->find(bit7z::tstring(part_file));
    if (it == archiver_->end())
    {
        throw IOError("File not found in zip: " + std::string(part_file));
    }
    size_t uncomp_size = it->size();
    if (uncomp_size == 0)
    {
        auto stream = std::make_shared<UnzipperStream>("");
        stream->SetFrom(shared_from_this());
        return stream;
    }
    if (uncomp_size <= max_mem_size_)
    {
        return ReadFileTobuff(it, uncomp_size, std::string{part_file});
    }
    return ReadFileToFile(it, std::string{part_file});
}

/**
 * @brief Extract the entry referenced by @p it into an in-memory buffer and cache it.
 * @param it Constant iterator addressing the target entry within the archive.
 * @param uncompsize Uncompressed size of the entry in bytes.
 * @param part_name Entry name used as the key in @ref memory_cache_.
 * @return Shared pointer to a stream backed by the freshly extracted buffer.
 */
std::shared_ptr<UnzipperStream> Bit7ZUnzipper::ReadFileTobuff(It it, size_t uncompsize, const std::string& part_name)
{
    UnzipperStream::Buffer buff(it->size());
    archiver_->extractTo(reinterpret_cast<bit7z::byte_t*>(buff.data.get()), it->size(), it->index());
    auto stream = UnzipperStream::MakeUnzipperStream(buff);
    stream->SetFrom(shared_from_this());
    memory_cache_[std::string{part_name}] = buff;
    return stream;
}

/**
 * @brief Extract the entry referenced by @p it into the temporary cache directory and cache its path.
 * @param it Constant iterator addressing the target entry within the archive.
 * @param part_name Entry name used as the key in @ref memory_cache_.
 * @return Shared pointer to a stream backed by the extracted file.
 *
 * Creates the cache directory on demand and names the output file from a UUID derived from
 * part_name, replacing any file that already exists at that location.
 */
std::shared_ptr<UnzipperStream> Bit7ZUnzipper::ReadFileToFile(It it, const std::string& part_name)
{
    if (!use_cache_dir_)
    {
        CreateBuffDir();
    }
    boost::uuids::name_generator_latest gen{boost::uuids::ns::url()};
    auto uuid = gen(part_name.c_str());
    std::filesystem::path cur_path = std::filesystem::path{cache_dir_} / boost::uuids::to_string(uuid);
    if (std::filesystem::exists(cur_path))
    {
        std::filesystem::remove_all(cur_path);
    }
    std::ofstream ofs(cur_path, std::ios_base::out | std::ios_base::binary);
    archiver_->extractTo(ofs, it->index());
    ofs.close();
    auto stream = UnzipperStream::MakeUnzipperStream(cur_path.string());
    stream->SetFrom(shared_from_this());
    memory_cache_[std::string{part_name}] = cur_path.string();
    return stream;
}
#endif  // HSBA_USE_BIT7Z
}  // namespace HsBa::Slicer