#include "version_info.h"

#include <cstdlib>
#include <cstring>
#include <string>

#include "LibHsBaSlicer/version_info.hpp"

namespace
{
char* DuplicateString(const std::string& str)
{
    char* result = static_cast<char*>(std::malloc(str.size() + 1));
    if (result)
    {
        std::memcpy(result, str.data(), str.size());
        result[str.size()] = '\0';
    }
    return result;
}
}  // namespace

HSBA_SLICER_API char* HsBaGetVersionJson(void)
{
    const auto json = HsBa::Slicer::GetVersionJson();
    return DuplicateString(json);
}

HSBA_SLICER_API char* HsBaGetVersionXml(void)
{
    const auto xml = HsBa::Slicer::GetVersionXml();
    return DuplicateString(xml);
}

HSBA_SLICER_API void HsBaFreeVersionString(char* str)
{
    std::free(str);
}
