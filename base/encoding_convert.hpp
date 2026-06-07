/** @file encoding_convert.hpp
 * @brief A header file containing the definition of encoding conversion functions.
 * @author HsBa
 * @date 2024-06-01
 */
#pragma once
#ifndef HSBA_SLICER_ENCODING_CONVERT_HPP
#define HSBA_SLICER_ENCODING_CONVERT_HPP

#include <string>
#include <string_view>
#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

namespace HsBa::Slicer
{
// encoding convert

/** @brief Convert a UTF-8 string to the local encoding.
 * @param str The UTF-8 string to convert.
 * @return The converted string in the local encoding.
 */
std::string utf8_to_local(const std::string& str);
/** @brief Convert a local encoding string to UTF-8.
 * @param str The local encoding string to convert.
 * @return The converted string in UTF-8.
 */
std::string local_to_utf8(const std::string& str);

#ifndef QT_VERSION
/** @brief Convert a string from one encoding to another.
 * @param str The string to convert.
 * @param from The source encoding.
 * @param to The target encoding.
 * @return The converted string.
 */
std::string encoding_convert(const std::string& str, const std::string& from, const std::string& to);
#endif  // QT_VERSION

#if _WIN32
/** @brief Set the console code page to UTF-8.
 * @return True if successful, false otherwise.
 */
bool windows_chcp_utf8();
/** @brief Set the Windows console to UTF-8.
 * @return True if successful, false otherwise.
 */
bool set_windows_console_utf8();
/** @brief Set the Windows console to ANSI.
 * @return True if successful, false otherwise.
 */
bool set_windows_console_ansi();
/** @brief Set the Windows console input to ANSI and output to UTF-8.
 * @return True if successful, false otherwise.
 */
bool set_windows_console_in_ansi_out_utf8();
/** @brief Set the Windows console input to UTF-8 and output to ANSI.
 * @return True if successful, false otherwise.
 */
bool set_windows_console_out_ansi_in_utf8();
/** @brief Check if the Windows version is Win7 or greater.
 * @return True if the version is Win7 or greater, false otherwise.
 */
bool windows_win7_or_greater();
/** @brief Check if the Windows version is Win10 or greater.
 * @return True if the version is Win10 or greater, false otherwise.
 */
bool windows_win10_or_greater();
#endif  // _WIN32
/** @brief An enumeration of supported systems.
 */
enum class System
{
    /** @brief The system type is undefined. */
    Undefined,
    /** @brief The system is Windows. */
    Windows,
    /** @brief The system is Linux. */
    Linux,
    /** @brief The system is macOS. */
    MacOS,
    /** @brief The system is Android. */
    Android,
    /** @brief The system is iOS. */
    IOS,
    /** @brief The system is Unix or Unix-like. */
    Unix,
    /** @brief The system is FreeBSD. */
    FreeBSD,
    /** @brief The system is NetBSD. */
    NetBSD,
    /** @brief The system is OpenBSD. */
    OpenBSD,
    /** @brief The system is DragonFly. */
    DragonFly,
    /** @brief The system is unknown. */
    Unknown
};
/** @brief Get the current system type.
 * @return The current system type as a value of the System enumeration.
 */
constexpr System system_type()
{
#if _WIN32
    return System::Windows;
#elif __ANDROID__
    return System::Android;
#elif __linux__
    return System::Linux;
#elif __APPLE__
#if TARGET_OS_IPHONE
    return System::IOS;
#elif TARGET_OS_MAC
    return System::MacOS;
#else
    return System::Unknown;
#endif
#elif __unix__
    return System::Unix;
#elif __FreeBSD__
    return System::FreeBSD;
#elif __NetBSD__
    return System::NetBSD;
#elif __OpenBSD__
    return System::OpenBSD;
#elif __DragonFly__
    return System::DragonFly;
#else
    return System::Unknown;
#endif
}
/** @brief Check if the current system is Windows.
 * @return True if the current system is Windows, false otherwise.
 */
constexpr bool is_windows()
{
    return system_type() == System::Windows;
}
/** @brief Check if the current system is Linux.
 * @return True if the current system is Linux, false otherwise.
 */
constexpr bool is_linux()
{
    return system_type() == System::Linux;
}
/** @brief Check if the current system is macOS.
 * @return True if the current system is macOS, false otherwise.
 */
constexpr bool is_macos()
{
    return system_type() == System::MacOS;
}
/** @brief Check if the current system is Android.
 * @return True if the current system is Android, false otherwise.
 */
constexpr bool is_android()
{
    return system_type() == System::Android;
}
/** @brief Check if the current system is iOS.
 * @return True if the current system is iOS, false otherwise.
 */
constexpr bool is_ios()
{
    return system_type() == System::IOS;
}
/** @brief Check if the current system is Unix or Unix-like.
 * @return True if the current system is Unix or Unix-like, false otherwise.
 */
constexpr bool is_unix()
{
    return system_type() == System::Unix;
}
/** @brief Check if the current system is FreeBSD.
 * @return True if the current system is FreeBSD, false otherwise.
 */
constexpr bool is_freebsd()
{
    return system_type() == System::FreeBSD;
}
/** @brief Check if the current system is NetBSD.
 * @return True if the current system is NetBSD, false otherwise.
 */
constexpr bool is_netbsd()
{
    return system_type() == System::NetBSD;
}
/** @brief Check if the current system is OpenBSD.
 * @return True if the current system is OpenBSD, false otherwise.
 */
constexpr bool is_openbsd()
{
    return system_type() == System::OpenBSD;
}
/** @brief Check if the current system is DragonFly.
 * @return True if the current system is DragonFly, false otherwise.
 */
constexpr bool is_dragonfly()
{
    return system_type() == System::DragonFly;
}
/** @brief Check if the current system is a Unix-like system.
 * @return True if the current system is a Unix-like system, false otherwise.
 */
constexpr bool like_to_unix()
{
    return is_unix() || is_freebsd() || is_netbsd() || is_openbsd() || is_dragonfly();
}
/** @brief Check if the current system is a general-purpose computer.
 * @return True if the current system is a general-purpose computer, false otherwise.
 */
constexpr bool like_to_computer()
{
    return is_windows() || is_linux() || is_macos();
}
/** @brief Check if the current system is a smartphone or tablet.
 * @return True if the current system is a smartphone or tablet, false otherwise.
 */
constexpr bool like_to_smartphone_or_pad()
{
    return is_ios() || is_android();
}
/** @brief Check if the current system is a server or workstation.
 * @return True if the current system is a server or workstation, false otherwise.
 */
constexpr bool like_to_server()
{
    return is_unix() || is_freebsd() || is_netbsd() || is_openbsd() || is_dragonfly() || is_windows() || is_linux();
}
/** @brief Check if the current system is likely to be used by gamers.
 * @return True if the current system is likely to be used by gamers, false otherwise.
 */
constexpr bool like_to_gamer()
{
#ifdef GAMER
    return true;
#endif
    return is_windows() || is_linux() || is_macos() || is_ios() || is_android();
}
/** @brief Check if the current system is likely to be used by developers.
 * @return True if the current system is likely to be used by developers, false otherwise.
 */
constexpr bool like_to_develop()
{
    return is_windows() || is_linux() || is_macos() || is_unix() || is_freebsd() || is_netbsd() || is_openbsd() ||
           is_dragonfly();
}
/** @brief Check if the current system is likely to be used by designers.
 * @return True if the current system is likely to be used by designers, false otherwise.
 */
constexpr bool like_to_designer()
{
    return is_windows() || is_linux() || is_macos();
}
/** @brief Check if the current system is likely to be used by general users.
 * @return True if the current system is likely to be used by general users, false otherwise.
 */
constexpr bool support_vcpkg()
{
    return is_windows() || is_linux() || is_macos() || is_ios() || is_android();
}
/** @brief Check if the current system is likely to be supported by CMake.
 * @return True if the current system is likely to be supported by CMake, false otherwise.
 */
constexpr bool support_cmake()
{
    return is_windows() || is_linux() || is_macos() || is_ios() || is_android();
}
/** @brief Check if the current system is likely to be supported by MSBuild.
 * @return True if the current system is likely to be supported by MSBuild, false otherwise.
 */
constexpr bool support_msbuild()
{
    return is_windows();
}
}  // namespace HsBa::Slicer

#endif  // !HSBA_SLICER_ENCODING_CONVERT_HPP
