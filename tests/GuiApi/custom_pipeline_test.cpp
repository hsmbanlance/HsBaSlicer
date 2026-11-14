/**
 * @brief Custom Lua pipeline through the C ABI: the HsBa table and the config readers behind it.
 *
 * This is the only entry that reaches LibHsBaSlicer/Extends/lua_pipeline.cpp from outside, so it is
 * also the only place where the boolean configuration fields can be checked for what they accept.
 * The C ABI spells every pipeline flag as int 0/1, and a Lua script that mirrors a native config
 * table hands over exactly those numbers; reading only LUA_TBOOLEAN turned them into the default
 * and the run silently took the other branch.
 * @author HsBa
 */
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

#define BOOST_TEST_MODULE GuiApiCustomPipelineTest
#include <boost/test/included/unit_test.hpp>

#include "DllHsBaSlicer/custom_pipeline.h"
#include "pipelinetypes/pipeline_types.h"

namespace
{
/**
 * @brief Raft the same two separate bodies four times, only the flag spelling differs.
 *
 * With the hull on the two squares become one 40x10 footprint, with it off each keeps its own
 * border loops and fill, so the number of returned paths says which branch was taken. The entry
 * function returns "hull_true hull_1 hull_false hull_0" for the host to compare.
 */
constexpr const char* kFloorFlagScript = R"lua(
    function run_pipeline()
        local function square(ox, oy, s)
            return { {x = ox, y = oy}, {x = ox + s, y = oy}, {x = ox + s, y = oy + s}, {x = ox, y = oy + s} }
        end
        local bodies = { square(0, 0, 10), square(30, 0, 10) }

        local function raft_paths(flag)
            local cfg = { raft_offset = 2.0, border_width = 1.0, fill_spacing = 0.5, border_count = 2,
                          use_convex_hull = flag }
            return #HsBa.floor(bodies, cfg)
        end

        return string.format("%d %d %d %d", raft_paths(true), raft_paths(1), raft_paths(false), raft_paths(0))
    end
)lua";

/** @brief Custom pipeline configuration that only runs an inline script. */
HsBaCustomPipelineConfig_t InlineScriptConfig(const char* source)
{
    HsBaCustomPipelineConfig_t cfg = HsBaCustomConfigDefault();
    cfg.pipeline_lua_source = source;
    return cfg;
}

/** @brief Split the whitespace separated numbers the script returns. */
std::vector<int> ParseCounts(const std::string& text)
{
    std::vector<int> values;
    std::istringstream stream(text);
    int value = 0;
    while (stream >> value)
    {
        values.push_back(value);
    }
    return values;
}
}  // namespace

BOOST_AUTO_TEST_SUITE(custom_pipeline)

// The hull flag has to be readable as the boolean it is documented to be, otherwise nothing below
// the config reader can be trusted.
BOOST_AUTO_TEST_CASE(hsba_floor_accepts_a_boolean_flag)
{
    HsBaCustomPipelineConfig_t cfg = InlineScriptConfig(kFloorFlagScript);
    HsBaCustomPipelineResult_t result = HsBaRunCustomPipeline(&cfg, nullptr, nullptr);
    BOOST_CHECK_MESSAGE(result.success == 1,
                        "custom pipeline failed: " << (result.error_message ? result.error_message : ""));
    BOOST_REQUIRE(result.result_string != nullptr);

    const std::vector<int> counts = ParseCounts(result.result_string);
    BOOST_REQUIRE_EQUAL(counts.size(), 4u);
    BOOST_TEST_MESSAGE("raft paths per flag spelling: true=" << counts[0] << " 1=" << counts[1]
                                                            << " false=" << counts[2] << " 0=" << counts[3]);

    BOOST_CHECK_GT(counts[0], 0);
    BOOST_CHECK_GT(counts[2], 0);
    // The two bodies merge into one footprint only when the hull is on, and one footprint yields
    // fewer loops: 25 paths merged against 54 separate at these inputs (see the message above).
    BOOST_CHECK_NE(counts[0], counts[2]);

    HsBaFreeCustomPipelineResult(&result);
}

// Numbers are the spelling the C ABI structs use: 1 has to mean true and 0 false, not "fall back to
// the default", which for use_convex_hull is false.
BOOST_AUTO_TEST_CASE(hsba_floor_accepts_numeric_flag_spellings)
{
    HsBaCustomPipelineConfig_t cfg = InlineScriptConfig(kFloorFlagScript);
    HsBaCustomPipelineResult_t result = HsBaRunCustomPipeline(&cfg, nullptr, nullptr);
    BOOST_REQUIRE(result.success == 1);
    BOOST_REQUIRE(result.result_string != nullptr);

    const std::vector<int> counts = ParseCounts(result.result_string);
    BOOST_REQUIRE_EQUAL(counts.size(), 4u);

    BOOST_CHECK_EQUAL(counts[1], counts[0]);  // 1 behaves like true
    BOOST_CHECK_EQUAL(counts[3], counts[2]);  // 0 behaves like false
    BOOST_CHECK_NE(counts[1], counts[3]);

    HsBaFreeCustomPipelineResult(&result);
}

// A script that reports failure has to come back as a failure, with a message and without any of
// the result strings left dangling: HsBaFreeCustomPipelineResult owns them until it is called.
BOOST_AUTO_TEST_CASE(entry_function_without_success_is_reported_as_failure)
{
    constexpr const char* kFailingScript = "function run_pipeline()\n  return false\nend\n";
    HsBaCustomPipelineConfig_t cfg = InlineScriptConfig(kFailingScript);
    HsBaCustomPipelineResult_t result = HsBaRunCustomPipeline(&cfg, nullptr, nullptr);
    BOOST_CHECK_EQUAL(result.success, 0);
    BOOST_REQUIRE(result.error_message != nullptr);
    BOOST_CHECK(std::string(result.error_message).size() > 0u);
    HsBaFreeCustomPipelineResult(&result);

    // Freeing releases the buffers and clears the pointers, so a second free cannot double-delete.
    BOOST_CHECK(result.result_string == nullptr);
    BOOST_CHECK(result.error_message == nullptr);
    HsBaFreeCustomPipelineResult(&result);
}

BOOST_AUTO_TEST_SUITE_END()
