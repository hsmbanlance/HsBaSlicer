/**
 * @file unzipper.cpp
 * @brief Implements the miniz-based ZIP extractor (@ref Unzipper).
 */
#include "unzipper.hpp"

#include <filesystem>
#include <format>

#include <boost/uuid.hpp>

namespace HsBa::Slicer
{
/**
 * @brief Destroys the unzipper and releases every owned resource.
 *
 * Ends the miniz reader when an archive is still open and removes the temporary cache directory
 * if one had been created for large entries.
 */
Unzipper::~Unzipper()
{
    if (is_open_)
    {
        mz_zip_reader_end(&zip_archive_);
        is_open_ = false;
    }
    if (use_cache_dir_)
    {
        if (std::filesystem::exists(cache_dir_))
        {
            std::filesystem::remove_all(cache_dir_);
        }
    }
}

/**
 * @brief Open the ZIP archive located at @p path so its entries can later be streamed.
 * @param path Path to the archive file to open.
 * @param reopen When true the archive is re-opened even if @p path is already open; when false and
 *               the same path is already open the call returns immediately as a no-op.
 *
 * (Re-)opening clears the previous memory and cache-directory caches so stale entries are never
 * served again.
 *
 * @throws IOError if miniz cannot open the archive file.
 */
void Unzipper::ReadFromFileImpl(std::string_view path, bool reopen)
{
    if (is_open_)
    {
        if (path == archiver_path_ && !reopen)
        {
            return;
        }
        mz_zip_reader_end(&zip_archive_);
        is_open_ = false;
    }
    memset(&zip_archive_, 0, sizeof(zip_archive_));
    if (!mz_zip_reader_init_file(&zip_archive_, path.data(), 0))
    {
        throw IOError("Failed to open zip file: " + std::string(path));
    }
    archiver_path_ = path;
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
 * @brief Obtain a readable stream for one entry inside the currently opened archive.
 * @param part_file Path/name of the entry within the archive.
 * @return Shared pointer to an UnzipperStream bound to this unzipper via shared_from_this().
 *
 * Raises the on-stream event, then serves the entry from @ref memory_cache_ when it was extracted
 * before. Otherwise it locates the entry, treats an empty entry as an empty stream, and dispatches
 * by uncompressed size: entries no larger than max_mem_size_ go through ReadFileTobuff, larger ones
 * through ReadFileToFile.
 *
 * @throws IOError if the archive is not open, or the entry is not present in it.
 */
std::shared_ptr<UnzipperStream> Unzipper::GetStreamImpl(std::string_view part_file)
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
    int file_index = mz_zip_reader_locate_file(&zip_archive_, part_file.data(), nullptr, 0);
    if (file_index < 0)
    {
        throw IOError("File not found in zip: " + std::string(part_file));
    }
    size_t uncomp_size = 0;
    mz_zip_archive_file_stat file_stat;
    mz_zip_reader_file_stat(&zip_archive_, file_index, &file_stat);
    uncomp_size = static_cast<size_t>(file_stat.m_uncomp_size);
    if (uncomp_size == 0)
    {
        auto stream = std::make_shared<UnzipperStream>("");
        stream->SetFrom(shared_from_this());
        return stream;
    }
    if (uncomp_size <= max_mem_size_)
    {
        return ReadFileTobuff(file_index, uncomp_size, std::string{part_file});
    }
    return ReadFileToFile(file_index, std::string{part_file});
}
/**
 * @brief Extract the entry at @p file_index into an in-memory buffer and cache it.
 * @param file_index Index of the target entry within the archive.
 * @param uncomp_size Uncompressed size of the entry in bytes.
 * @param part_name Entry name used as the cache key.
 * @return Shared pointer to a stream backed by the freshly extracted buffer.
 * @throws IOError if miniz cannot extract the entry into the buffer.
 */
std::shared_ptr<UnzipperStream> Unzipper::ReadFileTobuff(int file_index, size_t uncomp_size,
                                                         const std::string& part_name)
{
    UnzipperStream::Buffer buff(uncomp_size);
    auto user_read_buff = std::make_unique<char[]>(user_buff_size);
    if (!mz_zip_reader_extract_to_mem_no_alloc(&zip_archive_, file_index, buff.data.get(), uncomp_size, 0,
                                               user_read_buff.get(), user_buff_size))
    {
        throw IOError(std::format("Failed to extract file {}", part_name));
    }
    auto stream = UnzipperStream::MakeUnzipperStream(buff);
    stream->SetFrom(shared_from_this());
    memory_cache_[std::string{part_name}] = buff;
    return stream;
}

/**
 * @brief Create a deterministic temporary cache directory for large extracted entries.
 *
 * The directory lives under the current working path and is named from a UUID derived from the
 * archive path, so the same archive always maps to the same cache folder (any pre-existing folder
 * with that name is removed first). No-op when the archive is not open or a cache directory already
 * exists.
 */
void Unzipper::CreateBuffDir()
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
 * @brief Extract the entry at @p file_index into the cache directory and cache its path.
 * @param file_index Index of the target entry within the archive.
 * @param part_name Entry name used as the cache key.
 * @return Shared pointer to a stream backed by the extracted file.
 *
 * Creates the cache directory on demand and names the output file from a UUID derived from
 * part_name, replacing any file already present at that location.
 * @throws IOError if miniz cannot extract the entry to the file.
 */
std::shared_ptr<UnzipperStream> Unzipper::ReadFileToFile(int file_index, const std::string& part_name)
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
    if (!mz_zip_reader_extract_to_file(&zip_archive_, file_index, cur_path.string().c_str(), 0))
    {
        throw IOError(std::format("Failed to extract file {}", part_name));
    }
    auto stream = UnzipperStream::MakeUnzipperStream(cur_path.string());
    stream->SetFrom(shared_from_this());
    memory_cache_[std::string{part_name}] = cur_path.string();
    return stream;
}
}  // namespace HsBa::Slicer