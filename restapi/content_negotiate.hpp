/** @file content_negotiate.hpp
 * @brief JSON/XML format selection helpers for the REST server (no HTTP dependency).
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_RESTAPI_CONTENT_NEGOTIATE_HPP
#define HSBA_SLICER_RESTAPI_CONTENT_NEGOTIATE_HPP

#include <string>
#include <string_view>

namespace HsBa::Slicer::RestApi
{
/** @brief The two wire formats the server speaks. */
enum class DataFormat
{
    Json,  ///< JSON (application/json)
    Xml,   ///< XML (application/xml)
};

/**
 * @brief Decide the request-body format from a Content-Type header value.
 * @param content_type Raw Content-Type header (may include charset etc.).
 * @return DataFormat::Xml when the type denotes XML, otherwise Json (the default).
 */
DataFormat RequestBodyFormat(const std::string& content_type);

/**
 * @brief Decide the response format for a request.
 *
 * Precedence: an explicit ?format= query value wins; otherwise a client Accept
 * header mentioning xml (without a stronger json preference) selects XML;
 * otherwise the configured default format is used.
 *
 * @param query_format   Value of the "format" query parameter ("" when absent).
 * @param accept         Raw Accept header value ("" when absent).
 * @param default_format Server default, "json" or "xml".
 * @return The chosen format.
 */
DataFormat ResponseFormat(const std::string& query_format, const std::string& accept,
                          const std::string& default_format);

/** @brief Media type for a format, suitable for an HTTP Content-Type header. */
std::string_view ContentType(DataFormat fmt);

/** @brief Whether a format denotes XML. */
inline bool IsXml(DataFormat fmt)
{
    return fmt == DataFormat::Xml;
}

}  // namespace HsBa::Slicer::RestApi

#endif  // HSBA_SLICER_RESTAPI_CONTENT_NEGOTIATE_HPP
