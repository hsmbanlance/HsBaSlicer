/** @file param_store_ops.hpp
 * @brief Lib-layer wrapper: exposes the fileoperator ParamStore (wide-table process-parameter
 *        storage) as write/read operations over the PipelineConfig C structs.
 *
 * Follows the Lib/Dll/Module layering: the Dll layer calls only the free functions declared here and
 * never references fileoperator directly.
 *
 * The standard C configuration structs in pipelinetypes/pipeline_types.h are reused (passed in as
 * void* + kind and persisted/back-filled through AnyObject reflection); no functionally equivalent
 * config struct is redefined. const char* fields produced by Load are held by std::malloc and must
 * be released through FreeLoadedConfigStrings.
 */
#pragma once
#ifndef HSBA_SLICER_LIB_PARAM_STORE_OPS_HPP
#define HSBA_SLICER_LIB_PARAM_STORE_OPS_HPP

#include <string>
#include <string_view>

#include "../export.h"

namespace HsBa::Slicer
{
/** @brief Pipeline a parameter belongs to (ordered to match PipelineConfigTag / HsBaPipelineKind). */
enum class ParamPipelineKind
{
    Fdm,
    Sla,
    Sls,
    Slm,
    Lom,
    Tdp,
    Waam,
    Custom,
    FileTransfer,
};

/** @brief Storage backend; mobile targets support Sqlite only. */
enum class ParamBackend
{
    Sqlite,
    MySql,
    PostgreSql,
};

/** @brief Connection parameters: Sqlite uses sqlitePath; MySql/PostgreSql use host/port/user/password/database. */
struct ParamStoreConn
{
    ParamBackend backend = ParamBackend::Sqlite;
    std::string sqlitePath;
    std::string host;
    std::string user;
    std::string password;
    std::string database;
    unsigned port = 0;  ///< 0 means use the adapter's default port
};

/** @brief Result of one Save/Load; a non-empty error indicates failure (the error code is not swallowed). */
struct ParamStoreOutcome
{
    bool success = false;
    long long paramId = 0;
    std::string error;
};

/**
 * @brief Write (upsert) one PipelineConfig struct into the wide table.
 * @param conn  Database connection parameters.
 * @param kind  Configuration kind.
 * @param table Target table name; when empty a default name is derived from the type.
 * @param key   Business unique key.
 * @param cfg   Read-only pointer to the corresponding C config struct (e.g. HsBaFdmPipelineConfig_t).
 * @return Persistence result; paramId is the row id on success.
 */
HSBA_SLICER_LIB_API ParamStoreOutcome SavePipelineParams(const ParamStoreConn& conn, ParamPipelineKind kind,
                                                         std::string_view table, std::string_view key, const void* cfg);

/**
 * @brief Load one PipelineConfig by key and back-fill it into the caller-provided outCfg struct.
 *        The const char* fields in outCfg are allocated by std::malloc and must be released via
 *        FreeLoadedConfigStrings.
 * @param outCfg Writable pointer to the corresponding C config struct (should be initialized by *ConfigDefault() first).
 * @return success=true on a hit; on a miss or unavailable backend success=false and error is filled.
 */
HSBA_SLICER_LIB_API ParamStoreOutcome LoadPipelineParams(const ParamStoreConn& conn, ParamPipelineKind kind,
                                                         std::string_view table, std::string_view key, void* outCfg);

/** @brief Free the library-malloc'd const char* fields back-filled into cfg by LoadPipelineParams (nulling the pointers). */
HSBA_SLICER_LIB_API void FreeLoadedConfigStrings(ParamPipelineKind kind, void* cfg);

}  // namespace HsBa::Slicer

#endif  // HSBA_SLICER_LIB_PARAM_STORE_OPS_HPP
