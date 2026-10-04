/**
 * @file bit7z_def.hpp
 * @brief Shared defaults and helpers for the bit7z-based archive backends.
 *
 * Declares the platform-dependent 7z library paths and the helpers used to detect and stage
 * compressed tar archives (.tar.gz / .tgz / .tar.xz / .txz).
 */
#pragma once
#ifndef HSBA_SLICER_BIT7Z_DEF_HPP

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <string>
#include <string_view>

#include "base/template_helper.hpp"

namespace HsBa::Slicer
{
#ifdef HSBA_USE_BIT7Z
/** @brief Default path to 7z DLL/shared library (platform-dependent). */
#if _WIN32
constexpr Utils::TemplateString HSBA_7Z_DLL = "C:/Program Files/7-Zip/7z.dll";
#elif __APPLE__
constexpr Utils::TemplateString HSBA_7Z_DLL = "/usr/local/lib/7z.dylib";
#elif __linux__
constexpr Utils::TemplateString HSBA_7Z_DLL = "/usr/lib/7z.so";
#else
constexpr Utils::TemplateString HSBA_7Z_DLL = "";
#endif

/** @brief Default path to 7za DLL/shared library (platform-dependent). */
#if _WIN32
constexpr Utils::TemplateString HSBA_7ZA_DLL = "C:/Program Files/7-Zip/7za.dll";
#elif __APPLE__
constexpr Utils::TemplateString HSBA_7ZA_DLL = "/usr/local/lib/7za.dylib";
#elif __linux__
constexpr Utils::TemplateString HSBA_7ZA_DLL = "/usr/lib/7za.so";
#else
constexpr Utils::TemplateString HSBA_7ZA_DLL = "";
#endif

#endif  // HSBA_USE_BIT7Z

/**
 * @brief Check whether the given path refers to a compressed tar archive
 *        (.tar.gz / .tgz / .tar.xz / .txz).
 *
 * Such archives are handled as a tar archive nested inside a compression layer,
 * which 7z does not unpack in one step unless opened through the nested subfile.
 */
inline bool IsCompressedTarPath(std::string_view path)
{
    std::string lower{path};
    std::ranges::transform(lower, lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lower.ends_with(".tar.gz") || lower.ends_with(".tgz") || lower.ends_with(".tar.xz") ||
           lower.ends_with(".txz");
}

/**
 * @brief Generate a unique temporary file path for the inner tar of a compressed tar archive.
 *
 * The path embeds a high-resolution timestamp to avoid collisions between concurrent readers.
 */
inline std::filesystem::path MakeInnerTarTempPath(std::string_view archive_path)
{
    const std::filesystem::path archive{std::string{archive_path}};
    auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() / (archive.stem().string() + '.' + std::to_string(now) + ".tar");
}
}  // namespace HsBa::Slicer

#endif  // !HSBA_SLICER_BIT7Z_DEF_HPP
