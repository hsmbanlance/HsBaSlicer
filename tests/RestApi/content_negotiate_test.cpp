/** @file content_negotiate_test.cpp
 * @brief Unit tests for the JSON/XML content-negotiation helpers.
 * @author HsBa
 */
#define BOOST_TEST_MODULE RestApiContentNegotiateTest
#include <boost/test/included/unit_test.hpp>

#include "../../restapi/content_negotiate.hpp"

using namespace HsBa::Slicer::RestApi;

BOOST_AUTO_TEST_SUITE(restapi_content_negotiate)

BOOST_AUTO_TEST_CASE(request_body_format_defaults_to_json)
{
    BOOST_CHECK(RequestBodyFormat("application/json") == DataFormat::Json);
    BOOST_CHECK(RequestBodyFormat("application/json; charset=utf-8") == DataFormat::Json);
    BOOST_CHECK(RequestBodyFormat("") == DataFormat::Json);  // unknown -> json default
    BOOST_CHECK(RequestBodyFormat("text/plain") == DataFormat::Json);
}

BOOST_AUTO_TEST_CASE(request_body_format_detects_xml)
{
    BOOST_CHECK(RequestBodyFormat("application/xml") == DataFormat::Xml);
    BOOST_CHECK(RequestBodyFormat("text/xml") == DataFormat::Xml);
    BOOST_CHECK(RequestBodyFormat("APPLICATION/XML") == DataFormat::Xml);  // case-insensitive
}

BOOST_AUTO_TEST_CASE(query_format_beats_accept)
{
    // ?format wins over both the Accept header and the configured default.
    BOOST_CHECK(ResponseFormat("xml", "application/json", "json") == DataFormat::Xml);
    BOOST_CHECK(ResponseFormat("json", "application/xml", "xml") == DataFormat::Json);
}

BOOST_AUTO_TEST_CASE(accept_used_when_no_query)
{
    BOOST_CHECK(ResponseFormat("", "application/xml", "json") == DataFormat::Xml);
    BOOST_CHECK(ResponseFormat("", "application/json", "xml") == DataFormat::Json);
}

BOOST_AUTO_TEST_CASE(default_used_when_neither_present)
{
    BOOST_CHECK(ResponseFormat("", "*/*", "xml") == DataFormat::Xml);
    BOOST_CHECK(ResponseFormat("", "", "json") == DataFormat::Json);
    BOOST_CHECK(ResponseFormat("", "", "bogus") == DataFormat::Json);  // non-xml defaults to json
}

BOOST_AUTO_TEST_CASE(content_type_matches_format)
{
    BOOST_CHECK(std::string(ContentType(DataFormat::Json)).find("json") != std::string::npos);
    BOOST_CHECK(std::string(ContentType(DataFormat::Xml)).find("xml") != std::string::npos);
}

BOOST_AUTO_TEST_SUITE_END()
