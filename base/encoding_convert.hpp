#pragma once
#ifndef HSBA_SLICER_ENCODING_CONVERT_HPP
#define HSBA_SLICER_ENCODING_CONVERT_HPP

#include <string>
#include <string_view>

namespace HsBa::Slicer 
{
	// encoding convert
	
	std::string utf8_to_local(const std::string& str);
	std::string local_to_utf8(const std::string& str);

#ifndef QT_VERSION
	std::string encoding_convert(const std::string& str,const std::string& from,const std::string& to);
#endif // QT_VERSION

#if _WIN32
	bool windows_chcp_utf8();
	bool set_windows_console_utf8();
	bool set_windows_console_ansi();
	bool set_windows_console_in_ansi_out_utf8();
	bool set_windows_console_out_ansi_in_utf8();
	bool windows_win7_or_greater();
	bool windows_win10_or_greater();
#endif // _WIN32
	enum class System
	{
		Undefined,
		Windows,
		Linux,
		MacOS,
		Android,
		IOS,
		Unix,
		FreeBSD,
		NetBSD,
		OpenBSD,
		DragonFly,
		Unknown
	};
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
	constexpr bool is_windows()
	{
		return system_type() == System::Windows;
	}
	constexpr bool is_linux()
	{
		return system_type() == System::Linux;
	}
	constexpr bool is_macos()
	{
		return system_type() == System::MacOS;
	}
	constexpr bool is_android()
	{
		return system_type() == System::Android;
	}
	constexpr bool is_ios()
	{
		return system_type() == System::IOS;
	}
	constexpr bool is_unix()
	{
		return system_type() == System::Unix;
	}
	constexpr bool is_freebsd()
	{
		return system_type() == System::FreeBSD;
	}
	constexpr bool is_netbsd()
	{
		return system_type() == System::NetBSD;
	}
	constexpr bool is_openbsd()
	{
		return system_type() == System::OpenBSD;
	}
	constexpr bool is_dragonfly()
	{
		return system_type() == System::DragonFly;
	}
	constexpr bool like_to_unix()
	{
		return is_unix() || is_freebsd() || is_netbsd() || is_openbsd() || is_dragonfly();
	}

	constexpr bool like_to_computer()
	{
		return is_windows() || is_linux() || is_macos();
	}
	constexpr bool like_to_smartphone_or_pad()
	{
		return is_ios() || is_android();
	}
	constexpr bool like_to_server()
	{
		return is_unix() || is_freebsd() || is_netbsd() || is_openbsd() || is_dragonfly() || is_windows() || is_linux();
	}
	constexpr bool like_to_gamer()
	{
#ifdef GAMER
		return true;
#endif
		return is_windows() || is_linux() || is_macos() || is_ios() || is_android();
	}
	constexpr bool like_to_develop()
	{
		return is_windows() || is_linux() || is_macos() || is_unix() || is_freebsd() || is_netbsd() || is_openbsd() || is_dragonfly();
	}
	constexpr bool like_to_designer()
	{
		return is_windows() || is_linux() || is_macos();
	}

	constexpr bool support_vcpkg()
	{
		return is_windows() || is_linux() || is_macos() || is_ios() || is_android();
	}
	constexpr bool support_cmake()
	{
		return is_windows() || is_linux() || is_macos() || is_ios() || is_android();
	}
	constexpr bool support_msbuild()
	{
		return is_windows();
	}
}// namespace HsBa::Slicer

#endif // !HSBA_SLICER_ENCODING_CONVERT_HPP
