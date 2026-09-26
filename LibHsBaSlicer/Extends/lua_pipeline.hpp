#pragma once
#ifndef HSBA_SLICER_LIB_LUA_PIPELINE_HPP
#define HSBA_SLICER_LIB_LUA_PIPELINE_HPP

#include <functional>
#include <string>
#include <string_view>

#include "LibHsBaSlicer/export.h"

struct lua_State;

namespace HsBa::Slicer
{
/**
 * @brief Context for a fully Lua-driven custom pipeline run.
 *
 * Unlike the FDM/SLA/SLS pipelines (whose stage order is fixed in C++ with
 * optional per-stage Lua customization), the custom pipeline delegates the
 * *entire* workflow to a Lua entry function. The C++ side only builds a Lua
 * environment that exposes every pipeline building block through the global
 * `HsBa` table (model loading, slicing, support, fill, floor, path output,
 * packaging), and the Lua script decides which blocks to run and in which order.
 */
struct LuaPipelineContext
{
    std::string script;                    ///< Inline Lua source, executed first as a prelude (may be empty)
    std::string script_file;               ///< Path to the Lua script file (may be empty)
    std::string entry_func = "run_pipeline"; ///< Name of the Lua entry function to call
    std::string config_json;               ///< Free-form JSON string exposed as `pipeline_config`
    std::string output_path;               ///< Default output path exposed as `output_path`
    std::string model_name;                ///< Model name exposed as `model_name`
    std::string model_path;                ///< Model file path exposed as `model_path`
    std::function<void(int, std::string_view)> progress_cb; ///< Progress reporting hook
};

/**
 * @brief Output collected from a custom pipeline run.
 */
struct LuaPipelineOutput
{
    bool success = false;
    int total_layers = 0;       ///< Value set by the script via `HsBa.setLayers()`
    std::string output_path;    ///< Value set by the script via `HsBa.setOutputPath()`
    std::string result_string;  ///< String returned by the Lua entry function
    std::string error_message;  ///< Lua error / exception message on failure
};

/**
 * @brief Register the custom-pipeline Lua environment into an existing Lua state.
 *
 * Opens the standard libraries, installs the common AnyObject types and the
 * pooled 2D/3D/File registration functions (PolygonOperations, Support,
 * PolygonFill, PathOptimize, Zipper, Cipher, SQLite/MySQL/PostgreSQL adapters),
 * and creates the global `HsBa` operations table bound to @p ctx.
 *
 * @param L   Lua state (freshly created, not yet initialized).
 * @param ctx Pipeline context; must outlive the Lua state.
 * @param out Optional output object the script's progress writes are mirrored
 *            into (`setLayers` / `setOutputPath`).
 * @param run_state Optional caller-owned storage for the internal run state;
 *            it must outlive the Lua state. When null, a static fallback is
 *            used (convenient for single-threaded standalone embedding).
 */
HSBA_SLICER_LIB_API void SetupLuaPipelineEnvironment(lua_State* L, LuaPipelineContext& ctx, LuaPipelineOutput* out,
                                                     void* run_state = nullptr);

/**
 * @brief Run a fully Lua-driven pipeline.
 *
 * Creates a Lua state, sets up the environment (see
 * `SetupLuaPipelineEnvironment`), executes `ctx.script` / `ctx.script_file`,
 * then calls the entry function `ctx.entry_func`. The entry function may return
 * a string, which is reported back through `LuaPipelineOutput::result_string`.
 *
 * @param ctx Pipeline context.
 * @return Run result; on failure `error_message` carries the Lua error.
 */
HSBA_SLICER_LIB_API LuaPipelineOutput RunLuaPipeline(const LuaPipelineContext& ctx);

}  // namespace HsBa::Slicer

#endif  // !HSBA_SLICER_LIB_LUA_PIPELINE_HPP
