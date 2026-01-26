#pragma once
#ifndef HSBA_SLICER_BIT7Z_DEF_HPP

#include "base/template_helper.hpp"

namespace HsBa::Slicer 
{
#ifdef HSBA_USE_BIT7Z
#if _WIN32
    constexpr Utils::TemplateString HSBA_7Z_DLL = "C:/Program Files/7-Zip/7z.dll";
#elif __APPLE__
    constexpr Utils::TemplateString HSBA_7Z_DLL = "/usr/local/lib/7z.dylib";
#elif __linux__
    constexpr Utils::TemplateString HSBA_7Z_DLL = "/usr/lib/7z.so";
#else
    constexpr Utils::TemplateString HSBA_7Z_DLL = "";
#endif

#if _WIN32
    constexpr Utils::TemplateString HSBA_7ZA_DLL = "C:/Program Files/7-Zip/7za.dll";
#elif __APPLE__
    constexpr Utils::TemplateString HSBA_7ZA_DLL = "/usr/local/lib/7za.dylib";
#elif __linux__
    constexpr Utils::TemplateString HSBA_7ZA_DLL = "/usr/lib/7za.so";
#else
    constexpr Utils::TemplateString HSBA_7ZA_DLL = "";
#endif

#endif // HSBA_USE_BIT7Z
} // namespace HsBa::Slicer

#endif // !HSBA_SLICER_BIT7Z_DEF_HPP
