#include "version_info.hpp"

#include "utils/struct_json.hpp"
#include "utils/struct_xml.hpp"
#include "version.hpp"

namespace HsBa::Slicer
{
HSBA_SLICER_LIB_API std::string GetVersionJson()
{
    const auto info = HsBa::Slicer::Version::GetVersionInfo();
    return HsBa::Slicer::Utils::write_pretty_json(info);
}

HSBA_SLICER_LIB_API std::string GetVersionXml()
{
    const auto info = HsBa::Slicer::Version::GetVersionInfo();
    return HsBa::Slicer::Utils::write_xml(info, "VersionInfo");
}

}  // namespace HsBa::Slicer
