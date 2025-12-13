#pragma once
#ifndef HSBA_SLICER_FILENAME_CHECK_HPP
#define HSBA_SLICER_FILENAME_CHECK_HPP

#include <string>
#include <string_view>

namespace HsBa::Slicer
{	
	/**
	 * @brief string with no ASCII characters
	 * @param str string
	 * @return rue if string with no ASCII characters
	 */
	bool StringWithNoASCII(const std::string& str);
	
	/**
	 * @brief string which can be used as filename
	 * @param str string
	 * @return true if string can be used as filename
	 */
	bool StringEnableFileName(const std::string& str);
	
	/**
	 * @brief string which can be used as filename with path
	 * @param str string
	 * @return true if string can be used as filename with path
	 */
	bool StringEnableFileNameWithPath(const std::string& str);
	/// <summary>
	/// string which can be used as filename and only ASCII characters
	/// </summary>
	/// <param name="str">string</param>
	/// <returns>true if string can be used as filename and only ASCII characters</returns>
	bool StringEnableFileNameAndOnlyASCII(const std::string& str);
	
	/**
	 * @brief string which can be used as filename with path and only ASCII characters
	 * @param str string
	 * @return true if string can be used as filename with path and only ASCII characters
	 */
	bool StringEnableFileNameAndOnlyASCIIWithPath(const std::string& str);
	
	/**
	 * @brief wstring with no ASCII characters
	 * @param str wstring
	 * @return true if wstring with no ASCII characters
	 */
	bool StringWithNoASCII(const std::wstring& str);
	
	/**
	 * @brief wstring which can be used as filename
	 * @param str wstring
	 * @return true if wstring can be used as filename
	 */
	bool StringEnableFileName(const std::wstring& str);
	
	/**
	 * @brief wstring which can be used as filename with path
	 * @param str wstring
	 * @return true if wstring can be used as filename with path
	 */
	bool StringEnableFileNameWithPath(const std::wstring& str);

	/**
	 * @brief wstring which can be used as filename and only ASCII characters
	 * @param str wstring
	 * @return true if wstring can be used as filename and only ASCII characters
	 */
	bool StringEnableFileNameAndOnlyASCII(const std::wstring& str);
	
	/**
	 * @brief wstring which can be used as filename with path and only ASCII characters
	 * @param str wstring
	 * @return true if wstring can be used as filename with path and only ASCII characters
	 */
	bool StringEnableFileNameAndOnlyASCIIWithPath(const std::wstring& str);
}// namespace HsBa::Slicer

#endif // !HSBA_SLICER_FILENAME_CHECK_HPP
