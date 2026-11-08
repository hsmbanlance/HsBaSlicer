/** @file progress_stage.h
 * @brief C ABI for translating pipeline progress text into stable stage identifiers.
 *
 * The existing HsBaProgressCallback hands the host a human readable stage string.
 * GUIs that need to colour a progress bar, build a stage timeline or translate the
 * label cannot rely on that text staying byte-identical across releases, so the
 * library maps it onto the HsBaStage_t enumeration exposed here. The mapping never
 * fails: unknown text becomes HSBA_STAGE_UNKNOWN and its stable name is "unknown".
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_PROGRESS_STAGE_H
#define HSBA_SLICER_PROGRESS_STAGE_H

#include "dllexport.h"
#include "pipelinetypes/pipeline_types.h"

#ifdef __cplusplus
extern "C"
{
#endif  // __cplusplus

    /**
     * @brief Classify a progress stage string reported by a pipeline.
     * @param stage Stage text (UTF-8, may be NULL).
     * @return Matching HsBaStage_t value, HSBA_STAGE_UNKNOWN when nothing matches.
     */
    HSBA_SLICER_API HsBaStage_t HsBaStageFromText(const char* stage);

    /**
     * @brief Stable snake_case identifier of a stage, suitable for styling and i18n keys.
     * @param stage Stage enumerator.
     * @return Static UTF-8 string owned by the library ("unknown" for unclassified values).
     */
    HSBA_SLICER_API const char* HsBaStageName(HsBaStage_t stage);

    /**
     * @brief Localized-independent display label of a stage in English.
     * @param stage Stage enumerator.
     * @return Static UTF-8 string ("Unknown" for unclassified values).
     */
    HSBA_SLICER_API const char* HsBaStageLabel(HsBaStage_t stage);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // !HSBA_SLICER_PROGRESS_STAGE_H
