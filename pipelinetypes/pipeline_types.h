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
        HSBA_GCODE_MARLIN = 0,  ///< Marlin firmware (most common)
        HSBA_GCODE_REPRAP = 1,  ///< RepRap / RRF firmware
        HSBA_GCODE_KLIPPER = 2  ///< Klipper firmware
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
        const char* support_lua_script;  ///< Support Lua script path (NULL to use built-in algorithm)
        const char* support_lua_func;    ///< Support Lua function name (NULL, default "generate_support")
        const char* infill_lua_script;   ///< Infill Lua script path (NULL to use built-in algorithm)
        const char* infill_lua_func;     ///< Infill Lua function name (NULL, default "generate_fill")

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
        float floor_raft_offset;    ///< Raft outward offset from footprint (mm), default 2.0
        float floor_border_width;   ///< Floor border ring width (mm), default 1.0
        float floor_fill_spacing;   ///< Floor fill line spacing (mm), default 0.5
        float floor_fill_angle;     ///< Floor fill angle (degrees), default 0.0
        int floor_border_count;     ///< Number of floor border loops, default 2
        int floor_use_convex_hull;  ///< Use convex hull for floor (0=false, 1=true), default 0

        /* Support Configuration */
        int enable_support;                       ///< Enable support (0=false, 1=true), default 1
        float overhang_angle;                     ///< Overhang angle threshold (degrees), default 45.0
        float support_gap;                        ///< Gap between support and model (mm), default 0.5
        float support_diameter;                   ///< Support column diameter (mm), default 2.0
        float support_density;                    ///< Support fill density [0,1], default 0.3
        HsBaSlaSupportPattern_t support_pattern;  ///< Support pattern, default HSBA_SLA_SUPPORT_SACRIFICIAL

        /* Lua Custom Configuration */
        const char* support_lua_script;  ///< Support Lua script path (NULL = built-in)
        const char* support_lua_func;    ///< Support Lua function name (NULL = "generate_support")
        const char* floor_lua_script;    ///< Floor Lua script path (NULL = built-in)
        const char* floor_lua_func;      ///< Floor Lua function name (NULL = "generate_floor")
        const char* export_lua_script;   ///< Export Lua script path (NULL = built-in)
        const char* export_lua_func;     ///< Export Lua function name (NULL = "export_sla")

        /* Output Configuration */
        const char* output_path;        ///< Output zip file path (can be NULL)
        HsBaSlaImageType_t image_type;  ///< Layer image format, default HSBA_SLA_IMAGE_PNG
        int image_width;                ///< Image width in pixels (0 = auto), default 0
        int image_height;               ///< Image height in pixels (0 = auto), default 0

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
        float laser_power;      ///< Laser power (W), default 30.0
        float scan_speed;       ///< Scan speed (mm/s), default 2000.0
        float hatch_spacing;    ///< Hatch line spacing (mm), default 0.15
        float hatch_rotation;   ///< Hatch rotation between layers (degrees), default 90.0
        float bed_temperature;  ///< Powder bed temperature (°C), default 180.0

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
     *  SLM Types (metal powder-bed, laser/e-beam melting)
     *
     *  SLM mirrors the SLS process flow (Preprocess -> Slice -> Lua export).
     *  Metal-specific parameters (material, energy source, shielding gas) are
     *  folded into the config JSON handed to the export script.
     * ====================================================================== */

    typedef enum HsBaSlmMaterial
    {
        HSBA_SLM_MATERIAL_IRON = 0,
        HSBA_SLM_MATERIAL_ALUMINUM = 1,
        HSBA_SLM_MATERIAL_TITANIUM = 2,
        HSBA_SLM_MATERIAL_UNKNOWN = 3
    } HsBaSlmMaterial_t;

    typedef enum HsBaSlmLight
    {
        HSBA_SLM_LIGHT_LASER = 0,
        HSBA_SLM_LIGHT_EBEAM = 1,
        HSBA_SLM_LIGHT_UNKNOWN = 2
    } HsBaSlmLight_t;

    /// @brief Shared metal shielding-gas enum (used by SLM and WAAM).
    typedef enum HsBaMetalProtectGas
    {
        HSBA_METAL_GAS_ARGON = 0,
        HSBA_METAL_GAS_HELIUM = 1,
        HSBA_METAL_GAS_N2 = 2,
        HSBA_METAL_GAS_CO2 = 3,
        HSBA_METAL_GAS_UNKNOWN = 4
    } HsBaMetalProtectGas_t;

    typedef struct HsBaSlmPipelineConfig
    {
        const char* model_name;
        const char* model_path;
        float layer_height;
        float first_layer_height;
        float laser_power;
        float scan_speed;
        float hatch_spacing;
        float hatch_rotation;
        float bed_temperature;
        HsBaSlmMaterial_t material;
        HsBaSlmLight_t light_source;
        HsBaMetalProtectGas_t protect_gas;
        const char* export_lua_script; /* required */
        const char* export_lua_func;
        const char* output_path;
    } HsBaSlmPipelineConfig_t;

    typedef struct HsBaSlmPipelineResult
    {
        int success;
        int total_layers;
        char* export_path;
        char* error_message;
        double elapsed_seconds;
    } HsBaSlmPipelineResult_t;

    typedef void (*HsBaSlmProgressCallback)(int percent, const char* stage, void* user_data);
    typedef void (*HsBaSlmResultCallback)(HsBaSlmPipelineResult_t result, void* user_data);

    /* ========================================================================
     *  LOM Types (laminated object manufacturing, sheet bonding + cutting)
     * ====================================================================== */

    typedef enum HsBaLomCutMode
    {
        HSBA_LOM_CUT_CONTOUR = 0,
        HSBA_LOM_CUT_HALFTONE = 1
    } HsBaLomCutMode_t;

    typedef struct HsBaLomPipelineConfig
    {
        const char* model_name;
        const char* model_path;
        float layer_height;       /* sheet thickness (mm) */
        float first_layer_height; /* first sheet thickness (mm) */
        float cut_speed;          /* laser cut speed (mm/s) */
        float cut_margin;         /* contour offset (mm) */
        float cut_power;          /* laser cut power [0,1] */
        float bond_temperature;   /* bonding temperature (C) */
        float bond_pressure;      /* bonding pressure (MPa) */
        float bond_time;          /* bonding time per layer (s) */
        int seal_contour;         /* seal part edge (0=false, 1=true) */
        HsBaLomCutMode_t cut_mode;
        const char* export_lua_script; /* required */
        const char* export_lua_func;
        const char* output_path;
    } HsBaLomPipelineConfig_t;

    typedef struct HsBaLomPipelineResult
    {
        int success;
        int total_layers;
        char* export_path;
        char* error_message;
        double elapsed_seconds;
    } HsBaLomPipelineResult_t;

    typedef void (*HsBaLomProgressCallback)(int percent, const char* stage, void* user_data);
    typedef void (*HsBaLomResultCallback)(HsBaLomPipelineResult_t result, void* user_data);

    /* ========================================================================
     *  3DP Types (binder jetting, powder bed + liquid binder)
     * ====================================================================== */

    typedef enum HsBaTdpBinderMode
    {
        HSBA_TDP_FULL_COLOR = 0,
        HSBA_TDP_SINGLE = 1,
        HSBA_TDP_SINTERING = 2
    } HsBaTdpBinderMode_t;

    typedef struct HsBaTdpPipelineConfig
    {
        const char* model_name;
        const char* model_path;
        float layer_height;
        float first_layer_height;
        int head_count;            /* print head nozzle count */
        float drop_spacing;        /* binder drop spacing (mm) */
        float binder_saturation;   /* binder saturation [0,1] */
        float ink_curing_time;     /* per-layer curing time (s) */
        float bed_temperature;     /* powder bed temperature (C) */
        HsBaTdpBinderMode_t binder_mode;
        const char* export_lua_script; /* required */
        const char* export_lua_func;
        const char* output_path;
    } HsBaTdpPipelineConfig_t;

    typedef struct HsBaTdpPipelineResult
    {
        int success;
        int total_layers;
        char* export_path;
        char* error_message;
        double elapsed_seconds;
    } HsBaTdpPipelineResult_t;

    typedef void (*HsBaTdpProgressCallback)(int percent, const char* stage, void* user_data);
    typedef void (*HsBaTdpResultCallback)(HsBaTdpPipelineResult_t result, void* user_data);

    /* ========================================================================
     *  WAAM Types (wire arc additive manufacturing, robot deposition)
     *
     *  WAAM differs fundamentally from the bed-based modes: its output is a
     *  robot language program (ABB/KUKA/FANUC) rather than a layer zip.
     * ====================================================================== */

    typedef enum HsBaWaamMaterial
    {
        HSBA_WAAM_MATERIAL_STEEL = 0,
        HSBA_WAAM_MATERIAL_ALUMINUM = 1,
        HSBA_WAAM_MATERIAL_TITANIUM = 2,
        HSBA_WAAM_MATERIAL_COPPER = 3,
        HSBA_WAAM_MATERIAL_UNKNOWN = 4
    } HsBaWaamMaterial_t;

    typedef enum HsBaWaamWeldProcess
    {
        HSBA_WAAM_WELD_ARC = 0,
        HSBA_WAAM_WELD_LASER = 1,
        HSBA_WAAM_WELD_UNKNOWN = 2
    } HsBaWaamWeldProcess_t;

    typedef enum HsBaWaamProtection
    {
        HSBA_WAAM_PROTECTION_SHIELD_GAS = 0,
        HSBA_WAAM_PROTECTION_VACUUM = 1,
        HSBA_WAAM_PROTECTION_UNKNOWN = 2
    } HsBaWaamProtection_t;

    typedef enum HsBaWaamRobotType
    {
        HSBA_WAAM_ROBOT_ABB = 0,
        HSBA_WAAM_ROBOT_KUKA = 1,
        HSBA_WAAM_ROBOT_FANUC = 2,
        HSBA_WAAM_ROBOT_UNKNOWN = 3 /* requires a Lua path script */
    } HsBaWaamRobotType_t;

    typedef struct HsBaWaamPipelineConfig
    {
        const char* model_name;
        const char* model_path;
        float layer_height;          /* bead/layer height (mm) */
        float first_layer_height;    /* first layer height (mm) */
        float bead_width;            /* deposited bead width (mm) */
        float travel_speed;          /* torch travel speed (mm/s) */
        float wire_feed_speed;       /* wire feed speed (m/min) */
        float arc_current;           /* welding current (A) */
        float arc_voltage;           /* arc voltage (V) */
        float gas_flow_rate;         /* shielding gas flow (L/min) */
        HsBaWaamMaterial_t material;
        HsBaWaamWeldProcess_t welding_process;
        HsBaWaamProtection_t protection;
        HsBaMetalProtectGas_t protect_gas;
        float interpass_temperature; /* interpass temperature (C) */
        HsBaWaamRobotType_t robot_type;
        const char* path_lua_script; /* optional custom robot-code generator */
        const char* path_lua_func;   /* optional Lua function name */
        const char* output_path;
    } HsBaWaamPipelineConfig_t;

    typedef struct HsBaWaamPipelineResult
    {
        int success;
        int total_layers;
        char* output_path;
        char* error_message;
        double elapsed_seconds;
    } HsBaWaamPipelineResult_t;

    typedef void (*HsBaWaamProgressCallback)(int percent, const char* stage, void* user_data);
    typedef void (*HsBaWaamResultCallback)(HsBaWaamPipelineResult_t result, void* user_data);

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
        const char* host;  ///< Remote host address (must not be NULL)
        const char* port;  ///< Remote service port (must not be NULL)
        int pool_size;     ///< Connection pool size [1,16], default 4

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
        int success;             ///< Success flag (0=false, 1=true)
        int files_transferred;   ///< Number of files successfully transferred
        int total_files;         ///< Total number of files requested
        char* error_message;     ///< Error message (UTF-8, caller must free)
        double elapsed_seconds;  ///< Elapsed time (seconds)
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
     *  Custom Lua Pipeline Types
     * ====================================================================== */

    /**
     * @brief Fully Lua-driven custom pipeline configuration (C-compatible struct).
     *
     * Unlike the FDM/SLA/SLS pipelines, the entire workflow (stage order and
     * content) is defined by the Lua entry function inside `pipeline_lua_script`
     * (or `pipeline_lua_source`). The C++ side only exposes the pipeline
     * building blocks (model loading, slicing, support, fill, floor, G-code
     * path output, SLA/SLS packaging) through the global `HsBa` Lua table.
     */
    typedef struct HsBaCustomPipelineConfig
    {
        /* Lua Pipeline Definition */
        const char* pipeline_lua_script;  ///< Path to the pipeline Lua script (must not be NULL unless source is set)
        const char* pipeline_lua_source;  ///< Inline Lua source executed before the script file (can be NULL)
        const char* entry_func;           ///< Lua entry function name (NULL = "run_pipeline")
        const char* config_json;          ///< Free-form JSON string passed to Lua as `pipeline_config` (can be NULL)

        /* Model Configuration (optional, passed to Lua as `model_name` / `model_path`) */
        const char* model_name;  ///< Model name (can be NULL)
        const char* model_path;  ///< Model file path (can be NULL)

        /* Output Configuration */
        const char* output_path;  ///< Default output path passed to Lua as `output_path` (can be NULL)

    } HsBaCustomPipelineConfig_t;

    /**
     * @brief Custom Lua pipeline result (C-compatible struct).
     *
     * Must call HsBaFreeCustomPipelineResult to release memory after use.
     */
    typedef struct HsBaCustomPipelineResult
    {
        int success;             ///< Success flag (0=false, 1=true)
        int total_layers;        ///< Layer count reported by the Lua script (0 if not set)
        char* output_path;       ///< Output path reported by the Lua script (caller must free)
        char* result_string;     ///< String returned by the Lua entry function (caller must free)
        char* error_message;     ///< Error message (UTF-8, caller must free)
        double elapsed_seconds;  ///< Elapsed time (seconds)
    } HsBaCustomPipelineResult_t;

    /**
     * @brief Custom Lua pipeline progress callback function type.
     * @param percent Progress percentage (0-100, chosen by the Lua script).
     * @param stage Current stage description (UTF-8 string).
     * @param user_data User-defined data pointer.
     */
    typedef void (*HsBaCustomProgressCallback)(int percent, const char* stage, void* user_data);

    /**
     * @brief Result callback for async custom Lua pipeline.
     */
    typedef void (*HsBaCustomResultCallback)(HsBaCustomPipelineResult_t result, void* user_data);

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
     * @brief Initialize SLM pipeline config with default values.
     * @return Default configuration struct (string fields are NULL).
     */
    static inline HsBaSlmPipelineConfig_t HsBaSlmConfigDefault(void)
    {
        HsBaSlmPipelineConfig_t cfg;
        cfg.model_name = 0;
        cfg.model_path = 0;
        cfg.layer_height = 0.06f;
        cfg.first_layer_height = 0.08f;
        cfg.laser_power = 200.0f;
        cfg.scan_speed = 1000.0f;
        cfg.hatch_spacing = 0.1f;
        cfg.hatch_rotation = 67.0f;
        cfg.bed_temperature = 100.0f;
        cfg.material = HSBA_SLM_MATERIAL_TITANIUM;
        cfg.light_source = HSBA_SLM_LIGHT_LASER;
        cfg.protect_gas = HSBA_METAL_GAS_ARGON;
        cfg.export_lua_script = 0;
        cfg.export_lua_func = 0;
        cfg.output_path = 0;
        return cfg;
    }

    /**
     * @brief Initialize LOM pipeline config with default values.
     * @return Default configuration struct (string fields are NULL).
     */
    static inline HsBaLomPipelineConfig_t HsBaLomConfigDefault(void)
    {
        HsBaLomPipelineConfig_t cfg;
        cfg.model_name = 0;
        cfg.model_path = 0;
        cfg.layer_height = 0.2f;
        cfg.first_layer_height = 0.2f;
        cfg.cut_speed = 300.0f;
        cfg.cut_margin = 0.5f;
        cfg.cut_power = 0.8f;
        cfg.bond_temperature = 150.0f;
        cfg.bond_pressure = 1.0f;
        cfg.bond_time = 5.0f;
        cfg.seal_contour = 1;
        cfg.cut_mode = HSBA_LOM_CUT_CONTOUR;
        cfg.export_lua_script = 0;
        cfg.export_lua_func = 0;
        cfg.output_path = 0;
        return cfg;
    }

    /**
     * @brief Initialize 3DP pipeline config with default values.
     * @return Default configuration struct (string fields are NULL).
     */
    static inline HsBaTdpPipelineConfig_t HsBaTdpConfigDefault(void)
    {
        HsBaTdpPipelineConfig_t cfg;
        cfg.model_name = 0;
        cfg.model_path = 0;
        cfg.layer_height = 0.1f;
        cfg.first_layer_height = 0.12f;
        cfg.head_count = 128;
        cfg.drop_spacing = 0.05f;
        cfg.binder_saturation = 0.6f;
        cfg.ink_curing_time = 1.0f;
        cfg.bed_temperature = 40.0f;
        cfg.binder_mode = HSBA_TDP_SINGLE;
        cfg.export_lua_script = 0;
        cfg.export_lua_func = 0;
        cfg.output_path = 0;
        return cfg;
    }

    /**
     * @brief Initialize WAAM pipeline config with default values.
     * @return Default configuration struct (string fields are NULL).
     */
    static inline HsBaWaamPipelineConfig_t HsBaWaamConfigDefault(void)
    {
        HsBaWaamPipelineConfig_t cfg;
        cfg.model_name = 0;
        cfg.model_path = 0;
        cfg.layer_height = 0.8f;
        cfg.first_layer_height = 1.0f;
        cfg.bead_width = 1.2f;
        cfg.travel_speed = 8.0f;
        cfg.wire_feed_speed = 5.0f;
        cfg.arc_current = 180.0f;
        cfg.arc_voltage = 22.0f;
        cfg.gas_flow_rate = 15.0f;
        cfg.material = HSBA_WAAM_MATERIAL_STEEL;
        cfg.welding_process = HSBA_WAAM_WELD_ARC;
        cfg.protection = HSBA_WAAM_PROTECTION_SHIELD_GAS;
        cfg.protect_gas = HSBA_METAL_GAS_ARGON;
        cfg.interpass_temperature = 100.0f;
        cfg.robot_type = HSBA_WAAM_ROBOT_ABB;
        cfg.path_lua_script = 0;
        cfg.path_lua_func = 0;
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

    /**
     * @brief Initialize custom Lua pipeline config with default values.
     * @return Default configuration struct (string fields are NULL).
     */
    static inline HsBaCustomPipelineConfig_t HsBaCustomConfigDefault(void)
    {
        HsBaCustomPipelineConfig_t cfg;
        cfg.pipeline_lua_script = 0;
        cfg.pipeline_lua_source = 0;
        cfg.entry_func = 0;
        cfg.config_json = 0;
        cfg.model_name = 0;
        cfg.model_path = 0;
        cfg.output_path = 0;
        return cfg;
    }

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // !HSBA_SLICER_PIPELINE_TYPES_H
