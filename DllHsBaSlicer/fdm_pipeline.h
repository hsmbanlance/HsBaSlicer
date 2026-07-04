#pragma once
#ifndef HSBA_SLICER_FDM_PIPELINE_H
#define HSBA_SLICER_FDM_PIPELINE_H

#include "dllexport.h"

#ifdef __cplusplus
extern "C"
{
#endif  // __cplusplus

    /**
     * @brief FDM填充模式（C兼容枚举）
     */
    typedef enum HsBaFillMode
    {
        HSBA_FILL_LINE = 0,           ///< 平行线填充
        HSBA_FILL_SIMPLE_ZIGZAG = 1,  ///< 简单锯齿填充
        HSBA_FILL_ZIGZAG = 2          ///< 高级锯齿填充
    } HsBaFillMode_t;

    /**
     * @brief FDM支撑模式（C兼容枚举）
     */
    typedef enum HsBaSupportPattern
    {
        HSBA_SUPPORT_PLANE = 0,     ///< 柱状支撑
        HSBA_SUPPORT_TREE = 1,      ///< 树状支撑
        HSBA_SUPPORT_HONEYCOMB = 2  ///< 蜂窝支撑
    } HsBaSupportPattern_t;

    /**
     * @brief FDM流水线配置（C兼容结构体）
     */
    typedef struct HsBaFdmPipelineConfig
    {
        /* 模型配置 */
        const char* model_name;  ///< 模型名称
        const char* model_path;  ///< 模型文件路径

        /* 切片配置 */
        float layer_height;        ///< 层高（mm），默认0.2
        float first_layer_height;  ///< 首层层高（mm），默认0.25

        /* 填充配置 */
        double fill_spacing;       ///< 填充间距（mm），默认0.4
        HsBaFillMode_t fill_mode;  ///< 填充模式，默认HSBA_FILL_ZIGZAG
        double fill_angle;         ///< 填充角度（度），默认45.0
        int wall_count;            ///< 壁厚圈数，默认3

        /* 支撑配置 */
        int enable_support;                    ///< 是否启用支撑（0=false, 1=true），默认1
        float overhang_angle;                  ///< 悬垂角度阈值（度），默认45.0
        float support_gap;                     ///< 支撑与模型间距（mm），默认0.5
        float support_diameter;                ///< 支撑柱直径（mm），默认2.0
        float support_density;                 ///< 支撑填充密度[0,1]，默认0.3
        HsBaSupportPattern_t support_pattern;  ///< 支撑模式，默认HSBA_SUPPORT_PLANE
        int interface_layers;                  ///< 接口层数，默认2
        float interface_density;               ///< 接口层密度[0,1]，默认0.5

        /* 路径配置 */
        float line_width;            ///< 线宽（mm），默认0.4
        float print_speed;           ///< 打印速度（mm/s），默认50.0
        float travel_speed;          ///< 空走速度（mm/s），默认100.0
        float extrusion_multiplier;  ///< 挤出量倍率，默认1.0

        /* 输出配置 */
        const char* output_path;  ///< 输出G-code文件路径（可为NULL）

    } HsBaFdmPipelineConfig_t;

    /**
     * @brief FDM流水线结果（C兼容结构体）
     *
     * 使用完毕后必须调用 HsBaFreePipelineResult 释放内存。
     */
    typedef struct HsBaFdmPipelineResult
    {
        int success;             ///< 是否成功（0=false, 1=true）
        int total_layers;        ///< 总层数
        char* gcode_content;     ///< G-code内容（UTF-8，需调用者释放）
        char* error_message;     ///< 错误信息（UTF-8，需调用者释放）
        double elapsed_seconds;  ///< 耗时（秒）
    } HsBaFdmPipelineResult_t;

    /**
     * @brief 进度回调函数类型。
     * @param percent 进度百分比（0-100）。
     * @param stage 当前阶段描述（UTF-8字符串）。
     * @param user_data 用户自定义数据指针。
     */
    typedef void (*HsBaProgressCallback)(int percent, const char* stage, void* user_data);

    /**
     * @brief 创建带默认值的FDM流水线配置。
     * @return 默认配置结构体。
     */
    HSBA_SLICER_API HsBaFdmPipelineConfig_t HsBaCreateDefaultConfig(void);

    /**
     * @brief 同步运行FDM全流程流水线。
     *
     * 内部使用C++20协程优化，对外阻塞返回结果。
     * 流程：预处理 -> 切片 -> 支撑 -> 填充 -> 路径生成
     *
     * @param config 流水线配置。
     * @param callback 进度回调（可为NULL）。
     * @param user_data 回调用户数据（可为NULL）。
     * @return 流水线结果，使用完毕后调用 HsBaFreePipelineResult 释放。
     */
    HSBA_SLICER_API HsBaFdmPipelineResult_t HsBaRunFdmPipeline(const HsBaFdmPipelineConfig_t* config,
                                                               HsBaProgressCallback callback, void* user_data);

    /**
     * @brief 异步运行FDM全流程流水线（非阻塞）。
     *
     * 内部使用C++20协程异步执行，通过回调返回结果。
     *
     * @param config 流水线配置。
     * @param callback 进度回调（可为NULL）。
     * @param user_data 回调用户数据（可为NULL）。
     * @param result_callback 完成回调，接收结果（不可为NULL）。
     * @param result_user_data 结果回调用户数据（可为NULL）。
     * @return 任务句柄（当前保留，未来用于取消等操作）。
     */
    typedef void (*HsBaResultCallback)(HsBaFdmPipelineResult_t result, void* user_data);

    HSBA_SLICER_API void HsBaRunFdmPipelineAsync(const HsBaFdmPipelineConfig_t* config, HsBaProgressCallback callback,
                                                 void* user_data, HsBaResultCallback result_callback,
                                                 void* result_user_data);

    /**
     * @brief 释放流水线结果中分配的内存。
     *
     * 必须在使用完 HsBaFdmPipelineResult_t 后调用，
     * 释放 gcode_content 和 error_message 的内部分配内存。
     *
     * @param result 需要释放的结果。
     */
    HSBA_SLICER_API void HsBaFreePipelineResult(HsBaFdmPipelineResult_t* result);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // !HSBA_SLICER_FDM_PIPELINE_H
