#pragma once
#ifndef HSBA_SLICER_BIT7Z_DEF_HPP

namespace HsBa::Slicer 
{
#ifdef USE_BIT7Z
#if _WIN32
    const std::string HSBA_7Z_DLL = "C:/Program Files/7-Zip/7z.dll";
#elif __APPLE__
    const std::string HSBA_7Z_DLL = "/usr/local/lib/7z.dylib";
#elif __linux__
    const std::string HSBA_7Z_DLL = "/usr/lib/7z.so";
#else
    const std::string HSBA_7Z_DLL = "";
#endif

#if _WIN32
    const std::string HSBA_7ZA_DLL = "C:/Program Files/7-Zip/7za.dll";
#elif __APPLE__
    const std::string HSBA_7ZA_DLL = "/usr/local/lib/7za.dylib";
#elif __linux__
    const std::string HSBA_7ZA_DLL = "/usr/lib/7za.so";
#else
    const std::string HSBA_7ZA_DLL = "";
#endif

#endif // USE_BIT7Z
} // namespace HsBa::Slicer

#endif // !HSBA_SLICER_BIT7Z_DEF_HPP
