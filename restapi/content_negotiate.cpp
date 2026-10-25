/** @file content_negotiate.cpp
 * @brief Implements the JSON/XML negotiation helpers; see content_negotiate.hpp.
 * @author HsBa
 */
#include "content_negotiate.hpp"

#include <string>
#include <string_view>

#include "base/string_helper.hpp"

namespace HsBa::Slicer::RestApi
{
namespace
{
using HsBa::Slicer::Utils::to_lower;

bool Contains(const std::string& haystack, std::string_view needle)
{
    return haystack.find(needle) != std::string::npos;
}
}  // namespace

DataFormat RequestBodyFormat(const std::string& content_type)
{
    const std::string ct = to_lower(content_type);
    if (Contains(ct, "xml"))
    {
        return DataFormat::Xml;
    }
    return DataFormat::Json;
}

DataFormat ResponseFormat(const std::string& query_format, const std::string& accept, const std::string& default_format)
{
    const std::string q = to_lower(query_format);
    if (q == "xml")
    {
        return DataFormat::Xml;
    }
    if (q == "json")
    {
        return DataFormat::Json;
    }

    const std::string a = to_lower(accept);
    if (Contains(a, "xml"))
    {
        return DataFormat::Xml;
    }
    if (Contains(a, "json"))
    {
        return DataFormat::Json;
    }

    return to_lower(default_format) == "xml" ? DataFormat::Xml : DataFormat::Json;
}

std::string_view ContentType(DataFormat fmt)
{
    return fmt == DataFormat::Xml ? "application/xml; charset=utf-8" : "application/json; charset=utf-8";
}

}  // namespace HsBa::Slicer::RestApi
