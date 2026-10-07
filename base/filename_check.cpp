/** @file filename_check.cpp
 * @brief Implementations of the filename and path validity checking helpers.
 * @author HsBa
 */
#include "filename_check.hpp"

#include <regex>

namespace HsBa::Slicer
{
bool StringIsOnlyASCII(const std::string& str)
{
    std::regex pattern("[^\\x00-\\x7F]");
    return !std::regex_search(str, pattern);
}
bool StringIsValidFileName(const std::string& str)
{
    std::regex pattern(R"([\\/:*?\"<>|])");
    return !std::regex_search(str, pattern) && !str.empty();
}
bool StringIsValidPath(const std::string& str)
{
    std::regex pattern(R"([:*?\"<>|])");
    if (std::regex_search(str, pattern))
    {
        return false;
    }
    // test file not empty
    //  \// or //// is valid separators, only test last filename isn"t empty
    auto pos_windows = str.find_last_of('\\');
    auto pos_unix = str.find_last_of('/');
    if (pos_windows == std::string::npos && pos_unix == std::string::npos)
    {
        return true;
    }
    else if (pos_unix == std::string::npos)
    {
        return !str.substr(pos_windows + 1).empty();
    }
    else if (pos_windows == std::string::npos)
    {
        return !str.substr(pos_unix + 1).empty();
    }
    else
    {
        auto pos = std::max(pos_windows, pos_unix);
        return !str.substr(pos + 1).empty();
    }
}
bool StringIsValidFileNameWithNonASCII(const std::string& str)
{
    return !StringIsOnlyASCII(str) && StringIsValidFileName(str);
}
bool StringIsValidPathWithNonASCII(const std::string& str)
{
    return !StringIsOnlyASCII(str) && StringIsValidPath(str);
}

bool StringIsOnlyASCII(const std::wstring& str)
{
    std::wregex pattern(L"[^\\x00-\\x7F]");
    return !std::regex_search(str, pattern);
}
bool StringIsValidFileName(const std::wstring& str)
{
    std::wregex pattern(LR"([\\/:*?\"<>|])");
    return !std::regex_search(str, pattern) && !str.empty();
}
bool StringIsValidPath(const std::wstring& str)
{
    std::wregex pattern(LR"([:*?\"<>|])");
    if (std::regex_search(str, pattern))
    {
        return false;
    }
    // test file not empty
    auto pos_windows = str.find_last_of('\\');
    auto pos_unix = str.find_last_of('/');
    if (pos_windows == std::string::npos && pos_unix == std::string::npos)
    {
        return true;
    }
    else if (pos_unix == std::string::npos)
    {
        return !str.substr(pos_windows + 1).empty();
    }
    else if (pos_windows == std::string::npos)
    {
        return !str.substr(pos_unix + 1).empty();
    }
    else
    {
        auto pos = std::max(pos_windows, pos_unix);
        return !str.substr(pos + 1).empty();
    }
}
bool StringIsValidFileNameWithNonASCII(const std::wstring& str)
{
    return !StringIsOnlyASCII(str) && StringIsValidFileName(str);
}
bool StringIsValidPathWithNonASCII(const std::wstring& str)
{
    return !StringIsOnlyASCII(str) && StringIsValidPath(str);
}
}  // namespace HsBa::Slicer
