/** @file serialize_value.hpp
 * @brief Thin wrappers over the project Utils serializers plus shared REST view types.
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_RESTAPI_SERIALIZE_VALUE_HPP
#define HSBA_SLICER_RESTAPI_SERIALIZE_VALUE_HPP

#include <string>

#include "utils/struct_json.hpp"
#include "utils/struct_xml.hpp"

#include "content_negotiate.hpp"

namespace HsBa::Slicer::RestApi
{
/**
 * @brief Uniform error payload serialized to {code, message} JSON / <Error> XML.
 *
 * An aggregate of std::string + int, so both struct_json and struct_xml handle it
 * without special conversion support.
 */
struct ErrorView
{
    int code = 0;         ///< HTTP status code.
    std::string message;  ///< Human-readable error message.
    std::string detail;   ///< Optional extra context (may be empty).
};

/**
 * @brief Serialize any aggregate/reflectable value to JSON or XML text.
 * @tparam T Aggregate type handled by struct_json / struct_xml.
 * @param value Value to serialize.
 * @param fmt   Target wire format.
 * @param root  XML root element name (ignored for JSON).
 * @return Serialized text.
 */
template <typename T>
inline std::string SerializeValue(const T& value, DataFormat fmt, const char* root = "root")
{
    if (IsXml(fmt))
    {
        return Utils::write_xml(value, root);
    }
    return Utils::write_pretty_json(value);
}

/** @brief Serialize an ErrorView into a formatted body for the given target format. */
inline std::string SerializeError(const ErrorView& err, DataFormat fmt)
{
    return SerializeValue(err, fmt, "Error");
}

}  // namespace HsBa::Slicer::RestApi

#endif  // HSBA_SLICER_RESTAPI_SERIALIZE_VALUE_HPP
