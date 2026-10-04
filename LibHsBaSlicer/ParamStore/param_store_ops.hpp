/** @file param_store_ops.hpp
 * @brief Lib 层封装：把 fileoperator 的 ParamStore（工艺参数宽表存储）暴露为面向
 *        PipelineConfig C 结构体的写入/读取操作。遵循 Lib/Dll/Module 分层：Dll 层只调用
 *        本文件的自由函数，不直接引用 fileoperator。
 *
 * 复用 pipelinetypes/pipeline_types.h 中的标准 C 配置结构体（以 void* + kind 传入，经
 * AnyObject 反射落库/回填），不重新定义功能等价的配置结构体（memory: 模块配置结构体复用
 * 强制规范）。读取（Load）得到的 const char* 字段由 std::malloc 持有，须由
 * FreeLoadedConfigStrings 释放。
 */
#pragma once
#ifndef HSBA_SLICER_LIB_PARAM_STORE_OPS_HPP
#define HSBA_SLICER_LIB_PARAM_STORE_OPS_HPP

#include <string>
#include <string_view>

#include "../export.h"

namespace HsBa::Slicer
{
/** @brief 参数所属的流水线类型（顺序与 PipelineConfigTag / HsBaPipelineKind 对齐）。 */
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

/** @brief 存储后端；移动端仅支持 Sqlite。 */
enum class ParamBackend
{
    Sqlite,
    MySql,
    PostgreSql,
};

/** @brief 连接参数：Sqlite 用 sqlitePath；MySql/PostgreSql 用 host/port/user/password/database。 */
struct ParamStoreConn
{
    ParamBackend backend = ParamBackend::Sqlite;
    std::string sqlitePath;
    std::string host;
    std::string user;
    std::string password;
    std::string database;
    unsigned port = 0;  ///< 0 表示使用适配器默认端口
};

/** @brief 一次 Save/Load 的结果；error 非空表示失败（不吞错误码）。 */
struct ParamStoreOutcome
{
    bool success = false;
    long long paramId = 0;
    std::string error;
};

/**
 * @brief 写入（upsert）一个 PipelineConfig 结构体到宽表。
 * @param conn  数据库连接参数。
 * @param kind  配置类型。
 * @param table 目标表名；空则由类型派生默认表名。
 * @param key   业务唯一键。
 * @param cfg   指向对应 C 配置结构体（如 HsBaFdmPipelineConfig_t）的只读指针。
 * @return 落库结果，成功时 paramId 为该行 id。
 */
HSBA_SLICER_LIB_API ParamStoreOutcome SavePipelineParams(const ParamStoreConn& conn, ParamPipelineKind kind,
                                                         std::string_view table, std::string_view key, const void* cfg);

/**
 * @brief 按 key 读取一个 PipelineConfig，回填到调用方提供的 outCfg 结构体。
 *        outCfg 中的 const char* 字段由 std::malloc 分配，须由 FreeLoadedConfigStrings 释放。
 * @param outCfg 指向对应 C 配置结构体的可写指针（应先由 *ConfigDefault() 初始化）。
 * @return 命中时 success=true；未命中或后端不可用时 success=false 并填充 error。
 */
HSBA_SLICER_LIB_API ParamStoreOutcome LoadPipelineParams(const ParamStoreConn& conn, ParamPipelineKind kind,
                                                         std::string_view table, std::string_view key, void* outCfg);

/** @brief 释放 LoadPipelineParams 回填到 cfg 的、由库 malloc 的 const char* 字段（置空指针）。 */
HSBA_SLICER_LIB_API void FreeLoadedConfigStrings(ParamPipelineKind kind, void* cfg);

}  // namespace HsBa::Slicer

#endif  // HSBA_SLICER_LIB_PARAM_STORE_OPS_HPP
