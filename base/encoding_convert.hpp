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
	constexpr System system_type();
	constexpr bool is_windows();
	constexpr bool is_linux();
	constexpr bool is_macos();
	constexpr bool is_android();
	constexpr bool is_ios();
	constexpr bool is_unix();
	constexpr bool is_freebsd();
	constexpr bool is_netbsd();
	constexpr bool is_openbsd();
	constexpr bool is_dragonfly();
	constexpr bool like_to_unix();

	constexpr bool like_to_computer();
	constexpr bool like_to_smartphone_or_pad();
	constexpr bool like_to_server();
	constexpr bool like_to_gamer();
	constexpr bool like_to_develop();
	constexpr bool like_to_designer();

	constexpr bool support_vcpkg();
	constexpr bool support_cmake();
	constexpr bool support_msbuild();
}// namespace HsBa::Slicer

#endif // !HSBA_SLICER_ENCODING_CONVERT_HPP
