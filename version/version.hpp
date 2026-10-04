/** @file version.hpp
 * @brief Build and third-party library version information for HsBaSlicer.
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_VERSION_HPP
#define HSBA_SLICER_VERSION_HPP

#include <string_view>

#include "base/InplaceVector.hpp"

namespace HsBa::Slicer::Version
{
/**
 * @struct ThirdLibraries
 * @brief Descriptive entry for one third-party dependency (name, license and home page).
 */
struct ThirdLibraries
{
    std::string_view name;     ///< Library name.
    std::string_view license;  ///< License identifier.
    std::string_view mainPage; ///< Home page URL.
};
/**
 * @struct VersionInfo
 * @brief Aggregate describing the library version, build configuration and bundled third-party libraries.
 */
struct VersionInfo
{
    std::string_view librariesName;      ///< Library product name.
    std::string_view license;            ///< Product license identifier.
    std::string_view version;            ///< Version string.
    std::string_view buildType;          ///< Build type (Debug/Release/...).
    std::string_view buildPlatform;      ///< Platform the library was built for.
    std::string_view configureTime;      ///< CMake configure timestamp.
    std::string_view vcpkgTargetTriplet; ///< vcpkg target triplet used at build time.
    Utils::InplaceVector<ThirdLibraries, 100> thirdLibraries; ///< Inventory of third-party dependencies.
};


/// @brief Return the compiled-in version information for this build.
VersionInfo GetVersionInfo();
}  // namespace HsBa::Slicer::Version

#endif  // !HSBA_SLICER_VERSION_HPP