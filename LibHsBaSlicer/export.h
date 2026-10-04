/** @file export.h
 * @brief Defines the HSBA_SLICER_LIB_API export/import macro for the HsBaSlicer library C++ API.
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_LIB_EXPORT_H
#define HSBA_SLICER_LIB_EXPORT_H

#ifdef _WIN32
#ifdef HSBA_SLICER_EXPORTS
/** @brief On Windows: mark symbols for export when building the library. */
#define HSBA_SLICER_LIB_API __declspec(dllexport)
#else
/** @brief On Windows: mark symbols for import when consuming the library. */
#define HSBA_SLICER_LIB_API __declspec(dllimport)
#endif
#else
/** @brief On non-Windows platforms: no export/import decoration needed. */
#define HSBA_SLICER_LIB_API
#endif

#endif  // !HSBA_SLICER_LIB_EXPORT_H