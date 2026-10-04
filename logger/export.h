/** @file export.h
 * @brief Defines the HSBA_SLICER_LOG_API import/export macro for the logger library.
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_LOG_EXPORT_H
#define HSBA_SLICER_LOG_EXPORT_H

/// @brief Symbol visibility macro: dllexport when building the logger, dllimport when consuming it (Windows).
#ifdef _WIN32
#ifdef HSBA_SLICER_LOG_EXPORTS
#define HSBA_SLICER_LOG_API __declspec(dllexport)
#else
#define HSBA_SLICER_LOG_API __declspec(dllimport)
#endif
#else
#define HSBA_SLICER_LOG_API
#endif

#endif  // !HSBA_SLICER_LOG_EXPORT_H