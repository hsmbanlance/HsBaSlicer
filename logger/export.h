#pragma once
#ifndef HSBA_SLICER_LOG_EXPORT_H
#define HSBA_SLICER_LOG_EXPORT_H

#ifdef _WIN32
  #ifdef HSBA_SLICER_LOG_EXPORTS
    #define HSBA_SLICER_LOG_API __declspec(dllexport)
  #else
    #define HSBA_SLICER_LOG_API __declspec(dllimport)
  #endif
#else
  #define HSBA_SLICER_LOG_API
#endif

#endif // !HSBA_SLICER_LOG_EXPORT_H