#include "version_info.h"

#include <cstdlib>
#include <cstring>
#include <string>

#include "utils/struct_json.hpp"
#include "utils/struct_xml.hpp"
#include "version.hpp"

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
    const auto info = HsBa::Slicer::Version::GetVersionInfo();
    const auto json = HsBa::Slicer::Utils::write_pretty_json(info);
    return DuplicateString(json);
}

HSBA_SLICER_API char* HsBaGetVersionXml(void)
{
    const auto info = HsBa::Slicer::Version::GetVersionInfo();
    const auto xml = HsBa::Slicer::Utils::write_xml(info, "VersionInfo");
    return DuplicateString(xml);
}

HSBA_SLICER_API void HsBaFreeVersionString(char* str)
{
    std::free(str);
}
