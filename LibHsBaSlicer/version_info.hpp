#pragma once
#ifndef HSBA_SLICER_LIB_VERSION_INFO_HPP
#define HSBA_SLICER_LIB_VERSION_INFO_HPP

#include <string>

#include "export.h"

namespace HsBa::Slicer
{
/**
 * @brief Get version information as a pretty-printed JSON string.
 * @return UTF-8 encoded JSON string containing project version info.
 */
HSBA_SLICER_LIB_API std::string GetVersionJson();

/**
 * @brief Get version information as an XML string.
 * @return UTF-8 encoded XML string containing project version info.
 */
HSBA_SLICER_LIB_API std::string GetVersionXml();

}  // namespace HsBa::Slicer

#endif  // !HSBA_SLICER_LIB_VERSION_INFO_HPP
