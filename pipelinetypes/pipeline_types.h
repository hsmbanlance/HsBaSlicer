#pragma once
#ifndef HSBA_SLICER_PIPELINE_TYPES_H
#define HSBA_SLICER_PIPELINE_TYPES_H

/**
 * @file pipeline_types.h
 * @brief Standalone C-compatible type definitions for all pipeline configs/results.
 *
 * This header is intentionally free of HSBA_SLICER_API / dllexport.h so that
 * downstream modules (e.g. convert) can use the struct definitions without
 * depending on the DllHsBaSlicer shared library.
 */

#ifdef __cplusplus
extern "C"
{
#endif  // __cplusplus

    /* ========================================================================
     *  FDM Types
     * ====================================================================== */

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
     * @brief GCode output firmware target (C-compatible enum).
     */
    typedef enum HsBaGCodeFirmware
    {
        HSBA_GCODE_MARLIN = 0,   ///< Marlin firmware (most common)
        HSBA_GCODE_REPRAP = 1,   ///< RepRap / RRF firmware
        HSBA_GCODE_KLIPPER = 2   ///< Klipper firmware
    } HsBaGCodeFirmware_t;

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

        /* GCode Output Configuration */
        HsBaGCodeFirmware_t gcode_firmware;  ///< Target firmware, default HSBA_GCODE_MARLIN
        float nozzle_diameter;               ///< Nozzle diameter (mm), default 0.4
        float filament_diameter;             ///< Filament diameter (mm), default 1.75
        float nozzle_temp;                   ///< Nozzle temperature (C), default 200.0
        float bed_temp;                      ///< Bed temperature (C), default 60.0
        float retract_length;                ///< Retraction length (mm), default 1.0
        float retract_speed;                 ///< Retraction speed (mm/s), default 40.0
        float first_layer_speed;             ///< First layer speed (mm/s), default 20.0

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
     * @brief Result callback for async FDM pipeline.
     */
    typedef void (*HsBaResultCallback)(HsBaFdmPipelineResult_t result, void* user_data);

    /* ========================================================================
     *  SLA Types
     * ====================================================================== */

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
     * @brief SLA progress callback function type.
     * @param percent Progress percentage (0-100).
     * @param stage Current stage description (UTF-8 string).
     * @param user_data User-defined data pointer.
     */
    typedef void (*HsBaSlaProgressCallback)(int percent, const char* stage, void* user_data);

    /**
     * @brief Result callback for async SLA pipeline.
     */
    typedef void (*HsBaSlaResultCallback)(HsBaSlaPipelineResult_t result, void* user_data);

    /* ========================================================================
     *  SLS Types
     * ====================================================================== */

    /**
     * @brief SLS pipeline configuration (C-compatible struct).
     *
     * SLS (Selective Laser Sintering) uses a powder bed process.
     * No floor/raft or support structures are needed; the powder bed
     * provides both support and thermal stability.
     * Output format is entirely determined by the Lua export script.
     */
    typedef struct HsBaSlsPipelineConfig
    {
        /* Model Configuration */
        const char* model_name;  ///< Model name
        const char* model_path;  ///< Model file path

        /* Slice Configuration */
        float layer_height;        ///< Layer height (mm), default 0.1
        float first_layer_height;  ///< First layer height (mm), default 0.15

        /* Laser Configuration */
        float laser_power;         ///< Laser power (W), default 30.0
        float scan_speed;          ///< Scan speed (mm/s), default 2000.0
        float hatch_spacing;       ///< Hatch line spacing (mm), default 0.15
        float hatch_rotation;      ///< Hatch rotation between layers (degrees), default 90.0
        float bed_temperature;     ///< Powder bed temperature (°C), default 180.0

        /* Lua Export Configuration (required - no standard output format) */
        const char* export_lua_script;  ///< Export Lua script path (must not be NULL)
        const char* export_lua_func;    ///< Export Lua function name (NULL = "export_sls")

        /* Output Configuration */
        const char* output_path;  ///< Output file path (can be NULL)

    } HsBaSlsPipelineConfig_t;

    /**
     * @brief SLS pipeline result (C-compatible struct).
     *
     * Must call HsBaFreeSlsPipelineResult to release memory after use.
     */
    typedef struct HsBaSlsPipelineResult
    {
        int success;             ///< Success flag (0=false, 1=true)
        int total_layers;        ///< Total layer count
        char* export_path;       ///< Path to exported output file (UTF-8, caller must free)
        char* error_message;     ///< Error message (UTF-8, caller must free)
        double elapsed_seconds;  ///< Elapsed time (seconds)
    } HsBaSlsPipelineResult_t;

    /**
     * @brief SLS progress callback function type.
     * @param percent Progress percentage (0-100).
     * @param stage Current stage description (UTF-8 string).
     * @param user_data User-defined data pointer.
     */
    typedef void (*HsBaSlsProgressCallback)(int percent, const char* stage, void* user_data);

    /**
     * @brief Result callback for async SLS pipeline.
     */
    typedef void (*HsBaSlsResultCallback)(HsBaSlsPipelineResult_t result, void* user_data);

    /* ========================================================================
     *  File Transfer Types
     * ====================================================================== */

    /**
     * @brief File transfer pipeline configuration (C-compatible struct).
     *
     * Uses RemoteExecutorConnectionPool to send files to a remote service.
     */
    typedef struct HsBaFileTransferPipelineConfig
    {
        /* Connection Configuration */
        const char* host;       ///< Remote host address (must not be NULL)
        const char* port;       ///< Remote service port (must not be NULL)
        int pool_size;          ///< Connection pool size [1,16], default 4

        /* File Configuration */
        const char** file_paths;  ///< Array of file paths to transfer (must not be NULL)
        int file_count;           ///< Number of files in file_paths array

    } HsBaFileTransferPipelineConfig_t;

    /**
     * @brief File transfer pipeline result (C-compatible struct).
     *
     * Must call HsBaFreeFileTransferPipelineResult to release memory after use.
     */
    typedef struct HsBaFileTransferPipelineResult
    {
        int success;              ///< Success flag (0=false, 1=true)
        int files_transferred;    ///< Number of files successfully transferred
        int total_files;          ///< Total number of files requested
        char* error_message;      ///< Error message (UTF-8, caller must free)
        double elapsed_seconds;   ///< Elapsed time (seconds)
    } HsBaFileTransferPipelineResult_t;

    /**
     * @brief File transfer progress callback function type.
     * @param percent Progress percentage (0-100).
     * @param stage Current stage description (UTF-8 string).
     * @param user_data User-defined data pointer.
     */
    typedef void (*HsBaFileTransferProgressCallback)(int percent, const char* stage, void* user_data);

    /**
     * @brief Result callback for async file transfer pipeline.
     */
    typedef void (*HsBaFileTransferResultCallback)(HsBaFileTransferPipelineResult_t result, void* user_data);

    /* ========================================================================
     *  Default config initializers (inline, no DLL dependency)
     * ====================================================================== */

    /**
     * @brief Initialize FDM pipeline config with default values.
     * @return Default configuration struct (string fields are NULL).
     */
    static inline HsBaFdmPipelineConfig_t HsBaFdmConfigDefault(void)
    {
        HsBaFdmPipelineConfig_t cfg;
        cfg.model_name = 0;
        cfg.model_path = 0;
        cfg.layer_height = 0.2f;
        cfg.first_layer_height = 0.25f;
        cfg.fill_spacing = 0.4;
        cfg.fill_mode = HSBA_FILL_ZIGZAG;
        cfg.fill_angle = 45.0;
        cfg.wall_count = 3;
        cfg.top_layer_count = 3;
        cfg.bottom_layer_count = 3;
        cfg.infill_density = 0.2;
        cfg.enable_support = 1;
        cfg.overhang_angle = 45.0f;
        cfg.support_gap = 0.5f;
        cfg.support_diameter = 2.0f;
        cfg.support_density = 0.3f;
        cfg.support_pattern = HSBA_SUPPORT_PLANE;
        cfg.interface_layers = 2;
        cfg.interface_density = 0.5f;
        cfg.line_width = 0.4f;
        cfg.print_speed = 50.0f;
        cfg.travel_speed = 100.0f;
        cfg.extrusion_multiplier = 1.0f;
        cfg.gcode_firmware = HSBA_GCODE_MARLIN;
        cfg.nozzle_diameter = 0.4f;
        cfg.filament_diameter = 1.75f;
        cfg.nozzle_temp = 200.0f;
        cfg.bed_temp = 60.0f;
        cfg.retract_length = 1.0f;
        cfg.retract_speed = 40.0f;
        cfg.first_layer_speed = 20.0f;
        cfg.support_lua_script = 0;
        cfg.support_lua_func = 0;
        cfg.infill_lua_script = 0;
        cfg.infill_lua_func = 0;
        cfg.output_path = 0;
        return cfg;
    }

    /**
     * @brief Initialize SLA pipeline config with default values.
     * @return Default configuration struct (string fields are NULL).
     */
    static inline HsBaSlaPipelineConfig_t HsBaSlaConfigDefault(void)
    {
        HsBaSlaPipelineConfig_t cfg;
        cfg.model_name = 0;
        cfg.model_path = 0;
        cfg.layer_height = 0.05f;
        cfg.first_layer_height = 0.1f;
        cfg.bottom_exposure_time = 60.0f;
        cfg.normal_exposure_time = 2.5f;
        cfg.bottom_lift_distance = 5.0f;
        cfg.lift_distance = 3.0f;
        cfg.lift_speed = 60.0f;
        cfg.retract_speed = 150.0f;
        cfg.floor_raft_offset = 2.0f;
        cfg.floor_border_width = 1.0f;
        cfg.floor_fill_spacing = 0.5f;
        cfg.floor_fill_angle = 0.0f;
        cfg.floor_border_count = 2;
        cfg.floor_use_convex_hull = 0;
        cfg.enable_support = 1;
        cfg.overhang_angle = 45.0f;
        cfg.support_gap = 0.5f;
        cfg.support_diameter = 2.0f;
        cfg.support_density = 0.3f;
        cfg.support_pattern = HSBA_SLA_SUPPORT_SACRIFICIAL;
        cfg.support_lua_script = 0;
        cfg.support_lua_func = 0;
        cfg.floor_lua_script = 0;
        cfg.floor_lua_func = 0;
        cfg.export_lua_script = 0;
        cfg.export_lua_func = 0;
        cfg.output_path = 0;
        cfg.image_type = HSBA_SLA_IMAGE_PNG;
        cfg.image_width = 0;
        cfg.image_height = 0;
        return cfg;
    }

    /**
     * @brief Initialize SLS pipeline config with default values.
     * @return Default configuration struct (string fields are NULL).
     */
    static inline HsBaSlsPipelineConfig_t HsBaSlsConfigDefault(void)
    {
        HsBaSlsPipelineConfig_t cfg;
        cfg.model_name = 0;
        cfg.model_path = 0;
        cfg.layer_height = 0.1f;
        cfg.first_layer_height = 0.15f;
        cfg.laser_power = 30.0f;
        cfg.scan_speed = 2000.0f;
        cfg.hatch_spacing = 0.15f;
        cfg.hatch_rotation = 90.0f;
        cfg.bed_temperature = 180.0f;
        cfg.export_lua_script = 0;
        cfg.export_lua_func = 0;
        cfg.output_path = 0;
        return cfg;
    }

    /**
     * @brief Initialize file transfer pipeline config with default values.
     * @return Default configuration struct (string fields are NULL).
     */
    static inline HsBaFileTransferPipelineConfig_t HsBaFileTransferConfigDefault(void)
    {
        HsBaFileTransferPipelineConfig_t cfg;
        cfg.host = 0;
        cfg.port = 0;
        cfg.pool_size = 4;
        cfg.file_paths = 0;
        cfg.file_count = 0;
        return cfg;
    }

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // !HSBA_SLICER_PIPELINE_TYPES_H
