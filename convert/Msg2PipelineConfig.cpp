#include "Msg2PipelineConfig.hpp"

#include <cstdlib>
#include <cstring>
#include <string>

namespace HsBa::Slicer
{

namespace
{

/// @brief Duplicate a std::string as a malloc-allocated C string.
/// @return Pointer to malloc'd copy, or nullptr if input is empty.
char* DupString(const std::string& str)
{
    if (str.empty())
        return nullptr;
    char* dup = static_cast<char*>(std::malloc(str.size() + 1));
    if (dup)
        std::memcpy(dup, str.c_str(), str.size() + 1);
    return dup;
}

}  // anonymous namespace

void MsgToFdmConfig(const HsbaProto::msg_fdm_pipeline_config& msg, HsBaFdmPipelineConfig_t* config)
{
    *config = HsBaFdmConfigDefault();

    config->model_name = DupString(msg.fdm_pipe_config_model_name());
    config->model_path = DupString(msg.fdm_pipe_config_model_path());

    config->layer_height = msg.fdm_pipe_config_layer_height();
    config->first_layer_height = msg.fdm_pipe_config_first_layer_height();

    config->fill_spacing = msg.fdm_pipe_config_fill_space();
    config->fill_mode = static_cast<HsBaFillMode_t>(msg.fdm_pipe_config_fill_type());
    config->fill_angle = msg.fdm_pipe_config_fill_angle();
    config->wall_count = msg.fdm_pipe_config_wall_count();
    config->top_layer_count = msg.fdm_pipe_config_top_layer_count();
    config->bottom_layer_count = msg.fdm_pipe_config_bottom_layer_count();

    config->enable_support = msg.fdm_pipe_config_support_enable() ? 1 : 0;
    config->overhang_angle = msg.fdm_pipe_config_support_angle();
    config->support_gap = msg.fdm_pipe_config_support_gap();
    config->support_diameter = msg.fdm_pipe_config_support_support_diameter();
    config->support_density = msg.fdm_pipe_config_support_density();
    config->support_pattern = static_cast<HsBaSupportPattern_t>(msg.fdm_pipe_config_support_pattern());
    config->interface_layers = msg.fdm_pipe_config_interface_layers();
    config->interface_density = msg.fdm_pipe_config_interface_density();

    config->line_width = msg.fdm_pipe_config_line_width();
    config->print_speed = msg.fdm_pipe_config_print_speed();
    config->travel_speed = msg.fdm_pipe_config_travel_speed();
    config->extrusion_multiplier = msg.fdm_pipe_config_extrusion_multiplier();

    config->gcode_firmware = static_cast<HsBaGCodeFirmware_t>(msg.fdm_pipe_config_gcode_firmware());
    config->nozzle_diameter = msg.fdm_pipe_config_nozzle_diameter();
    config->filament_diameter = msg.fdm_pipe_config_filament_diameter();
    config->nozzle_temp = msg.fdm_pipe_config_nozzle_temp();
    config->bed_temp = msg.fdm_pipe_config_bed_temp();
    config->retract_length = msg.fdm_pipe_config_retract_length();
    config->retract_speed = msg.fdm_pipe_config_retract_speed();
    config->first_layer_speed = msg.fdm_pipe_config_first_layer_speed();

    config->support_lua_script = DupString(msg.fdm_pipe_config_support_lua_script());
    config->support_lua_func = DupString(msg.fdm_pipe_config_support_lua_func());
    config->infill_lua_script = DupString(msg.fdm_pipe_config_infill_lua_script());
    config->infill_lua_func = DupString(msg.fdm_pipe_config_infill_lua_func());

    config->output_path = DupString(msg.fdm_pipe_config_output_path());
}

void MsgToFdmResult(const HsbaProto::msg_fdm_pipe_result& msg, HsBaFdmPipelineResult_t* result)
{
    result->success = msg.fdm_pipe_result_success() ? 1 : 0;
    result->total_layers = msg.fdm_pipe_result_total_layers();
    result->gcode_content = DupString(msg.fdm_pipe_result_gcode_content());
    result->error_message = DupString(msg.fdm_pipe_result_error_message());
    result->elapsed_seconds = msg.fdm_pipe_result_elapsed_seconds();
}

void MsgToSlaConfig(const HsbaProto::sla_pipe_config& msg, HsBaSlaPipelineConfig_t* config)
{
    *config = HsBaSlaConfigDefault();

    config->model_name = DupString(msg.sla_pipe_config_model_name());
    config->model_path = DupString(msg.sla_pipe_config_model_path());

    config->layer_height = msg.sla_pipe_config_layer_height();
    config->first_layer_height = msg.sla_pipe_config_first_layer_height();

    config->bottom_exposure_time = msg.sla_pipe_config_bottom_exposure_time();
    config->normal_exposure_time = msg.sla_pipe_config_normal_exposure_time();
    config->bottom_lift_distance = msg.sla_pipe_config_bottom_lift_distance();
    config->lift_distance = msg.sla_pipe_config_lift_distance();
    config->lift_speed = msg.sla_pipe_config_lift_speed();
    config->retract_speed = msg.sla_pipe_config_retract_speed();

    config->floor_raft_offset = msg.sla_pipe_config_floor_raft_offset();
    config->floor_border_width = msg.sla_pipe_config_floor_border_width();
    config->floor_fill_spacing = msg.sla_pipe_config_floor_fill_spacing();
    config->floor_fill_angle = msg.sla_pipe_config_floor_fill_angle();
    config->floor_border_count = msg.sla_pipe_config_floor_border_count();
    config->floor_use_convex_hull = msg.sla_pipe_config_floor_use_convex_hull() ? 1 : 0;

    config->enable_support = msg.sla_pipe_config_support_enable() ? 1 : 0;
    config->overhang_angle = msg.sla_pipe_config_overhang_angle();
    config->support_gap = msg.sla_pipe_config_support_gap();
    config->support_diameter = msg.sla_pipe_config_support_diameter();
    config->support_density = msg.sla_pipe_config_support_density();
    config->support_pattern = static_cast<HsBaSlaSupportPattern_t>(msg.sla_pipe_config_support_pattern());

    config->support_lua_script = DupString(msg.sla_pipe_config_support_lua_script());
    config->support_lua_func = DupString(msg.sla_pipe_config_support_lua_func());
    config->floor_lua_script = DupString(msg.sla_pipe_config_floor_lua_script());
    config->floor_lua_func = DupString(msg.sla_pipe_config_floor_lua_func());
    config->export_lua_script = DupString(msg.sla_pipe_config_export_lua_script());
    config->export_lua_func = DupString(msg.sla_pipe_config_export_lua_func());

    config->output_path = DupString(msg.sla_pipe_config_output_path());
    config->image_type = static_cast<HsBaSlaImageType_t>(msg.sla_pipe_config_output_image_type());
    config->image_width = msg.sla_pipe_config_output_image_width();
    config->image_height = msg.sla_pipe_config_output_image_height();
}

void MsgToSlaResult(const HsbaProto::sla_pipe_result& msg, HsBaSlaPipelineResult_t* result)
{
    result->success = msg.sla_pipe_result_success() ? 1 : 0;
    result->total_layers = msg.sla_pipe_result_total_layers();
    result->export_path = DupString(msg.sla_pipe_result_export_path());
    result->error_message = DupString(msg.sla_pipe_result_error_message());
    result->elapsed_seconds = msg.sla_pipe_result_elapsed_seconds();
}

void MsgToSlsConfig(const HsbaProto::sls_pipe_config& msg, HsBaSlsPipelineConfig_t* config)
{
    *config = HsBaSlsConfigDefault();

    config->model_name = DupString(msg.sls_pipe_config_model_name());
    config->model_path = DupString(msg.sls_pipe_config_model_path());

    config->layer_height = msg.sls_pipe_config_layer_height();
    config->first_layer_height = msg.sls_pipe_config_first_layer_height();

    config->laser_power = msg.sls_pipe_config_laser_power();
    config->scan_speed = msg.sls_pipe_config_scan_speed();
    config->hatch_spacing = msg.sls_pipe_config_hatch_spacing();
    config->hatch_rotation = msg.sls_pipe_config_hatch_rotation();
    config->bed_temperature = msg.sls_pipe_config_bed_temperature();

    config->export_lua_script = DupString(msg.sls_pipe_config_export_lua_script());
    config->export_lua_func = DupString(msg.sls_pipe_config_export_lua_func());

    config->output_path = DupString(msg.sls_pipe_config_output_path());
}

void MsgToSlsResult(const HsbaProto::sls_pipe_result& msg, HsBaSlsPipelineResult_t* result)
{
    result->success = msg.sls_pipe_result_success() ? 1 : 0;
    result->total_layers = msg.sls_pipe_result_total_layers();
    result->export_path = DupString(msg.sls_pipe_result_export_path());
    result->error_message = DupString(msg.sls_pipe_result_error_message());
    result->elapsed_seconds = msg.sls_pipe_result_elapsed_seconds();
}

}  // namespace HsBa::Slicer
