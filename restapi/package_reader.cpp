/** @file package_reader.cpp
 * @brief miniz-backed reader for the entries of a result package.
 *
 * Only the read side of miniz is used, and only the four calls a listing plus one extraction need.
 * Every failure leaves a reason string that names nothing but the entry it was asked about, so the
 * route layer can answer a remote client without disclosing a server-side directory.
 * @author HsBa
 */
#include "package_reader.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>

#include <miniz.h>

namespace HsBa::Slicer::RestApi
{
namespace
{
/**
 * @brief A zip archive opened for reading and closed on every exit path.
 *
 * The readers below have half a dozen early returns; keeping the end call in a destructor is what
 * stops a failure branch from leaking the file handle the archive holds open.
 */
class ScopedArchive
{
public:
    ScopedArchive() = default;
    ScopedArchive(const ScopedArchive&) = delete;
    ScopedArchive& operator=(const ScopedArchive&) = delete;
    ScopedArchive(ScopedArchive&&) = delete;
    ScopedArchive& operator=(ScopedArchive&&) = delete;

    ~ScopedArchive()
    {
        if (open_)
        {
            mz_zip_reader_end(&archive_);
        }
    }

    /** @brief Open the archive at @p path; false when it is missing, not a zip, or unreadable. */
    bool Open(const std::string& path)
    {
        open_ = mz_zip_reader_init_file(&archive_, path.c_str(), 0) != MZ_FALSE;
        return open_;
    }

    /** @brief The underlying archive handle; only valid once Open returned true.
     *
     * Handed out as a pointer because every miniz reader call takes one, and a reference here would
     * only push an address-of onto each call site.
     */
    mz_zip_archive* Get() { return &archive_; }

    /** @brief Records the central directory holds, directory entries included. */
    mz_uint Count() { return static_cast<mz_uint>(mz_zip_reader_get_num_files(&archive_)); }

private:
    mz_zip_archive archive_{};  ///< Zero-initialised by the member initializer, as miniz requires.
    bool open_ = false;         ///< Whether the handle is ours to close.
};

/** @brief Release whatever extract_to_heap allocated, on every path out of the calling scope. */
class ScopedHeap
{
public:
    explicit ScopedHeap(void* block) : block_(block) {}
    ScopedHeap(const ScopedHeap&) = delete;
    ScopedHeap& operator=(const ScopedHeap&) = delete;
    ~ScopedHeap()
    {
        if (block_)
        {
            mz_free(block_);
        }
    }

private:
    void* block_ = nullptr;
};

/**
 * @brief Lower-cased extension of an entry name without the dot; empty when there is none.
 *
 * The last slash is honoured because packages store their files in folders, and a directory called
 * "layers.png" must not decide the content type of "layers/layer_1".
 */
std::string LowerExtension(std::string_view entry)
{
    const std::size_t slash = entry.find_last_of("/");
    const std::string_view name = (slash == std::string_view::npos) ? entry : entry.substr(slash + 1);
    const std::size_t dot = name.find_last_of(".");
    if ((dot == std::string_view::npos) || (dot + 1 >= name.size()))
    {
        return {};
    }
    std::string ext(name.substr(dot + 1));
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}
}  // namespace

bool IsSafeEntryName(std::string_view entry)
{
    if (entry.empty() || (entry.size() > 512))
    {
        return false;
    }
    if (entry.front() == '/')
    {
        return false;
    }
    for (const char c : entry)
    {
        const unsigned char code = static_cast<unsigned char>(c);
        // A backslash is a path separator on the platform the server most often runs on, and a
        // control character can hide a trailing dot or a CR/LF from whatever consumes the name.
        if ((code < 0x20U) || (code == 0x7FU) || (c == '\\'))
        {
            return false;
        }
    }
    return entry.find("..") == std::string_view::npos;
}

std::string PackageMediaType(std::string_view entry)
{
    const std::string ext = LowerExtension(entry);
    if (ext == "png")
    {
        return "image/png";
    }
    if ((ext == "jpg") || (ext == "jpeg"))
    {
        return "image/jpeg";
    }
    if (ext == "svg")
    {
        return "image/svg+xml";
    }
    if (ext == "json")
    {
        return "application/json";
    }
    if ((ext == "txt") || (ext == "gcode") || (ext == "nc"))
    {
        return "text/plain; charset=utf-8";
    }
    return "application/octet-stream";
}

bool ReadPackageEntries(const std::string& archivePath, std::vector<PackageEntryInfo>& out, std::string& error)
{
    out.clear();
    ScopedArchive archive;
    if (!archive.Open(archivePath))
    {
        error = "result file is not a readable zip archive";
        return false;
    }
    const mz_uint count = archive.Count();
    out.reserve(count);
    for (mz_uint i = 0; i < count; ++i)
    {
        mz_zip_archive_file_stat stat{};
        if (!mz_zip_reader_file_stat(archive.Get(), i, &stat))
        {
            out.clear();
            error = "package entry record is unreadable";
            return false;
        }
        if (mz_zip_reader_is_file_a_directory(archive.Get(), i))
        {
            continue;
        }
        PackageEntryInfo info;
        info.path = stat.m_filename;
        info.size = static_cast<long long>(stat.m_comp_size);
        info.uncompressed_size = static_cast<long long>(stat.m_uncomp_size);
        out.push_back(std::move(info));
    }
    return true;
}

bool ReadPackageEntry(const std::string& archivePath, const std::string& entry, std::string& data, std::string& error)
{
    data.clear();
    if (!IsSafeEntryName(entry))
    {
        // Unreachable through the route layer, which answers 400 before getting here; kept so a
        // direct caller cannot turn this function into an arbitrary path probe.
        error = "package entry name is not usable";
        return false;
    }
    ScopedArchive archive;
    if (!archive.Open(archivePath))
    {
        error = "result file is not a readable zip archive";
        return false;
    }
    // Flag 0 searches case-insensitively, which is what the unzipper of this project also does, so
    // an entry a client copied from the listing always resolves the same way.
    const int index = mz_zip_reader_locate_file(archive.Get(), entry.c_str(), nullptr, 0);
    if (index < 0)
    {
        error = PACKAGE_ENTRY_NOT_FOUND;
        return false;
    }
    mz_zip_archive_file_stat stat{};
    if (!mz_zip_reader_file_stat(archive.Get(), static_cast<mz_uint>(index), &stat))
    {
        error = "package entry record is unreadable";
        return false;
    }
    if (static_cast<long long>(stat.m_uncomp_size) > PACKAGE_ENTRY_MAX_BYTES)
    {
        error = PACKAGE_ENTRY_TOO_LARGE;
        return false;
    }
    std::size_t expanded = 0;
    void* block = mz_zip_reader_extract_to_heap(archive.Get(), static_cast<mz_uint>(index), &expanded, 0);
    if (!block)
    {
        error = "package entry could not be expanded";
        return false;
    }
    const ScopedHeap guard(block);
    data.assign(static_cast<const char*>(block), expanded);
    return true;
}
}  // namespace HsBa::Slicer::RestApi
