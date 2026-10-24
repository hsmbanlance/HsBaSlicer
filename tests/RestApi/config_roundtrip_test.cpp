/** @file config_roundtrip_test.cpp
 * @brief Reflection-bridge round-trip tests (config -> JSON/XML -> config).
 * @author HsBa
 */
#include <string>

#define BOOST_TEST_MODULE RestApiConfigRoundtripTest
#include <boost/test/included/unit_test.hpp>

#include "fileoperator/param_convert.hpp"
#include "pipelinetypes/pipeline_types.h"

#include "../../restapi/config_bridge.hpp"
#include "../../restapi/content_negotiate.hpp"
#include "../../restapi/pipeline_executor.hpp"

using namespace HsBa::Slicer;
using namespace HsBa::Slicer::RestApi;

BOOST_AUTO_TEST_SUITE(restapi_config_bridge)

BOOST_AUTO_TEST_CASE(fdm_default_roundtrips_through_json)
{
    void* a = CreateDefaultConfig(HSBA_PIPELINE_FDM);
    BOOST_REQUIRE(a != nullptr);
    const std::string json = SerializeConfig(ConfigTypeInfo(HSBA_PIPELINE_FDM), a, DataFormat::Json);

    void* b = CreateDefaultConfig(HSBA_PIPELINE_FDM);
    StringArena arena;
    DeserializeConfig(ConfigTypeInfo(HSBA_PIPELINE_FDM), json, DataFormat::Json, b, arena);
    const std::string json2 = SerializeConfig(ConfigTypeInfo(HSBA_PIPELINE_FDM), b, DataFormat::Json);

    BOOST_CHECK(json == json2);  // serialize -> deserialize -> serialize is stable
    DestroyConfig(HSBA_PIPELINE_FDM, a);
    DestroyConfig(HSBA_PIPELINE_FDM, b);
}

BOOST_AUTO_TEST_CASE(fdm_default_roundtrips_through_xml)
{
    void* a = CreateDefaultConfig(HSBA_PIPELINE_FDM);
    const std::string xml = SerializeConfig(ConfigTypeInfo(HSBA_PIPELINE_FDM), a, DataFormat::Xml);

    void* b = CreateDefaultConfig(HSBA_PIPELINE_FDM);
    StringArena arena;
    DeserializeConfig(ConfigTypeInfo(HSBA_PIPELINE_FDM), xml, DataFormat::Xml, b, arena);
    const std::string xml2 = SerializeConfig(ConfigTypeInfo(HSBA_PIPELINE_FDM), b, DataFormat::Xml);

    BOOST_CHECK(xml == xml2);
    DestroyConfig(HSBA_PIPELINE_FDM, a);
    DestroyConfig(HSBA_PIPELINE_FDM, b);
}

BOOST_AUTO_TEST_CASE(json_body_populates_scalar_and_string_fields)
{
    const std::string body = R"({"model_name":"cube","wall_count":7})";
    void* cfg = CreateDefaultConfig(HSBA_PIPELINE_FDM);
    StringArena arena;
    DeserializeConfig(ConfigTypeInfo(HSBA_PIPELINE_FDM), body, DataFormat::Json, cfg, arena);

    const std::string out = SerializeConfig(ConfigTypeInfo(HSBA_PIPELINE_FDM), cfg, DataFormat::Json);
    BOOST_CHECK(out.find("\"model_name\": \"cube\"") != std::string::npos);
    BOOST_CHECK(out.find("\"wall_count\": 7") != std::string::npos);

    DestroyConfig(HSBA_PIPELINE_FDM, cfg);
}

BOOST_AUTO_TEST_CASE(unknown_field_is_ignored)
{
    const std::string body = R"({"nonexistent_field":123,"wall_count":2})";
    void* cfg = CreateDefaultConfig(HSBA_PIPELINE_FDM);
    StringArena arena;
    BOOST_CHECK_NO_THROW(DeserializeConfig(ConfigTypeInfo(HSBA_PIPELINE_FDM), body, DataFormat::Json, cfg, arena));
    const std::string out = SerializeConfig(ConfigTypeInfo(HSBA_PIPELINE_FDM), cfg, DataFormat::Json);
    BOOST_CHECK(out.find("nonexistent_field") == std::string::npos);
    DestroyConfig(HSBA_PIPELINE_FDM, cfg);
}

BOOST_AUTO_TEST_CASE(invalid_json_throws)
{
    void* cfg = CreateDefaultConfig(HSBA_PIPELINE_FDM);
    StringArena arena;
    BOOST_CHECK_THROW(DeserializeConfig(ConfigTypeInfo(HSBA_PIPELINE_FDM), "{not json", DataFormat::Json, cfg, arena),
                      std::exception);
    DestroyConfig(HSBA_PIPELINE_FDM, cfg);
}

BOOST_AUTO_TEST_CASE(executor_kind_helpers)
{
    BOOST_CHECK(IsValidKind(HSBA_PIPELINE_FDM));
    BOOST_CHECK(!IsValidKind(HSBA_PIPELINE_KIND_COUNT));
    BOOST_CHECK(std::string(KindName(HSBA_PIPELINE_WAAM)) == "waam");
    BOOST_CHECK(ParseKind("sla") == HSBA_PIPELINE_SLA);
    BOOST_CHECK(ParseKind("nope") == HSBA_PIPELINE_KIND_COUNT);
    BOOST_CHECK(ConfigTypeInfo(HSBA_PIPELINE_SLA) != nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
