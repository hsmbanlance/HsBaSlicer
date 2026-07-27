/**
 * @file main_header.cpp
 * @brief LibHsBaSlicer usage example - non-module version (traditional headers)
 *
 * Demonstrates how to use LibHsBaSlicer C++ API directly via #include headers,
 * performing a complete FDM slicing workflow:
 * Preprocess -> Slice -> Support -> Fill -> Path Generation.
 *
 * Compiler: any C++20 compatible compiler (no module support required)
 * Platforms: Windows / Linux / macOS
 */

#include <filesystem>
#include <format>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

// LibHsBaSlicer public API headers
#include "2D/FloatPolygons.hpp"
#include "LibHsBaSlicer/Fill/polygon_fill.hpp"
#include "LibHsBaSlicer/Path/path_generator.hpp"
#include "LibHsBaSlicer/Preprocess/model_preprocess.hpp"
#include "LibHsBaSlicer/Slice/mesh_slice.hpp"
#include "LibHsBaSlicer/Support/fdm_support.hpp"

using namespace HsBa::Slicer;

// ---------------------------------------------------------------------------
// Process parameters
// ---------------------------------------------------------------------------
static constexpr float kLayerHeight = 0.2f;        // layer height (mm)
static constexpr float kFirstLayerHeight = 0.25f;  // first layer height (mm)
static constexpr float kLineWidth = 0.4f;          // line width (mm)
static constexpr float kPrintSpeed = 60.0f;        // print speed (mm/s)
static constexpr float kTravelSpeed = 120.0f;      // travel speed (mm/s)
static constexpr double kFillSpacing = 0.4;        // fill spacing (mm)
static constexpr double kFillAngle = 45.0;         // fill angle (deg)
static constexpr int kWallCount = 3;               // wall loop count
static constexpr float kOverhangAngle = 45.0f;     // overhang threshold (deg)

// ---------------------------------------------------------------------------
// Main workflow
// ---------------------------------------------------------------------------
int main()
{
    std::cout << "=== LibHsBaSlicer non-module sample (traditional headers) ===" << std::endl;

    const std::string model_name = "stanford_bunny";
    const std::string model_path = "models/stanford_bunny.stl";

    // Check model file existence
    if (!std::filesystem::exists(model_path))
    {
        std::cerr << std::format("Model file not found: {}", model_path) << std::endl;
        std::cerr << "Please place Stanford Bunny STL in the models/ directory." << std::endl;
        return 1;
    }

    // =====================================================================
    // Step 1: Preprocess - load model
    // =====================================================================
    std::cout << "[1/5] Loading model..." << std::endl;
    auto model = LoadModel(model_name, model_path);
    if (!model)
    {
        std::cerr << "Failed to load model!" << std::endl;
        return 1;
    }

    // Query model info
    ModelInfo info = GetModelInfo(model_name);
    std::cout << std::format("  BBox: ({:.2f}, {:.2f}, {:.2f}) ~ ({:.2f}, {:.2f}, {:.2f})", info.bbox_min.x(),
                             info.bbox_min.y(), info.bbox_min.z(), info.bbox_max.x(), info.bbox_max.y(),
                             info.bbox_max.z())
              << std::endl;
    std::cout << std::format("  Volume: {:.2f} mm^3", info.volume) << std::endl;

    // Optional: apply transforms
    // TranslateModel(model_name, Eigen::Vector3f(0, 0, -info.bbox_min.z()));
    // ScaleModel(model_name, 1.0f);

    // =====================================================================
    // Step 2: Slice - generate layer contours
    // =====================================================================
    std::cout << "[2/5] Slicing..." << std::endl;

    const float model_height = info.bbox_max.z() - info.bbox_min.z();
    const int total_layers = static_cast<int>(model_height / kLayerHeight) + 1;

    std::vector<Polygons> layer_contours;
    layer_contours.reserve(total_layers);

    for (int i = 0; i < total_layers; ++i)
    {
        float z = (i == 0) ? kFirstLayerHeight : kFirstLayerHeight + i * kLayerHeight;
        Polygons contours = Slice(*model, z);
        layer_contours.push_back(std::move(contours));
    }
    std::cout << std::format("  Total layers: {}", total_layers) << std::endl;

    // =====================================================================
    // Step 3: Support - generate FDM supports
    // =====================================================================
    std::cout << "[3/5] Generating supports..." << std::endl;

    // Convert integer polygons to floating-point (support API requires PolygonsD)
    std::vector<PolygonsD> layers_d;
    layers_d.reserve(total_layers);
    for (const auto& polys : layer_contours)
    {
        layers_d.push_back(UnIntegerization(polys));
    }

    Support::FdmSupportConfig support_cfg;
    support_cfg.overhang_angle_threshold = kOverhangAngle;
    support_cfg.layer_height = kLayerHeight;
    support_cfg.support_density = 0.3f;
    support_cfg.support_pattern = 0;  // Plane

    std::vector<PolygonsD> supports = GenerateAllFdmSupport(layers_d, support_cfg);
    std::cout << std::format("  Support layers: {}", supports.size()) << std::endl;

    // =====================================================================
    // Step 4: Fill - polygon infill
    // =====================================================================
    std::cout << "[4/5] Filling..." << std::endl;

    std::vector<LayerPathData> all_layer_data;
    all_layer_data.reserve(total_layers);

    for (int i = 0; i < total_layers; ++i)
    {
        LayerPathData layer_data;
        layer_data.z_height = (i == 0) ? kFirstLayerHeight : kFirstLayerHeight + i * kLayerHeight;

        // Outline (use floating-point version directly)
        layer_data.outlines = layers_d[i];

        // Infill
        if (!layer_contours[i].empty())
        {
            Polygons filled = FillWithBorder(layer_contours[i], kFillSpacing, kWallCount, FillMode::Zigzag, kFillAngle);
            // Convert to floating-point
            layer_data.fills = UnIntegerization(filled);
        }

        // Support
        if (i < static_cast<int>(supports.size()))
        {
            layer_data.supports = supports[i];
        }

        all_layer_data.push_back(std::move(layer_data));
    }
    std::cout << "  Fill complete." << std::endl;

    // =====================================================================
    // Step 5: Path generation - output G-code
    // =====================================================================
    std::cout << "[5/5] Generating G-code paths..." << std::endl;

    FdmPathConfig path_cfg;
    path_cfg.layer_height = kLayerHeight;
    path_cfg.line_width = kLineWidth;
    path_cfg.print_speed = kPrintSpeed;
    path_cfg.travel_speed = kTravelSpeed;
    path_cfg.extrusion_multiplier = 1.0f;
    path_cfg.units = GCodeUnits::mm;

    auto gcode_path = GenerateGCodePath(all_layer_data, path_cfg);

    // Save G-code
    std::filesystem::create_directories("output");
    std::filesystem::path out_file = "output/lib_header_sample.gcode";
    gcode_path->Save(out_file);

    std::cout << std::format("Slicing complete! G-code saved to: {}", out_file.string()) << std::endl;

    // Clean up model pool
    RemoveModel(model_name);

    return 0;
}
