#include "fdm_pipeline.h"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "LibHsBaSlicer/Fill/polygon_fill.hpp"
#include "LibHsBaSlicer/Path/path_generator.hpp"
#include "LibHsBaSlicer/Preprocess/model_preprocess.hpp"
#include "preprocess/ModelLoader.hpp"
#include "LibHsBaSlicer/Slice/mesh_slice.hpp"
#include "LibHsBaSlicer/Support/fdm_support.hpp"
#include "2D/PolygonFill.hpp"
#include "base/coroutine.hpp"
#include "support/SupportConfig.hpp"
#include "support/LuaSupport.hpp"

namespace HsBa::Slicer::Pipeline
{

// 内部结果（命名空间可见，供lambda引用）
struct InternalResult
{
    bool success = false;
    int total_layers = 0;
    std::string gcode_content;
    std::string error_message;
    double elapsed_seconds = 0.0;
};

// 内部配置
struct InternalConfig
{
    std::string model_name;
    std::string model_path;
    float layer_height = 0.2f;
    float first_layer_height = 0.25f;
    double fill_spacing = 0.4;
    FillMode fill_mode = FillMode::Zigzag;
    double fill_angle = 45.0;
    int wall_count = 3;
    int top_layer_count = 3;
    int bottom_layer_count = 3;
    double infill_density = 0.2;
    bool enable_support = true;
    std::string support_lua_script;
    std::string support_lua_func;
    std::string infill_lua_script;
    std::string infill_lua_func;
    Support::FdmSupportConfig support_config;
    FdmPathConfig path_config;
    std::string output_path;
    HsBaProgressCallback progress_cb = nullptr;
    void* progress_user_data = nullptr;
};

namespace
{

// RAII守卫：确保malloc分配的C字符串在异常/提前返回时被释放
struct OwnedCString
{
    char* data = nullptr;

    OwnedCString() = default;
    explicit OwnedCString(const std::string& str)
    {
        if (!str.empty())
        {
            data = static_cast<char*>(std::malloc(str.size() + 1));
            if (data)
                std::memcpy(data, str.c_str(), str.size() + 1);
        }
    }

    OwnedCString(const OwnedCString&) = delete;
    OwnedCString& operator=(const OwnedCString&) = delete;

    OwnedCString(OwnedCString&& other) noexcept : data(std::exchange(other.data, nullptr)) {}
    OwnedCString& operator=(OwnedCString&& other) noexcept
    {
        if (this != &other)
        {
            std::free(data);
            data = std::exchange(other.data, nullptr);
        }
        return *this;
    }

    ~OwnedCString() { std::free(data); }

    // 释放所有权并返回裸指针（调用者负责释放）
    char* release() { return std::exchange(data, nullptr); }
};

int CalculateLayerCount(const ModelInfo& info, float layer_height, float first_layer_height)
{
    float model_height = info.bbox_max.z() - info.bbox_min.z();
    if (model_height <= 0.0f)
        return 0;
    float remaining = model_height - first_layer_height;
    if (remaining <= 0.0f)
        return 1;
    return 1 + static_cast<int>(std::ceil(remaining / layer_height));
}

float GetLayerZ(int layer_index, float first_layer_height, float layer_height)
{
    if (layer_index == 0)
        return first_layer_height;
    return first_layer_height + layer_index * layer_height;
}

void ReportProgress(const InternalConfig& cfg, int percent, const std::string& stage)
{
    if (cfg.progress_cb)
    {
        cfg.progress_cb(percent, stage.c_str(), cfg.progress_user_data);
    }
}

// UnSafePolygons -> PolygonsD 转换
PolygonsD UnSafePolygonsToPolygonsD(const UnSafePolygons& unsafe_polys)
{
    Polygons int_polys;
    int_polys.reserve(unsafe_polys.size() * 2);
    for (const auto& up : unsafe_polys)
    {
        // for fdm/fff only closed polygons are valid, skip open polylines
        if (!up.closed || up.path.size() < 3)
        {
            continue;
        }

        auto normalized = NormalizeToSimplePolygons(up.path);
        for (const auto& simple_poly : normalized)
        {
            int_polys.push_back(simple_poly);
        }
    }
    return UnIntegerization(int_polys);
}
}  // anonymous namespace

InternalConfig BuildConfig(const HsBaFdmPipelineConfig_t* cfg, HsBaProgressCallback cb, void* ud)
{
    InternalConfig ic;
    ic.model_name = cfg->model_name ? cfg->model_name : "";
    ic.model_path = cfg->model_path ? cfg->model_path : "";
    ic.layer_height = cfg->layer_height;
    ic.first_layer_height = cfg->first_layer_height;
    ic.fill_spacing = cfg->fill_spacing;
    ic.fill_mode = static_cast<FillMode>(static_cast<int>(cfg->fill_mode));
    ic.fill_angle = cfg->fill_angle;
    ic.wall_count = cfg->wall_count;
    ic.top_layer_count = cfg->top_layer_count;
    ic.bottom_layer_count = cfg->bottom_layer_count;
    ic.infill_density = cfg->infill_density;
    ic.enable_support = cfg->enable_support != 0;
    ic.support_lua_script = cfg->support_lua_script ? cfg->support_lua_script : "";
    ic.support_lua_func = cfg->support_lua_func ? cfg->support_lua_func : "";
    ic.infill_lua_script = cfg->infill_lua_script ? cfg->infill_lua_script : "";
    ic.infill_lua_func = cfg->infill_lua_func ? cfg->infill_lua_func : "";

    ic.support_config.overhang_angle_threshold = cfg->overhang_angle;
    ic.support_config.layer_height = cfg->layer_height;
    ic.support_config.support_gap = cfg->support_gap;
    ic.support_config.support_diameter = cfg->support_diameter;
    ic.support_config.support_density = cfg->support_density;
    ic.support_config.support_pattern = static_cast<int>(cfg->support_pattern);
    ic.support_config.interface_layers = cfg->interface_layers;
    ic.support_config.honeycomb_cell_size = 5.0f;

    ic.path_config.layer_height = cfg->layer_height;
    ic.path_config.line_width = cfg->line_width;
    ic.path_config.print_speed = cfg->print_speed;
    ic.path_config.travel_speed = cfg->travel_speed;
    ic.path_config.extrusion_multiplier = cfg->extrusion_multiplier;

    ic.output_path = cfg->output_path ? cfg->output_path : "";
    ic.progress_cb = cb;
    ic.progress_user_data = ud;
    return ic;
}

HsBaFdmPipelineResult_t ToCResult(const InternalResult& ir)
{
    OwnedCString content(ir.gcode_content);
    OwnedCString error(ir.error_message);

    HsBaFdmPipelineResult_t cr{};
    cr.success = ir.success ? 1 : 0;
    cr.total_layers = ir.total_layers;
    cr.gcode_content = content.release();
    cr.error_message = error.release();
    cr.elapsed_seconds = ir.elapsed_seconds;
    return cr;
}

// ModelLoader 已在文件顶部引入。
// InternalConfig 不引用 ModelLoader，因此不受其不可复制的影响。

// 协程核心实现
Utils::Task<InternalResult> RunPipelineAsync(const InternalConfig& cfg)
{
    InternalResult result;
    auto start_time = std::chrono::steady_clock::now();

    try
    {
        // ========== 阶段1: 预处理 ==========
        // ModelLoader 在协程内局部创建，每个 pipeline 独立模型池
        ModelLoader model_loader;
        ReportProgress(cfg, 0, "Loading model...");
        auto model = model_loader.GetModel(cfg.model_name);
        if (!model)
        {
            model = model_loader.LoadModel(cfg.model_name, cfg.model_path);
        }
        if (!model)
        {
            result.success = false;
            result.error_message = "Failed to load model: " + cfg.model_path;
            co_return result;
        }

        ModelInfo info;
        model->BoundingBox(info.bbox_min, info.bbox_max);
        info.volume = model->Volume();
        int total_layers = CalculateLayerCount(info, cfg.layer_height, cfg.first_layer_height);
        if (total_layers <= 0)
        {
            result.success = false;
            result.error_message = "Invalid model height";
            co_return result;
        }
        result.total_layers = total_layers;
        ReportProgress(cfg, 10, "Model loaded");

        // ========== 阶段2: 切片 ==========
        ReportProgress(cfg, 15, "Slicing...");
        std::vector<PolygonsD> layer_outlines(total_layers);
        float z_offset = info.bbox_min.z();

        for (int i = 0; i < total_layers; ++i)
        {
            float z = GetLayerZ(i, cfg.first_layer_height, cfg.layer_height) + z_offset;
            layer_outlines[i] = UnSafePolygonsToPolygonsD(UnSafeSlice(*model, z));
            int progress = 15 + (i * 25) / total_layers;
            ReportProgress(cfg, progress, "Slicing layer");
        }
        ReportProgress(cfg, 40, "Slicing complete");

        // ========== 阶段3: 支撑 ==========
        std::vector<PolygonsD> layer_supports(total_layers);
        if (cfg.enable_support)
        {
            ReportProgress(cfg, 45, "Generating supports...");
            if (!cfg.support_lua_script.empty())
            {
                // Lua自定义支撑：读取文件内容，使用string_view构造函数避免歧义
                std::string func = cfg.support_lua_func.empty() ? "generate_support" : cfg.support_lua_func;
                std::ifstream ifs(cfg.support_lua_script);
                std::string script_content((std::istreambuf_iterator<char>(ifs)),
                                           std::istreambuf_iterator<char>());
                auto lua_support = std::make_unique<Support::LuaSupport>(
                    std::string_view(script_content), std::string_view(func));
                layer_supports = lua_support->GenerateAll(layer_outlines, cfg.support_config);
            }
            else
            {
                layer_supports = GenerateAllFdmSupport(layer_outlines, cfg.support_config);
            }
            ReportProgress(cfg, 60, "Support generation complete");
        }
        else
        {
            ReportProgress(cfg, 60, "Support disabled");
        }

        // ========== 阶段4: 填充 ==========
        ReportProgress(cfg, 65, "Generating fills...");
        std::vector<PolygonsD> layer_fills(total_layers);
        
        // 确定顶层/底层/中间层范围
        const int bottom_end = cfg.bottom_layer_count;                              // [0, bottom_end) 为底层
        const int top_start = total_layers - cfg.top_layer_count;                   // [top_start, total_layers) 为顶层
        const bool has_lua_infill = !cfg.infill_lua_script.empty();
        const std::string infill_func = cfg.infill_lua_func.empty() ? "generate_fill" : cfg.infill_lua_func;
        
        // 中间层填充间距：密度越低间距越大
        double middle_spacing = cfg.fill_spacing;
        if (cfg.infill_density > 0.0 && cfg.infill_density < 1.0)
        {
            middle_spacing = cfg.fill_spacing / cfg.infill_density;
        }
        
        for (int i = 0; i < total_layers; ++i)
        {
            if (!layer_outlines[i].empty())
            {
                Polygons int_polys = Integerization(layer_outlines[i]);
                bool is_solid = (i < bottom_end) || (i >= top_start);  // 顶层/底层实心填充
        
                if (has_lua_infill && !is_solid)
                {
                    // Lua自定义填充（仅中间层）
                    Polygons fill_result = LuaCustomFill(int_polys, cfg.infill_lua_script, infill_func);
                    layer_fills[i] = UnIntegerization(fill_result);
                }
                else if (is_solid)
                {
                    // 顶层/底层：实心填充
                    Polygons fill_result =
                        FillWithBorder(int_polys, cfg.fill_spacing, cfg.wall_count, cfg.fill_mode, cfg.fill_angle);
                    layer_fills[i] = UnIntegerization(fill_result);
                }
                else
                {
                    // 中间层：按填充率调整间距
                    Polygons fill_result =
                        FillWithBorder(int_polys, middle_spacing, cfg.wall_count, cfg.fill_mode, cfg.fill_angle);
                    layer_fills[i] = UnIntegerization(fill_result);
                }
            }
            int progress = 65 + (i * 20) / total_layers;
            ReportProgress(cfg, progress, "Filling layer");
        }
        ReportProgress(cfg, 85, "Fill generation complete");

        // ========== 阶段5: 路径生成 ==========
        ReportProgress(cfg, 90, "Generating G-code paths...");

        std::vector<LayerPathData> layer_path_data(total_layers);
        for (int i = 0; i < total_layers; ++i)
        {
            layer_path_data[i].outlines = layer_outlines[i];
            layer_path_data[i].fills = layer_fills[i];
            layer_path_data[i].supports = layer_supports[i];
            layer_path_data[i].z_height = GetLayerZ(i, cfg.first_layer_height, cfg.layer_height);
        }

        auto gcode_path = GenerateGCodePath(layer_path_data, cfg.path_config);

        if (gcode_path)
        {
            result.gcode_content = gcode_path->ToString();
            result.success = true;
        }
        else
        {
            result.success = false;
            result.error_message = "Failed to generate G-code path";
        }

        ReportProgress(cfg, 100, "Pipeline complete");
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.error_message = std::string("Pipeline error: ") + e.what();
    }

    auto end_time = std::chrono::steady_clock::now();
    result.elapsed_seconds = std::chrono::duration<double>(end_time - start_time).count();

    co_return result;
}

}  // namespace HsBa::Slicer::Pipeline

// ========== C导出接口 ==========

HSBA_SLICER_API HsBaFdmPipelineConfig_t HsBaCreateDefaultConfig(void)
{
    HsBaFdmPipelineConfig_t cfg{};

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
    cfg.support_lua_script = nullptr;
    cfg.support_lua_func = nullptr;
    cfg.infill_lua_script = nullptr;
    cfg.infill_lua_func = nullptr;

    return cfg;
}

HSBA_SLICER_API HsBaFdmPipelineResult_t HsBaRunFdmPipeline(const HsBaFdmPipelineConfig_t* config,
                                                           HsBaProgressCallback callback, void* user_data)
{
    auto ic = HsBa::Slicer::Pipeline::BuildConfig(config, callback, user_data);
    auto task = HsBa::Slicer::Pipeline::RunPipelineAsync(ic);
    auto ir = task.get_result();
    return HsBa::Slicer::Pipeline::ToCResult(ir);
}

HSBA_SLICER_API void HsBaRunFdmPipelineAsync(const HsBaFdmPipelineConfig_t* config, HsBaProgressCallback callback,
                                             void* user_data, HsBaResultCallback result_callback,
                                             void* result_user_data)
{
    // 堆分配 config，确保协程执行期间生命周期安全
    auto shared_cfg = std::make_shared<HsBa::Slicer::Pipeline::InternalConfig>(HsBa::Slicer::Pipeline::BuildConfig(config, callback, user_data));
    auto task = HsBa::Slicer::Pipeline::RunPipelineAsync(*shared_cfg);
    task.then(
        [shared_cfg, result_callback, result_user_data](HsBa::Slicer::Pipeline::InternalResult ir)
        {
            auto cr = HsBa::Slicer::Pipeline::ToCResult(ir);
            if (result_callback)
            {
                result_callback(cr, result_user_data);
            }
        });
}

HSBA_SLICER_API void HsBaFreePipelineResult(HsBaFdmPipelineResult_t* result)
{
    if (!result)
        return;
    std::free(std::exchange(result->gcode_content, nullptr));
    std::free(std::exchange(result->error_message, nullptr));
}
