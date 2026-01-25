#include "encoding_convert.hpp"

//use qt for qstring
#ifdef QT_VERSION 
#define USE_QSTRING
#include <QString>
#include <QStringConverter>
#else
#ifndef __ANDROID__
#include <boost/locale.hpp>
#else
#include <vector>
#include <iconv.h>
#include <errno.h>
#endif // !__ANDROID__
#endif // QT_VERSION 

#if _WIN32
#include <Windows.h>
#include <VersionHelpers.h>
#endif

#include "base/error.hpp"

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
#else // __ANDROID__
		constexpr size_t kMinBufferSize = 64; // Minimum buffer size for output
		constexpr size_t kInitialExpansionFactor = 2;
		constexpr size_t kMaxExpansionFactor = 16; // Maximum expansion factor to prevent infinite loops
		constexpr size_t kGrowthMultiplier = 2;
		if (from == to || str.empty()) {
			return str;
		}

		struct IconvDeleter {
			iconv_t cd;
			IconvDeleter(iconv_t c) : cd(c) {}
			~IconvDeleter() { if (cd != reinterpret_cast<iconv_t>(-1)) iconv_close(cd); }
		};

		iconv_t cd = iconv_open(to.c_str(), from.c_str());
		if (cd == reinterpret_cast<iconv_t>(-1)) {
			throw RuntimeError("iconv_open failed for: " + from + " -> " + to);
		}
		IconvDeleter guard(cd);

		std::string inBuf = str;
		size_t inBytesLeft = inBuf.size();

		size_t outBufSize = std::max(inBytesLeft * kInitialExpansionFactor, static_cast<size_t>(kMinBufferSize));
		std::vector<char> outBuf;

		char* inPtr = inBuf.data();
		char* outPtr = nullptr;
		size_t outBytesLeft = 0;
		size_t res = 0;

		do {
			outBuf.resize(outBufSize);
			outBytesLeft = outBufSize;
			outPtr = outBuf.data();

			inPtr = inBuf.data();
			inBytesLeft = inBuf.size();

			res = iconv(cd, &inPtr, &inBytesLeft, &outPtr, &outBytesLeft);

			if (res == static_cast<size_t>(-1) && errno != E2BIG) {
				throw RuntimeError("iconv failed: " + std::string(strerror(errno)));
			}

			if (errno == E2BIG) {
				outBufSize *= kGrowthMultiplier;
			}
		} while (errno == E2BIG && outBufSize < inBytesLeft * kMaxExpansionFactor);

		return std::string(outBuf.data(), outBuf.size() - outBytesLeft);
#endif // __ANDROID__
	}
#endif // !QT_VERSION

#if _WIN32
	constexpr UINT WINDOWS_CHCP_UTF8 = CP_UTF8;//65001
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
}// namespace HsBa::Slicer