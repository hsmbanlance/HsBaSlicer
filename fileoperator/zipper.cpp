/**
 * @file zipper.cpp
 * @brief Implements the miniz-based ZIP compressor (@ref Zipper) and the standalone extraction helpers.
 */
#include "zipper.hpp"

#include <filesystem>
#include <type_traits>

#include "base/encoding_convert.hpp"
#include "base/error.hpp"
#include "base/template_helper.hpp"

namespace HsBa::Slicer
{
/**
 * @brief Construct a Zipper selecting the miniz compression level.
 * @param compression Compression level to map onto the miniz backend.
 * @throws NotSupportedError if @p compression is Undefine or Unknown.
 */
Zipper::Zipper(MinizCompression compression)
{
    if (compression == MinizCompression::Undefine || compression == MinizCompression::Unknown)
    {
        throw NotSupportedError("Unknow or Undefine miniz compression");
    }
    switch (compression)
    {
    case MinizCompression::No:
        compression_ = MZ_NO_COMPRESSION;
        break;
    case MinizCompression::Fast:
        compression_ = MZ_BEST_SPEED;
        break;
    case MinizCompression::Tight:
        compression_ = MZ_BEST_COMPRESSION;
        break;
    default:
        throw NotSupportedError("Unknow or Undefine miniz compression");
        break;
    }
}

/**
 * @brief Stage an in-memory file for later compression under the given archive name.
 * @param name Name of the file within the archive (UTF-8, converted to the local encoding).
 * @param data File content.
 * @throws InvalidArgumentError if @p name is already staged.
 */
void Zipper::AddByteFile(std::string_view name, const std::string& data)
{
    std::string ansi_name = utf8_to_local(std::string{name});
    auto [add, add_res] = byteFilesWaitCompress_.emplace(ansi_name, Bytes{data});
    if (!add_res)
    {
        throw InvalidArgumentError("Duplicate name files");
    }
}
/**
 * @brief Stage an on-disk file for later compression under the given archive name.
 * @param name Name of the file within the archive (UTF-8, converted to the local encoding).
 * @param path Path to the source file on disk.
 * @throws InvalidArgumentError if @p name is already staged.
 */
void Zipper::AddFile(std::string_view name, std::string_view path)
{
    std::string ansi_name = utf8_to_local(std::string{name});
    auto [add, add_res] = byteFilesWaitCompress_.emplace(ansi_name, std::string(path));
    if (!add_res)
    {
        throw InvalidArgumentError("Duplicate name files");
    }
}

/**
 * @brief Stage an in-memory file, appending "_duplicate" to the name when it collides with an entry.
 * @param name Desired archive entry name (UTF-8, converted to the local encoding).
 * @param data File content.
 */
void Zipper::AddByteFileIgnoreDuplicate(std::string_view name, const std::string& data)
{
    std::string ansi_name = utf8_to_local(std::string{name});
    if (byteFilesWaitCompress_.count(std::string{ansi_name}))
    {
        byteFilesWaitCompress_.emplace((std::string(ansi_name) + duplicate_addition), Bytes{data});
    }
    else
    {
        byteFilesWaitCompress_.emplace(ansi_name, Bytes{data});
    }
}
/**
 * @brief Stage an on-disk file, appending "_duplicate" to the name when it collides with an entry.
 * @param name Desired archive entry name (UTF-8, converted to the local encoding).
 * @param path Path to the source file on disk.
 */
void Zipper::AddFileIgnoreDuplicate(std::string_view name, std::string_view path)
{
    std::string ansi_name = utf8_to_local(std::string{name});
    if (byteFilesWaitCompress_.count(std::string{ansi_name}))
    {
        byteFilesWaitCompress_.emplace((std::string(ansi_name) + duplicate_addition), std::string(path));
    }
    else
    {
        byteFilesWaitCompress_.emplace(ansi_name, std::string(path));
    }
}
/**
 * @brief Write all staged entries into a ZIP archive on disk and report progress per entry.
 * @param filePath Output archive path (UTF-8, converted to the local encoding).
 * @throws IOError if the archive cannot be finalized or no entries could be added.
 */
void Zipper::Save(std::string_view filePath)
{
    std::string path = std::filesystem::path(filePath).make_preferred().string();
    path = utf8_to_local(path);
    mz_zip_archive archiver{};
    mz_zip_zero_struct(&archiver);
    mz_bool status = mz_zip_writer_init_file(&archiver, path.c_str(), 0);
    mz_bool add_files_status = AddAllToZip(archiver);
    status = mz_zip_writer_finalize_archive(&archiver);
    if (status <= MZ_OK && add_files_status <= MZ_OK)
    {
        mz_zip_writer_end(&archiver);
        throw IOError("Failed to save zip file");
    }
    mz_zip_writer_end(&archiver);
}


/**
 * @brief Add every staged entry to @p archiver, raising the progress event after each one.
 * @param archiver Initialized miniz writer archive to append entries to.
 * @return miniz status code (MZ_OK on success, otherwise the first failing status).
 */
mz_bool Zipper::AddAllToZip(mz_zip_archive& archiver)
{
    mz_bool status = MZ_OK;
    size_t fileCount = byteFilesWaitCompress_.size();
    size_t currentFileIndex = 0;
    for (const auto& [name, bytes] : byteFilesWaitCompress_)
    {
        status = std::visit(Utils::Overloaded{[&archiver, &name, this](const std::string& arg) -> mz_bool
                                              { return ZipAddFile(archiver, name, arg); },
                                              [&archiver, &name, this](const Bytes& arg) -> mz_bool
                                              { return ZipAddMember(archiver, name, arg); }},
                            bytes);
        if (status <= MZ_OK)
        {
            return status;
        }
        ++currentFileIndex;
        double progress = static_cast<double>(currentFileIndex) / fileCount;
        RaiseEvent(progress, name);
    }
    return status;
}

/**
 * @brief Add a single on-disk file referenced by @p path to @p archiver under the name @p name.
 */
mz_bool Zipper::ZipAddFile(mz_zip_archive& archiver, const std::string& name, const std::string& path) const
{
    return mz_zip_writer_add_file(&archiver, name.c_str(), path.c_str(), NULL, 0, compression_);
}

/**
 * @brief Add an in-memory byte buffer to @p archiver under the name @p name.
 */
mz_bool Zipper::ZipAddMember(mz_zip_archive& archiver, const std::string& name, const Bytes& bytes) const
{
    return mz_zip_writer_add_mem(&archiver, name.c_str(), bytes.data.data(), bytes.data.size(), compression_);
}

/**
 * @brief Extract every entry of a ZIP archive into the @p output_path directory.
 * @param archive_path Path to the ZIP archive to read.
 * @param output_path Destination directory (created together with parent folders as needed).
 * @throws IOError if the archive cannot be opened, a file stat cannot be read, or extraction fails.
 */
void MiniZExtractFile(std::string_view archive_path, std::string_view output_path)
{
    mz_zip_archive archiver{};
    mz_zip_zero_struct(&archiver);
    if (!mz_zip_reader_init_file(&archiver, archive_path.data(), 0))
    {
        throw IOError("Failed to open zip file");
    }
    std::filesystem::path outputDir(output_path);
    if (!std::filesystem::exists(outputDir))
    {
        std::filesystem::create_directories(outputDir);
    }
    mz_uint file_count = mz_zip_reader_get_num_files(&archiver);
    for (mz_uint i = 0; i != file_count; ++i)
    {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(&archiver, i, &file_stat))
        {
            mz_zip_reader_end(&archiver);
            throw IOError("Failed to get file stat from zip");
        }
        std::string full_output_path = (outputDir / file_stat.m_filename).string();
        std::filesystem::path full_output_path_obj(full_output_path);

        if (file_stat.m_is_directory)
        {
            if (!std::filesystem::exists(full_output_path_obj))
            {
                std::filesystem::create_directories(full_output_path_obj);
            }
        }
        else
        {
            std::filesystem::path output_dir = full_output_path_obj.parent_path();
            if (!std::filesystem::exists(output_dir))
            {
                std::filesystem::create_directories(output_dir);
            }

            if (!mz_zip_reader_extract_to_file(&archiver, i, full_output_path.c_str(), 0))
            {
                mz_zip_reader_end(&archiver);
                throw IOError("Failed to extract file from zip");
            }
        }
    }
    mz_zip_reader_end(&archiver);
}

/**
 * @brief Extract every entry of a ZIP archive into memory as filename-to-content pairs.
 * @param archive_path Path to the ZIP archive to read.
 * @return Map from archive entry name to its uncompressed content.
 * @throws IOError if the archive cannot be opened, a file stat cannot be read, or extraction fails.
 */
std::unordered_map<std::string, std::string> MiniZExtractFileToBuffer(std::string_view archive_path)
{
    mz_zip_archive archiver{};
    mz_zip_zero_struct(&archiver);
    if (!mz_zip_reader_init_file(&archiver, archive_path.data(), 0))
    {
        throw IOError("Failed to open zip file");
    }
    std::unordered_map<std::string, std::string> result;
    mz_uint file_count = mz_zip_reader_get_num_files(&archiver);
    for (mz_uint i = 0; i != file_count; ++i)
    {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(&archiver, i, &file_stat))
        {
            mz_zip_reader_end(&archiver);
            throw IOError("Failed to get file stat from zip");
        }
        std::string content(file_stat.m_uncomp_size, '\0');
        if (!mz_zip_reader_extract_to_mem(&archiver, i, content.data(), content.size(), 0))
        {
            mz_zip_reader_end(&archiver);
            throw IOError("Failed to extract file from zip");
        }
        result[file_stat.m_filename] = std::move(content);
    }
    mz_zip_reader_end(&archiver);
    return result;
}
}  // namespace HsBa::Slicer
