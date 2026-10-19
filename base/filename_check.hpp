/** @file filename_check.hpp
 * @brief A header file containing functions for checking the validity of filenames and paths in the HsBa Slicer
 * project. This file defines a set of functions that check whether a given string can be used as a valid filename or
 * path. The functions check for the presence of invalid characters, ensure that the filename is not empty, and inspect
 * whether the string is pure ASCII or carries non-ASCII code units. These checks help ensure that filenames and paths
 * used in the HsBa Slicer project are valid and do not cause issues when creating or accessing files.
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_FILENAME_CHECK_HPP
#define HSBA_SLICER_FILENAME_CHECK_HPP

#include <string>
#include <string_view>

namespace HsBa::Slicer
{
/**
 * @brief string that contains only ASCII characters
 * @param str string
 * @return true if every character of str is within the ASCII range (0x00-0x7F)
 */
bool StringIsOnlyASCII(const std::string& str);

/**
 * @brief string which can be used as filename
 * @param str string
 * @return true if string can be used as filename
 */
bool StringIsValidFileName(const std::string& str);

/**
 * @brief string which can be used as filename with path
 * @param str string
 * @return true if string can be used as filename with path
 */
bool StringIsValidPath(const std::string& str);
/**
 * @brief string which can be used as filename and carries at least one non-ASCII character
 * @param str string
 * @return true if string is a valid filename and contains at least one non-ASCII character
 */
bool StringIsValidFileNameWithNonASCII(const std::string& str);

/**
 * @brief string which can be used as filename with path and carries at least one non-ASCII character
 * @param str string
 * @return true if string is a valid filename-with-path and contains at least one non-ASCII character
 */
bool StringIsValidPathWithNonASCII(const std::string& str);

/**
 * @brief wstring that contains only ASCII characters
 * @param str wstring
 * @return true if every code unit of str is within the ASCII range (0x00-0x7F)
 */
bool StringIsOnlyASCII(const std::wstring& str);

/**
 * @brief wstring which can be used as filename
 * @param str wstring
 * @return true if wstring can be used as filename
 */
bool StringIsValidFileName(const std::wstring& str);

/**
 * @brief wstring which can be used as filename with path
 * @param str wstring
 * @return true if wstring can be used as filename with path
 */
bool StringIsValidPath(const std::wstring& str);

/**
 * @brief wstring which can be used as filename and carries at least one non-ASCII character
 * @param str wstring
 * @return true if wstring is a valid filename and contains at least one non-ASCII character
 */
bool StringIsValidFileNameWithNonASCII(const std::wstring& str);

/**
 * @brief wstring which can be used as filename with path and carries at least one non-ASCII character
 * @param str wstring
 * @return true if wstring is a valid filename-with-path and contains at least one non-ASCII character
 */
bool StringIsValidPathWithNonASCII(const std::wstring& str);
}  // namespace HsBa::Slicer

#endif  // !HSBA_SLICER_FILENAME_CHECK_HPP
