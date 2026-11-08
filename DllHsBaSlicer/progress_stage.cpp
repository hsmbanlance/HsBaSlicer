/** @file progress_stage.cpp
 * @brief Stage-text classification table behind HsBaStageFromText / HsBaStageName.
 *
 * The pipeline sources report progress with free-form English text (see the
 * ReportProgress call sites in fdm/sla/sls/slm/lom/tdp/waam_pipeline.cpp). This
 * file is the single place that knows what those strings mean, so a wording change
 * in a pipeline only needs a new row here to keep GUI stage handling working.
 * @author HsBa
 */
#include "progress_stage.h"

#include <cstddef>
#include <string_view>

namespace
{

/** @brief One exact-match rule of the classification table. */
struct ExactRule
{
    std::string_view text;
    HsBaStage_t stage;
};

/** @brief One prefix rule, used for texts with a variable tail ("Exporting via Lua script..."). */
struct PrefixRule
{
    std::string_view prefix;
    HsBaStage_t stage;
};

// Exact texts currently emitted by the seven bed/robot pipelines.
constexpr ExactRule kExactRules[] = {
    {"Loading model", HSBA_STAGE_LOAD_MODEL},
    {"Model loaded", HSBA_STAGE_MODEL_LOADED},
    {"Slicing", HSBA_STAGE_SLICING},
    {"Slicing layer", HSBA_STAGE_SLICING_LAYER},
    {"Slicing bead layer", HSBA_STAGE_SLICING_LAYER},
    {"Slicing sheet", HSBA_STAGE_SLICING_LAYER},
    {"Slicing complete", HSBA_STAGE_SLICE_DONE},
    {"Generating floor", HSBA_STAGE_FLOOR},
    {"Floor generation complete", HSBA_STAGE_FLOOR_DONE},
    {"Generating supports", HSBA_STAGE_SUPPORT},
    {"Support generation complete", HSBA_STAGE_SUPPORT_DONE},
    {"Support disabled", HSBA_STAGE_SUPPORT_DISABLED},
    {"Generating fills", HSBA_STAGE_FILL},
    {"Filling layer", HSBA_STAGE_FILLING_LAYER},
    {"Fill generation complete", HSBA_STAGE_FILL_DONE},
    {"Fill skipped", HSBA_STAGE_FILL_DONE},
    {"Generating G-code paths", HSBA_STAGE_PATH},
    {"Generating robot path", HSBA_STAGE_PATH},
    {"Writing G-code file", HSBA_STAGE_WRITE_FILE},
    {"Exporting", HSBA_STAGE_EXPORT},
    {"Pipeline complete", HSBA_STAGE_DONE},
    {"Cancelled", HSBA_STAGE_CANCELLED},
};

// Prefix rules catch the wordings whose tail varies with the process or the model.
constexpr PrefixRule kPrefixRules[] = {
    {"Slicing bead", HSBA_STAGE_SLICING_LAYER},  {"Slicing sheet", HSBA_STAGE_SLICING_LAYER},
    {"Slicing layer", HSBA_STAGE_SLICING_LAYER}, {"Exporting", HSBA_STAGE_EXPORT},
    {"Writing ", HSBA_STAGE_WRITE_FILE},         {"Support disabled", HSBA_STAGE_SUPPORT_DISABLED},
    {"Loading model", HSBA_STAGE_LOAD_MODEL},    {"Pipeline complete", HSBA_STAGE_DONE},
    {"Cancelled", HSBA_STAGE_CANCELLED},
};

// Stable machine identifiers: never renumber, never rename -- these are API surface.
constexpr const char* kStageNames[] = {
    "unknown",         "model.loading", "model.loaded", "slicing.start", "slicing.layer",
    "slicing.done",    "floor.start",   "floor.done",   "support.start", "support.done",
    "support.skipped", "fill.start",    "fill.layer",   "fill.done",     "path.start",
    "file.write",      "export",        "done",         "cancelled",
};

constexpr const char* kStageLabels[] = {
    "Unknown",
    "Loading model",
    "Model loaded",
    "Slicing",
    "Slicing layer",
    "Slicing complete",
    "Generating floor",
    "Floor complete",
    "Generating supports",
    "Supports complete",
    "Supports skipped",
    "Generating fills",
    "Filling layer",
    "Fills complete",
    "Generating tool path",
    "Writing output file",
    "Exporting",
    "Complete",
    "Cancelled",
};

/**
 * @brief Reduce a reported stage text to its canonical stem, without allocating.
 *
 * Drops the trailing "..." ellipsis (and any stray dots/spaces), then cuts a
 * parenthetical qualifier so that "Support disabled (spiral mode)" and
 * "Support disabled" collapse onto the same stage. Only right-truncation is used,
 * so the returned view keeps pointing into the caller's buffer.
 */
std::string_view Normalize(std::string_view text)
{
    while (!text.empty() && (text.back() == '.' || text.back() == ' '))
    {
        text.remove_suffix(1);
    }
    const std::size_t paren = text.find(" (");
    if (paren != std::string_view::npos)
    {
        text = text.substr(0, paren);
    }
    return text;
}

}  // namespace

HSBA_SLICER_API HsBaStage_t HsBaStageFromText(const char* stage)
{
    if (!stage)
    {
        return HSBA_STAGE_UNKNOWN;
    }
    const std::string_view text = Normalize(std::string_view(stage));
    if (text.empty())
    {
        return HSBA_STAGE_UNKNOWN;
    }
    for (const auto& rule : kExactRules)
    {
        if (rule.text == text)
        {
            return rule.stage;
        }
    }
    for (const auto& rule : kPrefixRules)
    {
        if (text.size() >= rule.prefix.size() && text.compare(0, rule.prefix.size(), rule.prefix) == 0)
        {
            return rule.stage;
        }
    }
    return HSBA_STAGE_UNKNOWN;
}

HSBA_SLICER_API const char* HsBaStageName(HsBaStage_t stage)
{
    const int index = static_cast<int>(stage);
    constexpr int count = static_cast<int>(sizeof(kStageNames) / sizeof(kStageNames[0]));
    if (index < 0 || index >= count)
    {
        return kStageNames[0];
    }
    return kStageNames[index];
}

HSBA_SLICER_API const char* HsBaStageLabel(HsBaStage_t stage)
{
    const int index = static_cast<int>(stage);
    constexpr int count = static_cast<int>(sizeof(kStageLabels) / sizeof(kStageLabels[0]));
    if (index < 0 || index >= count)
    {
        return kStageLabels[0];
    }
    return kStageLabels[index];
}
