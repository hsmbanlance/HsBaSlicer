/** @file package_reader.hpp
 * @brief Read-only access to the entries of a result package (zip archive) produced by a pipeline.
 *
 * The REST layer serves two things from a package: the entry listing (names and sizes, so a client
 * can pick an image) and one entry's bytes. Both are read straight from the archive with the small
 * miniz reader the fileoperator library already links, instead of growing the CRTP IUnzipper
 * interface with a listing operation a single read-only endpoint would not justify.
 *
 * No function here touches HTTP and none echoes the archive path back: a caller that reports a
 * failure to a remote client quotes only the entry name it was asked for.
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_RESTAPI_PACKAGE_READER_HPP
#define HSBA_SLICER_RESTAPI_PACKAGE_READER_HPP

#include <string>
#include <string_view>
#include <vector>

namespace HsBa::Slicer::RestApi
{
/** @brief One file inside a result package, as the listing endpoint reports it. */
struct PackageEntryInfo
{
    std::string path;                 ///< Name inside the archive, '/' separated, exactly as stored.
    long long size = 0;               ///< Bytes the entry occupies inside the archive.
    long long uncompressed_size = 0;  ///< Bytes the entry expands to.
};

/** @brief Largest entry this server is willing to hand out; anything bigger is refused, not streamed. */
inline constexpr long long PACKAGE_ENTRY_MAX_BYTES = 32LL * 1024 * 1024;

/**
 * @brief Error text ReadPackageEntry sets when the archive simply has no such entry.
 *
 * Exported because the route layer maps it to a 404 while every other failure of the same call
 * means the archive itself is unusable; the two must not share one status code.
 */
inline constexpr std::string_view PACKAGE_ENTRY_NOT_FOUND = "package entry not found";

/** @brief Error text ReadPackageEntry sets when the entry is bigger than PACKAGE_ENTRY_MAX_BYTES. */
inline constexpr std::string_view PACKAGE_ENTRY_TOO_LARGE = "package entry too large";

/**
 * @brief Whether @p entry can be used against an archive without a traversal risk.
 *
 * Accepts 1 to 512 characters, rejects anything containing "..", a backslash, a control character,
 * or a leading '/'. An inner '/' is fine: packages store layer files in folders, and the route
 * pattern captures the whole tail of the URL as the entry name.
 */
bool IsSafeEntryName(std::string_view entry);

/** @brief Content type the entry name suggests, octet-stream for anything unrecognized. */
std::string PackageMediaType(std::string_view entry);

/**
 * @brief Enumerate the files of the archive at @p archivePath.
 * @return false with a reason in @p error when the file is missing, not a zip, or unreadable.
 *
 * Directory entries are skipped: a package is a flat set of files for this server, and an entry
 * count of directories would only confuse a client picking an image.
 */
bool ReadPackageEntries(const std::string& archivePath, std::vector<PackageEntryInfo>& out, std::string& error);

/**
 * @brief Extract one entry of the archive at @p archivePath into @p data.
 * @return false with a reason in @p error; the reason is one of the two exported texts for the two
 *         client-visible cases (missing entry, oversized entry).
 *
 * The size cap is checked against the archive's own record before any bytes are allocated, so a
 * hostile package cannot make this process expand a stub into gigabytes.
 */
bool ReadPackageEntry(const std::string& archivePath, const std::string& entry, std::string& data, std::string& error);
}  // namespace HsBa::Slicer::RestApi

#endif  // HSBA_SLICER_RESTAPI_PACKAGE_READER_HPP
