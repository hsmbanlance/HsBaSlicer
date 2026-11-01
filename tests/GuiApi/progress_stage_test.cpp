/** @file progress_stage_test.cpp
 * @brief Stage-text classification contract of DllHsBaSlicer/progress_stage.h.
 *
 * Every string in the exact-match table is a real ReportProgress call site of one of the
 * seven bed/robot pipelines, so this suite is the regression guard for "a GUI keeps
 * classifying the stages it already knew after a pipeline reworded another one".
 * @author HsBa
 */
#include <string>
#include <vector>

#define BOOST_TEST_MODULE GuiApiProgressStageTest
#include <boost/test/included/unit_test.hpp>

#include "DllHsBaSlicer/progress_stage.h"

namespace
{
/** @brief One reported stage text together with the stage a GUI must see. */
struct StageCase
{
    const char* text;
    HsBaStage_t stage;
};

/** @brief The texts currently emitted by fdm/sla/sls/slm/lom/tdp/waam_pipeline.cpp. */
const std::vector<StageCase>& EmittedTexts()
{
    static const std::vector<StageCase> cases = {
        {"Loading model...", HSBA_STAGE_LOAD_MODEL},
        {"Model loaded", HSBA_STAGE_MODEL_LOADED},
        {"Slicing...", HSBA_STAGE_SLICING},
        {"Slicing layer", HSBA_STAGE_SLICING_LAYER},
        {"Slicing sheet", HSBA_STAGE_SLICING_LAYER},
        {"Slicing bead layer", HSBA_STAGE_SLICING_LAYER},
        {"Slicing complete", HSBA_STAGE_SLICE_DONE},
        {"Generating floor...", HSBA_STAGE_FLOOR},
        {"Floor generation complete", HSBA_STAGE_FLOOR_DONE},
        {"Generating supports...", HSBA_STAGE_SUPPORT},
        {"Support generation complete", HSBA_STAGE_SUPPORT_DONE},
        {"Support disabled", HSBA_STAGE_SUPPORT_DISABLED},
        {"Support disabled (spiral mode)", HSBA_STAGE_SUPPORT_DISABLED},
        {"Generating fills...", HSBA_STAGE_FILL},
        {"Filling layer", HSBA_STAGE_FILLING_LAYER},
        {"Fill generation complete", HSBA_STAGE_FILL_DONE},
        {"Fill skipped (spiral mode)", HSBA_STAGE_FILL_DONE},
        {"Generating G-code paths...", HSBA_STAGE_PATH},
        {"Generating robot path...", HSBA_STAGE_PATH},
        {"Writing G-code file...", HSBA_STAGE_WRITE_FILE},
        {"Exporting...", HSBA_STAGE_EXPORT},
        {"Exporting via Lua script...", HSBA_STAGE_EXPORT},
        {"Pipeline complete", HSBA_STAGE_DONE},
        {"Cancelled", HSBA_STAGE_CANCELLED},
    };
    return cases;
}
}  // namespace

BOOST_AUTO_TEST_SUITE(gui_progress_stage)

BOOST_AUTO_TEST_CASE(every_emitted_text_classifies)
{
    for (const StageCase& item : EmittedTexts())
    {
        BOOST_CHECK_MESSAGE(HsBaStageFromText(item.text) == item.stage,
                            std::string("text '") + item.text + "' did not classify");
    }
}

BOOST_AUTO_TEST_CASE(unknown_text_and_null_fall_back)
{
    // A custom (Lua) pipeline reports free-form text, which must never be guessed.
    BOOST_CHECK_EQUAL(HsBaStageFromText(nullptr), HSBA_STAGE_UNKNOWN);
    BOOST_CHECK_EQUAL(HsBaStageFromText(""), HSBA_STAGE_UNKNOWN);
    BOOST_CHECK_EQUAL(HsBaStageFromText("..."), HSBA_STAGE_UNKNOWN);
    BOOST_CHECK_EQUAL(HsBaStageFromText("Bunny says hello"), HSBA_STAGE_UNKNOWN);
    BOOST_CHECK_EQUAL(std::string(HsBaStageName(HSBA_STAGE_UNKNOWN)), "unknown");
    BOOST_CHECK_EQUAL(std::string(HsBaStageLabel(HSBA_STAGE_UNKNOWN)), "Unknown");
}

BOOST_AUTO_TEST_CASE(stable_names_cover_every_stage)
{
    // The names are API surface: a GUI styles and translates on them, so both the
    // count and the spelling of each entry are pinned here.
    const char* const expected[] = {"unknown",       "model.loading", "model.loaded",    "slicing.start",
                                    "slicing.layer", "slicing.done",  "floor.start",     "floor.done",
                                    "support.start", "support.done",  "support.skipped", "fill.start",
                                    "fill.layer",    "fill.done",     "path.start",      "file.write",
                                    "export",        "done",          "cancelled"};
    constexpr int count = static_cast<int>(sizeof(expected) / sizeof(expected[0]));
    for (int i = 0; i < count; ++i)
    {
        const auto stage = static_cast<HsBaStage_t>(i);
        BOOST_CHECK_EQUAL(std::string(HsBaStageName(stage)), expected[i]);
        BOOST_CHECK(*HsBaStageLabel(stage) != '\0');
    }
    // Enumerators are contiguous from UNKNOWN up to CANCELLED.
    BOOST_CHECK_EQUAL(static_cast<int>(HSBA_STAGE_CANCELLED), count - 1);
    // Out-of-range values must not index past the table.
    BOOST_CHECK_EQUAL(std::string(HsBaStageName(static_cast<HsBaStage_t>(-1))), "unknown");
    BOOST_CHECK_EQUAL(std::string(HsBaStageName(static_cast<HsBaStage_t>(999))), "unknown");
    BOOST_CHECK_EQUAL(std::string(HsBaStageLabel(static_cast<HsBaStage_t>(999))), "Unknown");
}

BOOST_AUTO_TEST_CASE(classified_text_survives_trailing_punctuation)
{
    // The normalization only right-trims dots and cuts a trailing parenthesis, so a
    // reworded prefix is a mapping-table change and never a silent UNKNOWN.
    BOOST_CHECK_EQUAL(HsBaStageFromText("Slicing layer   "), HSBA_STAGE_SLICING_LAYER);
    BOOST_CHECK_EQUAL(HsBaStageFromText("Pipeline complete."), HSBA_STAGE_DONE);
    BOOST_CHECK_EQUAL(HsBaStageFromText("Cancelled: pipeline cancelled"), HSBA_STAGE_CANCELLED);
    BOOST_CHECK_EQUAL(HsBaStageFromText("Loading model (first try)"), HSBA_STAGE_LOAD_MODEL);
    // A text that merely starts with a known word but is not one of the ruled prefixes
    // is still unknown: "Slic" is not "Slicing".
    BOOST_CHECK_EQUAL(HsBaStageFromText("Sliced bread"), HSBA_STAGE_UNKNOWN);
}

BOOST_AUTO_TEST_SUITE_END()
