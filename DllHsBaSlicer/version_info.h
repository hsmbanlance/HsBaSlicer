#pragma once
#ifndef HSBA_SLICER_VERSION_INFO_H
#define HSBA_SLICER_VERSION_INFO_H

#include "dllexport.h"

#ifdef __cplusplus
extern "C"
{
#endif  // __cplusplus

    /**
     * @brief Get version information as a JSON string.
     *
     * The returned string is dynamically allocated and must be freed
     * by calling HsBaFreeVersionString.
     *
     * @return UTF-8 encoded JSON string containing version information.
     */
    HSBA_SLICER_API char* HsBaGetVersionJson(void);

    /**
     * @brief Get version information as an XML string.
     *
     * The returned string is dynamically allocated and must be freed
     * by calling HsBaFreeVersionString.
     *
     * @return UTF-8 encoded XML string containing version information.
     */
    HSBA_SLICER_API char* HsBaGetVersionXml(void);

    /**
     * @brief Free a string returned by HsBaGetVersionJson or HsBaGetVersionXml.
     *
     * @param str The string to free.
     */
    HSBA_SLICER_API void HsBaFreeVersionString(char* str);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // !HSBA_SLICER_VERSION_INFO_H
