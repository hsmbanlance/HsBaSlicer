#include "encoding_convert.hpp"

//use qt for qstring
#ifdef QT_VERSION 
#define USE_QSTRING
#include <QString>
#include <QStringConverter>
#else
#ifndef __ANDROID__
#include <boost/locale.hpp>
#endif // !__ANDROID__
#endif // QT_VERSION 

#if _WIN32
#include <Windows.h>
#include <VersionHelpers.h>
#endif

namespace HsBa::Slicer
{
#ifndef USE_QSTRING
	//boost system locale may error if use qt
	static std::string SystemLocaleStr()
	{
#ifdef __ANDROID__
		return "utf8";
#else
		std::string strCodePage = boost::locale::util::get_system_locale();
		std::locale loc = boost::locale::generator().generate(strCodePage);
		return std::use_facet<boost::locale::info>(loc).encoding();
#endif // __ANDROID__
	}
#endif // !USE_QSTRING

	std::string utf8_to_local(const std::string& str)
	{
#ifndef USE_QSTRING
#ifdef __ANDROID__
		return str;
#else
#if _WIN32
		if (windows_chcp_utf8()) //If chcp is utf-8,return str
		{
			return str;
		}
#endif // _WIN32
		return boost::locale::conv::between(str, SystemLocaleStr(), "utf-8");
#endif // __ANDROID__
#else
		//If Use qt,code here
		return QString::fromStdString(str).toStdString();
#endif // !USE_QSTRING
	}
	std::string local_to_utf8(const std::string& str)
	{
#ifndef USE_QSTRING
#ifdef __ANDROID__
		return str;
#else
#if _WIN32
		if (windows_chcp_utf8())//If chcp is utf-8,return str
		{
			return str;
		}
#endif // _WIN32
		return boost::locale::conv::between(str, "utf-8", SystemLocaleStr());
#endif // __ANDROID__


#else
		//If Use qt,code here
		return QString::fromStdString(str).toStdString();
#endif // !USE_QSTRING
	}

#ifndef QT_VERSION
	std::string encoding_convert(const std::string& str, const std::string& from, const std::string& to)
	{
#ifndef __ANDROID__
		if (from == to)
		{
			return str;
		}
		return boost::locale::conv::between(str, from, to);
#else
		return str;
#endif // !__ANDROID__
	}
#endif // !QT_VERSION

#if _WIN32
	constexpr UINT WINDOWS_CHCP_UTF8 = 65001;
	bool windows_chcp_utf8()
	{
		return GetACP() == WINDOWS_CHCP_UTF8;//65001 is utf-8
	}
	bool set_windows_console_utf8()
	{
		return SetConsoleCP(WINDOWS_CHCP_UTF8) || SetConsoleOutputCP(WINDOWS_CHCP_UTF8);
	}

	bool set_windows_console_ansi()
	{
		UINT windows_ansi = GetACP();
		return SetConsoleCP(windows_ansi) || SetConsoleOutputCP(windows_ansi);
	}

	bool set_windows_console_in_ansi_out_utf8()
	{
		UINT windows_ansi = GetACP();
		return SetConsoleCP(windows_ansi) || SetConsoleOutputCP(WINDOWS_CHCP_UTF8);
	}

	bool set_windows_console_out_ansi_in_utf8()
	{
		UINT windows_ansi = GetACP();
		return SetConsoleCP(WINDOWS_CHCP_UTF8) || SetConsoleOutputCP(windows_ansi);
	}

	bool windows_win7_or_greater()
	{
		return IsWindows7OrGreater();
	}

	bool windows_win10_or_greater()
	{
		return IsWindows10OrGreater();
	}
#endif // _WIN32

	constexpr System system_type()
	{
#if _WIN32
		return System::Windows;
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
#elif __ANDROID__
		return System::Android;
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
	constexpr bool is_ios()
	{
		return system_type() == System::IOS;
	}
	constexpr bool is_unix()
	{
		return system_type() == System::Unix;
	}
	constexpr bool is_android()
	{
		return system_type() == System::Android;
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