#pragma once
#ifndef HSBA_SLICER_BIT7Z_ZIPPER_HPP
#define HSBA_SLICER_BIT7Z_ZIPPER_HPP

#include <vector>
#include <string>
#include <string_view>
#include <map>
#include <variant>

#ifdef USE_BIT7Z
#include <bit7z/bit7z.hpp>
#endif // USE_BIT7Z

#include "IZipper.hpp"

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
    void Bit7zExtract(const std::string& archive, const std::string& outdir, const std::string& password = "",
        const std::string& dll_path = HSBA_7Z_DLL);
    void Bit7zExtract(const std::string& archive,
        /*out*/std::map<std::string, std::vector<std::byte>>& bufs, const std::string& password = "",
        const std::string& dll_path = HSBA_7Z_DLL);

    enum class ZipperFormat
    {
        Undefine,
        // support all

        Zip,
        SevenZip,
        XZ,
        BZIP2,
        GZIP,
        TAR,
        // only support extract
        RAR,
        ISO,
        Z,
        Unknown
    };

    class Bit7zZipper final : public IZipper
    {
    public:
        Bit7zZipper() :format_{ ZipperFormat::SevenZip }, dll_path_{HSBA_7Z_DLL} {}
        Bit7zZipper(std::string_view dll_path,ZipperFormat format,std::string_view password)
            :dll_path_{dll_path},format_{format},password_{password}{}
        void AddByteFile(std::string_view name, const std::vector<std::byte>& data);
        void AddByteFile(std::string_view name, const std::string& data) override;
        void AddFile(std::string_view name, std::string_view path) override;
        //To add duplicate file, filename add "_duplicate"
        void AddByteFileIgnoreDuplicate(std::string_view name, const std::vector<std::byte>& data);
        void AddByteFileIgnoreDuplicate(std::string_view name, const std::string& data) override;
        void AddFileIgnoreDuplicate(std::string_view name, std::string_view path) override;
        void Save(std::string_view filePath) override;
    private:
        using Bytes = std::vector<std::byte>;
        using BytesFileName = std::variant<Bytes, std::string>;
        using ByteFiles = std::map<std::string, BytesFileName>;
        ByteFiles byteFilesWaitCompress_;
        const std::string duplicate_addition = "_duplicate";
        std::string dll_path_;
        ZipperFormat format_;
        std::string password_;
        void SaveAllFile(/*ref*/bit7z::BitArchiveWriter& compress, const std::string& path);
    };
#endif // USE_BIT7Z
}// namespace HsBa::Slicer
#endif // !HSBA_SLICER_BIT7Z_ZIPPER_HPP
