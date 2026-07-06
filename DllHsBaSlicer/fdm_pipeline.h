#pragma once
#ifndef HSBA_SLICER_FDM_PIPELINE_H
#define HSBA_SLICER_FDM_PIPELINE_H

#include "dllexport.h"

#ifdef __cplusplus
extern "C"
{
#endif  // __cplusplus

    /**
     * @brief FDM fill modes (C-compatible enum).
     */
    typedef enum HsBaFillMode
    {
        HSBA_FILL_LINE = 0,           ///< Parallel line fill
        HSBA_FILL_SIMPLE_ZIGZAG = 1,  ///< Simple zigzag fill
        HSBA_FILL_ZIGZAG = 2          ///< Advanced zigzag fill
    } HsBaFillMode_t;

    /**
     * @brief FDM support patterns (C-compatible enum).
     */
    typedef enum HsBaSupportPattern
    {
        HSBA_SUPPORT_PLANE = 0,     ///< Pillar support
        HSBA_SUPPORT_TREE = 1,      ///< Tree support
        HSBA_SUPPORT_HONEYCOMB = 2  ///< Honeycomb support
    } HsBaSupportPattern_t;

    /**
     * @brief FDM pipeline configuration (C-compatible struct).
     */
    typedef struct HsBaFdmPipelineConfig
    {
        /* Model Configuration */
        const char* model_name;  ///< Model name
        const char* model_path;  ///< Model file path

        /* Slice Configuration */
        float layer_height;        ///< Layer height (mm), default 0.2
        float first_layer_height;  ///< First layer height (mm), default 0.25

        /* Fill Configuration */
        double fill_spacing;       ///< Fill line spacing (mm), default 0.4
        HsBaFillMode_t fill_mode;  ///< Fill mode, default HSBA_FILL_ZIGZAG
        double fill_angle;         ///< Fill angle (degrees), default 45.0
        int wall_count;            ///< Wall perimeter count, default 3
        int top_layer_count;       ///< Top solid layer count, default 3
        int bottom_layer_count;    ///< Bottom solid layer count, default 3
        double infill_density;     ///< Infill density [0,1], default 0.2

        /* Support Configuration */
        int enable_support;                    ///< Enable support (0=false, 1=true), default 1
        float overhang_angle;                  ///< Overhang angle threshold (degrees), default 45.0
        float support_gap;                     ///< Gap between support and model (mm), default 0.5
        float support_diameter;                ///< Support column diameter (mm), default 2.0
        float support_density;                 ///< Support fill density [0,1], default 0.3
        HsBaSupportPattern_t support_pattern;  ///< Support pattern, default HSBA_SUPPORT_PLANE
        int interface_layers;                  ///< Interface layer count, default 2
        float interface_density;               ///< Interface layer density [0,1], default 0.5

        /* Path Configuration */
        float line_width;            ///< Line width (mm), default 0.4
        float print_speed;           ///< Print speed (mm/s), default 50.0
        float travel_speed;          ///< Travel speed (mm/s), default 100.0
        float extrusion_multiplier;  ///< Extrusion multiplier, default 1.0

        /* Lua Custom Configuration */
        const char* support_lua_script;   ///< Support Lua script path (NULL to use built-in algorithm)
        const char* support_lua_func;     ///< Support Lua function name (NULL, default "generate_support")
        const char* infill_lua_script;    ///< Infill Lua script path (NULL to use built-in algorithm)
        const char* infill_lua_func;      ///< Infill Lua function name (NULL, default "generate_fill")

        /* Output Configuration */
        const char* output_path;  ///< Output G-code file path (can be NULL)

    } HsBaFdmPipelineConfig_t;

    /**
     * @brief FDM pipeline result (C-compatible struct).
     *
     * Must call HsBaFreePipelineResult to release memory after use.
     */
    typedef struct HsBaFdmPipelineResult
    {
        int success;             ///< Success flag (0=false, 1=true)
        int total_layers;        ///< Total layer count
        char* gcode_content;     ///< G-code content (UTF-8, caller must free)
        char* error_message;     ///< Error message (UTF-8, caller must free)
        double elapsed_seconds;  ///< Elapsed time (seconds)
    } HsBaFdmPipelineResult_t;

    /**
     * @brief Progress callback function type.
     * @param percent Progress percentage (0-100).
     * @param stage Current stage description (UTF-8 string).
     * @param user_data User-defined data pointer.
     */
    typedef void (*HsBaProgressCallback)(int percent, const char* stage, void* user_data);

    /**
     * @brief Create FDM pipeline config with default values.
     * @return Default configuration struct.
     */
    HSBA_SLICER_API HsBaFdmPipelineConfig_t HsBaCreateDefaultConfig(void);

    /**
     * @brief Run FDM full pipeline synchronously.
     *
     * Uses C++20 coroutines internally, blocks until result is ready.
     * Pipeline: Preprocess -> Slice -> Support -> Fill -> Path Generation
     *
     * @param config Pipeline configuration.
     * @param callback Progress callback (can be NULL).
     * @param user_data Callback user data (can be NULL).
     * @return Pipeline result, call HsBaFreePipelineResult to release after use.
     */
    HSBA_SLICER_API HsBaFdmPipelineResult_t HsBaRunFdmPipeline(const HsBaFdmPipelineConfig_t* config,
                                                               HsBaProgressCallback callback, void* user_data);

    /**
     * @brief Run FDM full pipeline asynchronously (non-blocking).
     *
     * Uses C++20 coroutines for async execution, returns result via callback.
     *
     * @param config Pipeline configuration.
     * @param callback Progress callback (can be NULL).
     * @param user_data Callback user data (can be NULL).
     * @param result_callback Completion callback receiving result (must not be NULL).
     * @param result_user_data Result callback user data (can be NULL).
     * @return Task handle (reserved for future cancellation, etc.).
     */
    typedef void (*HsBaResultCallback)(HsBaFdmPipelineResult_t result, void* user_data);

    HSBA_SLICER_API void HsBaRunFdmPipelineAsync(const HsBaFdmPipelineConfig_t* config, HsBaProgressCallback callback,
                                                 void* user_data, HsBaResultCallback result_callback,
                                                 void* result_user_data);

    /**
     * @brief Free memory allocated in pipeline result.
     *
     * Must be called after HsBaFdmPipelineResult_t is no longer needed,
     * releases internally allocated memory for gcode_content and error_message.
     *
     * @param result Result to free.
     */
    HSBA_SLICER_API void HsBaFreePipelineResult(HsBaFdmPipelineResult_t* result);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // !HSBA_SLICER_FDM_PIPELINE_H
