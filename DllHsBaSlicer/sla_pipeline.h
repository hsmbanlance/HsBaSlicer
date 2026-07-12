#pragma once
#ifndef HSBA_SLICER_SLA_PIPELINE_H
#define HSBA_SLICER_SLA_PIPELINE_H

#include "dllexport.h"

#ifdef __cplusplus
extern "C"
{
#endif  // __cplusplus

    /**
     * @brief SLA support patterns (C-compatible enum).
     */
    typedef enum HsBaSlaSupportPattern
    {
        HSBA_SLA_SUPPORT_SACRIFICIAL = 0,  ///< Sacrificial (thin column) support
        HSBA_SLA_SUPPORT_CONE = 1          ///< Cone-shaped support
    } HsBaSlaSupportPattern_t;

    /**
     * @brief SLA layer image format (C-compatible enum).
     */
    typedef enum HsBaSlaImageType
    {
        HSBA_SLA_IMAGE_PNG = 0,  ///< PNG (default, lossless)
        HSBA_SLA_IMAGE_JPG = 1,  ///< JPEG (lossy, smaller file size)
        HSBA_SLA_IMAGE_SVG = 2   ///< SVG (vector, scalable)
    } HsBaSlaImageType_t;

    /**
     * @brief SLA pipeline configuration (C-compatible struct).
     *
     * Covers model, slicing, floor/raft, support, export, and Lua customisation.
     */
    typedef struct HsBaSlaPipelineConfig
    {
        /* Model Configuration */
        const char* model_name;  ///< Model name
        const char* model_path;  ///< Model file path

        /* Slice Configuration */
        float layer_height;        ///< Layer height (mm), default 0.05
        float first_layer_height;  ///< First layer height (mm), default 0.1

        /* Exposure Configuration */
        float bottom_exposure_time;  ///< Bottom layer exposure (s), default 60.0
        float normal_exposure_time;  ///< Normal layer exposure (s), default 2.5
        float bottom_lift_distance;  ///< Bottom layer lift distance (mm), default 5.0
        float lift_distance;         ///< Normal lift distance (mm), default 3.0
        float lift_speed;            ///< Lift speed (mm/s), default 60.0
        float retract_speed;         ///< Retract speed (mm/s), default 150.0

        /* Floor / Raft Configuration */
        float floor_raft_offset;     ///< Raft outward offset from footprint (mm), default 2.0
        float floor_border_width;    ///< Floor border ring width (mm), default 1.0
        float floor_fill_spacing;    ///< Floor fill line spacing (mm), default 0.5
        float floor_fill_angle;      ///< Floor fill angle (degrees), default 0.0
        int   floor_border_count;    ///< Number of floor border loops, default 2
        int   floor_use_convex_hull; ///< Use convex hull for floor (0=false, 1=true), default 0

        /* Support Configuration */
        int   enable_support;                    ///< Enable support (0=false, 1=true), default 1
        float overhang_angle;                    ///< Overhang angle threshold (degrees), default 45.0
        float support_gap;                       ///< Gap between support and model (mm), default 0.5
        float support_diameter;                  ///< Support column diameter (mm), default 2.0
        float support_density;                   ///< Support fill density [0,1], default 0.3
        HsBaSlaSupportPattern_t support_pattern; ///< Support pattern, default HSBA_SLA_SUPPORT_SACRIFICIAL

        /* Lua Custom Configuration */
        const char* support_lua_script;  ///< Support Lua script path (NULL = built-in)
        const char* support_lua_func;    ///< Support Lua function name (NULL = "generate_support")
        const char* floor_lua_script;    ///< Floor Lua script path (NULL = built-in)
        const char* floor_lua_func;      ///< Floor Lua function name (NULL = "generate_floor")
        const char* export_lua_script;   ///< Export Lua script path (NULL = built-in)
        const char* export_lua_func;     ///< Export Lua function name (NULL = "export_sla")

        /* Output Configuration */
        const char* output_path;       ///< Output zip file path (can be NULL)
        HsBaSlaImageType_t image_type; ///< Layer image format, default HSBA_SLA_IMAGE_PNG
        int   image_width;             ///< Image width in pixels (0 = auto), default 0
        int   image_height;            ///< Image height in pixels (0 = auto), default 0

    } HsBaSlaPipelineConfig_t;

    /**
     * @brief SLA pipeline result (C-compatible struct).
     *
     * Must call HsBaFreeSlaPipelineResult to release memory after use.
     */
    typedef struct HsBaSlaPipelineResult
    {
        int success;             ///< Success flag (0=false, 1=true)
        int total_layers;        ///< Total layer count
        char* export_path;       ///< Path to exported zip file (UTF-8, caller must free)
        char* error_message;     ///< Error message (UTF-8, caller must free)
        double elapsed_seconds;  ///< Elapsed time (seconds)
    } HsBaSlaPipelineResult_t;

    /**
     * @brief Progress callback function type.
     * @param percent Progress percentage (0-100).
     * @param stage Current stage description (UTF-8 string).
     * @param user_data User-defined data pointer.
     */
    typedef void (*HsBaSlaProgressCallback)(int percent, const char* stage, void* user_data);

    /**
     * @brief Result callback for async pipeline.
     */
    typedef void (*HsBaSlaResultCallback)(HsBaSlaPipelineResult_t result, void* user_data);

    /**
     * @brief Create SLA pipeline config with default values.
     * @return Default configuration struct.
     */
    HSBA_SLICER_API HsBaSlaPipelineConfig_t HsBaCreateDefaultSlaConfig(void);

    /**
     * @brief Run SLA full pipeline synchronously.
     *
     * Pipeline: Preprocess -> Slice -> Floor -> Support -> Export (zip)
     *
     * @param config Pipeline configuration.
     * @param callback Progress callback (can be NULL).
     * @param user_data Callback user data (can be NULL).
     * @return Pipeline result, call HsBaFreeSlaPipelineResult to release after use.
     */
    HSBA_SLICER_API HsBaSlaPipelineResult_t HsBaRunSlaPipeline(const HsBaSlaPipelineConfig_t* config,
                                                               HsBaSlaProgressCallback callback, void* user_data);

    /**
     * @brief Run SLA full pipeline asynchronously (non-blocking).
     *
     * @param config Pipeline configuration.
     * @param callback Progress callback (can be NULL).
     * @param user_data Callback user data (can be NULL).
     * @param result_callback Completion callback receiving result (must not be NULL).
     * @param result_user_data Result callback user data (can be NULL).
     */
    HSBA_SLICER_API void HsBaRunSlaPipelineAsync(const HsBaSlaPipelineConfig_t* config,
                                                  HsBaSlaProgressCallback callback, void* user_data,
                                                  HsBaSlaResultCallback result_callback,
                                                  void* result_user_data);

    /**
     * @brief Free memory allocated in SLA pipeline result.
     *
     * Must be called after HsBaSlaPipelineResult_t is no longer needed.
     *
     * @param result Result to free.
     */
    HSBA_SLICER_API void HsBaFreeSlaPipelineResult(HsBaSlaPipelineResult_t* result);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // !HSBA_SLICER_SLA_PIPELINE_H
