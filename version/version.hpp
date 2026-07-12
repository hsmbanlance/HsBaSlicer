#pragma once
#ifndef HSBA_SLICER_VERSION_HPP
#define HSBA_SLICER_VERSION_HPP

#include <string_view>

#include "base/InplaceVector.hpp"

namespace HsBa::Slicer::Version
{
struct ThirdLibraries
{
    std::string_view name;
    std::string_view license;
    std::string_view mainPage;
};
struct VersionInfo
{
    std::string_view librariesName;
    std::string_view license;
    std::string_view version;
    std::string_view buildType;
    std::string_view buildPlatform;
    std::string_view configureTime;
    std::string_view vcpkgTargetTriplet;
    Utils::InplaceVector<ThirdLibraries, 100> thirdLibraries;
};


VersionInfo GetVersionInfo();
}  // namespace HsBa::Slicer::Version

#endif  // !HSBA_SLICER_VERSION_HPP