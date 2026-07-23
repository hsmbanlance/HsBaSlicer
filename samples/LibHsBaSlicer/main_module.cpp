/**
 * @file main_module.cpp
 * @brief LibHsBaSlicer usage example - C++20 module version
 *
 * Demonstrates how to use the ModuleHsBaSlicer C++20 module wrapper,
 * performing a complete FDM slicing workflow with class-based API:
 * Model -> FdmPipeline -> FdmResult.
 *
 * Compiler: C++20 module-capable compiler (MSVC 19.34+, GCC 14+, Clang 16+)
 * Requires: HSBA_SLICER_MODULE=ON (default)
 */

// Standard library includes MUST come before `import` in MSVC C++20 modules.
// Otherwise the BMI's std declarations conflict with re-included headers (C2572).
#include <filesystem>
#include <format>
#include <iostream>
#include <string>

// Project headers: GMF-defined types (ModelInfo, etc.) are not exported by
// C++20 modules (standard limitation). Consumers must include them directly.
#include "LibHsBaSlicer/Preprocess/model_preprocess.hpp"
#include "pipelinetypes/pipeline_types.h"

import hsba.slicer;

using namespace HsBa::Slicer;

int main()
{
    std::cout << "=== ModuleHsBaSlicer C++20 module sample ===" << std::endl;

    const std::string model_path = "models/stanford_bunny.stl";

    // Check model file existence
    if (!std::filesystem::exists(model_path))
    {
        std::cerr << std::format("Model file not found: {}", model_path) << std::endl;
        std::cerr << "Please place Stanford Bunny STL in the models/ directory." << std::endl;
        return 1;
    }

    try
    {
        // =================================================================
        // Step 1: Load model (RAII - auto cleanup on scope exit)
        // =================================================================
        std::cout << "[1/3] Loading model..." << std::endl;
        Model model("stanford_bunny", model_path);

        // Query model info
        ModelInfo info = model.info();
        std::cout << std::format("  BBox: ({:.2f}, {:.2f}, {:.2f}) ~ ({:.2f}, {:.2f}, {:.2f})",
                                 info.bbox_min.x(), info.bbox_min.y(), info.bbox_min.z(),
                                 info.bbox_max.x(), info.bbox_max.y(), info.bbox_max.z())
                  << std::endl;
        std::cout << std::format("  Volume: {:.2f} mm^3", info.volume) << std::endl;

        // Optional transforms
        // model.translate(Eigen::Vector3f(0, 0, -info.bbox_min.z()));
        // model.scale(1.0f);

        // =================================================================
        // Step 2: Configure FDM pipeline (using pipeline_types.h config)
        // =================================================================
        std::cout << "[2/3] Configuring FDM pipeline..." << std::endl;

        HsBaFdmPipelineConfig_t cfg = defaultFdmConfig();
        cfg.layer_height         = 0.2f;
        cfg.first_layer_height   = 0.25f;
        cfg.line_width           = 0.4f;
        cfg.print_speed          = 60.0f;
        cfg.travel_speed         = 120.0f;
        cfg.extrusion_multiplier = 1.0f;
        cfg.fill_spacing         = 0.4;
        cfg.fill_angle           = 45.0;
        cfg.wall_count           = 3;
        cfg.fill_mode            = HSBA_FILL_ZIGZAG;
        cfg.enable_support       = 1;
        cfg.overhang_angle       = 45.0f;
        cfg.support_density      = 0.3f;
        cfg.support_pattern      = HSBA_SUPPORT_PLANE;
        cfg.output_path          = "output/lib_module_sample.gcode";

        FdmPipeline pipeline(cfg);

        // =================================================================
        // Step 3: Run full pipeline
        // =================================================================
        std::cout << "[3/3] Running FDM pipeline..." << std::endl;
        FdmResult result = pipeline.run(model);

        std::cout << std::format("  Total layers: {}", result.total_layers) << std::endl;
        std::cout << std::format("  G-code size: {} bytes", result.gcode.size()) << std::endl;
        std::cout << "Slicing complete! G-code saved to: output/lib_module_sample.gcode"
                  << std::endl;

        // Model destructor automatically calls RemoveModel()
    }
    catch (const SlicerError& e)
    {
        std::cerr << std::format("Slicer error: {}", e.what()) << std::endl;
        return 1;
    }
    catch (const std::exception& e)
    {
        std::cerr << std::format("Error: {}", e.what()) << std::endl;
        return 1;
    }

    // =====================================================================
    // Bonus: Step-by-step usage (individual pipeline stages)
    // =====================================================================
    // try {
    //     Model model("bunny", "models/stanford_bunny.stl");
    //     FdmPipeline pipeline;
    //
    //     auto contours = pipeline.sliceAll(model);
    //     auto layers_d = /* convert to double */;
    //     auto supports = pipeline.generateSupports(layers_d);
    //     auto filled   = pipeline.fill(contours[0]);
    //     auto path     = pipeline.generatePath(layer_data);
    // } catch (const SlicerError& e) { ... }

    // =====================================================================
    // Version info
    // =====================================================================
    // std::cout << versionJson() << std::endl;
    // std::cout << versionXml() << std::endl;

    return 0;
}
