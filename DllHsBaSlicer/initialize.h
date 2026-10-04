/** @file initialize.h
 * @brief Declares the C entry point that initializes the HsBaSlicer DLL (registration and one-time setup).
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_INITIALIZE_H
#define HSBA_SLICER_INITIALIZE_H

#include "dllexport.h"

#if __cplusplus
extern "C"
#endif  // __cplusplus
    /**
     * @brief Initialize the DLL: register Lua functions, pipelines and any one-time runtime setup.
     *
     * Safe to call once at load time; repeated calls have no additional effect.
     */
    HSBA_SLICER_API void initialize();

#endif  // !HSBA_SLICER_INITIALIZE_H
