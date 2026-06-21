#pragma once
#ifndef HSBA_SLICER_IZIPPER_HPP
#define HSBA_SLICER_IZIPPER_HPP

#include <string>
#include <string_view>

namespace HsBa::Slicer
{
/**
 * @brief Interface for archive compression (zipper) implementations.
 *
 * This abstract class defines the common interface for creating compressed archives
 * from files and in-memory data.
 */
class IZipper
{
public:
    virtual ~IZipper() {}

    /**
     * @brief Add a file from memory buffer to the archive.
     * @param name Name of the file within the archive.
     * @param data File content as string.
     */
    virtual void AddByteFile(std::string_view name, const std::string& data) = 0;

    /**
     * @brief Add a file from disk to the archive.
     * @param name Name of the file within the archive.
     * @param path Path to the source file on disk.
     */
    virtual void AddFile(std::string_view name, std::string_view path) = 0;

    /**
     * @brief Add a file from memory buffer, ignoring if duplicate exists.
     * @param name Name of the file within the archive.
     * @param data File content as string.
     */
    virtual void AddByteFileIgnoreDuplicate(std::string_view name, const std::string& data) = 0;

    /**
     * @brief Add a file from disk, ignoring if duplicate exists.
     * @param name Name of the file within the archive.
     * @param path Path to the source file on disk.
     */
    virtual void AddFileIgnoreDuplicate(std::string_view name, std::string_view path) = 0;

    /**
     * @brief Save the archive to disk.
     * @param filePath Output path for the archive file.
     */
    virtual void Save(std::string_view filePath) = 0;

    IZipper() = default;
    IZipper(const IZipper&) = delete;
    IZipper& operator=(const IZipper&) = delete;

private:
};
}  // namespace HsBa::Slicer
#endif  // !HSBA_SLICER_IZIPPER_HPP
