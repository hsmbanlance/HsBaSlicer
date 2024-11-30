#pragma once
#ifndef HSBA_SLICER_FILENAME_CHECK_HPP
#define HSBA_SLICER_FILENAME_CHECK_HPP

#include <string>
#include <string_view>

namespace HsBa::Slicer
{
	/// <summary>
	/// string with no ASCII characters
	/// </summary>
	/// <param name="str">string</param>
	/// <returns>true if string with no ASCII characters</returns>
	bool StringWithNoASCII(const std::string& str);
	/// <summary>
	/// string which can be used as filename
	/// </summary>
	/// <param name="str">string</param>
	/// <returns>true if string can be used as filename</returns>
	bool StringEnableFileName(const std::string& str);
	/// <summary>
	/// string which can be used as filename with path
	/// </summary>
	/// <param name="str">string</param>
	/// <returns>true if string can be used as filename with path</returns>
	bool StringEnableFileNameWithPath(const std::string& str);
	/// <summary>
	/// string which can be used as filename and only ASCII characters
	/// </summary>
	/// <param name="str">string</param>
	/// <returns>true if string can be used as filename and only ASCII characters</returns>
	bool StringEnableFileNameAndOnlyASCII(const std::string& str);
	/// <summary>
	/// string which can be used as filename with path and only ASCII characters
	/// </summary>
	/// <param name="str">string</param>
	/// <returns>true if string can be used as filename with path and only ASCII characters</returns>
	bool StringEnableFileNameAndOnlyASCIIWithPath(const std::string& str);

	/// <summary>
	/// wstring with no ASCII characters
	/// </summary>
	/// <param name="str">wstring</param>
	/// <returns>true if wstring with no ASCII characters</returns>
	bool StringWithNoASCII(const std::wstring& str);
	/// <summary>
	/// wstring which can be used as filename
	/// </summary>
	/// <param name="str">wstring</param>
	/// <returns>true if wstring can be used as filename</returns>
	bool StringEnableFileName(const std::wstring& str);
	/// <summary>
	/// wstring which can be used as filename with path
	/// </summary>
	/// <param name="str">wstring</param>
	/// <returns>true if wstring can be used as filename with path</returns>
	bool StringEnableFileNameWithPath(const std::wstring& str);
	/// <summary>
	/// wstring which can be used as filename and only ASCII characters
	/// </summary>
	/// <param name="str">wstring</param>
	/// <returns>true if wstring can be used as filename and only ASCII characters</returns>
	bool StringEnableFileNameAndOnlyASCII(const std::wstring& str);
	/// <summary>
	/// wstring which can be used as filename with path and only ASCII characters
	/// </summary>
	/// <param name="str">wstring</param>
	/// <returns>true if wstring can be used as filename with path and only ASCII characters</returns>
	bool StringEnableFileNameAndOnlyASCIIWithPath(const std::wstring& str);
}// namespace HsBa::Slicer

#endif // !HSBA_SLICER_FILENAME_CHECK_HPP
