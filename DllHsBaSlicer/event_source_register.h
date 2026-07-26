#pragma once
#ifndef HSBA_SLICER_EVENT_SOURCE_REGISTER_H
#define HSBA_SLICER_EVENT_SOURCE_REGISTER_H

#include "dllexport.h"

#ifdef __cplusplus
extern "C"
{
#endif  // __cplusplus
    HSBA_SLICER_API void HsBaAddZipperEventCallback(const char* event_name, void (*func)(double, const char*));

    HSBA_SLICER_API void HsBaAddDBEventCallback(const char* event_name, void (*func)(const char*, const char*));
#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // !HSBA_SLICER_EVENT_SOURCE_REGISTER_H