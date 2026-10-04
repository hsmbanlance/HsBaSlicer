/** @file dllexport.h
 * @brief Defines the HSBA_SLICER_API export/import macro for the HsBaSlicer shared library C ABI.
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_DLLEXPORT_H
#define HSBA_SLICER_DLLEXPORT_H

#ifdef _WIN32
#ifdef HSBA_SLICER_EXPORTS
/** @brief On Windows: mark symbols for export when building the DLL. */
#define HSBA_SLICER_API __declspec(dllexport)
#else
/** @brief On Windows: mark symbols for import when consuming the DLL. */
#define HSBA_SLICER_API __declspec(dllimport)
#endif
#else
/** @brief On non-Windows platforms: default visibility, no annotation required. */
#define HSBA_SLICER_API
#endif

#endif  // !HSBA_SLICER_DLLEXPORT_H
