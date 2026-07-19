#include "PipelineConfig2Msg.hpp"

namespace HsBa::Slicer
{

void FdmConfigToMsg(const HsBaFdmPipelineConfig_t& config, HsbaProto::msg_fdm_pipeline_config* msg)
{
    if (config.model_name)
        msg->set_fdm_pipe_config_model_name(config.model_name);
    if (config.model_path)
        msg->set_fdm_pipe_config_model_path(config.model_path);

    msg->set_fdm_pipe_config_layer_height(config.layer_height);
    msg->set_fdm_pipe_config_first_layer_height(config.first_layer_height);

    msg->set_fdm_pipe_config_fill_space(config.fill_spacing);
    msg->set_fdm_pipe_config_fill_type(static_cast<HsbaProto::msg_fdm_filltype>(config.fill_mode));
    msg->set_fdm_pipe_config_fill_angle(config.fill_angle);
    msg->set_fdm_pipe_config_wall_count(config.wall_count);
    msg->set_fdm_pipe_config_top_layer_count(config.top_layer_count);
    msg->set_fdm_pipe_config_bottom_layer_count(config.bottom_layer_count);

    msg->set_fdm_pipe_config_support_enable(config.enable_support != 0);
    msg->set_fdm_pipe_config_support_angle(config.overhang_angle);
    msg->set_fdm_pipe_config_support_gap(config.support_gap);
    msg->set_fdm_pipe_config_support_support_diameter(config.support_diameter);
    msg->set_fdm_pipe_config_support_density(config.support_density);
    msg->set_fdm_pipe_config_support_pattern(static_cast<HsbaProto::msg_fdm_support_pattern>(config.support_pattern));
    msg->set_fdm_pipe_config_interface_layers(config.interface_layers);
    msg->set_fdm_pipe_config_interface_density(config.interface_density);

    msg->set_fdm_pipe_config_line_width(config.line_width);
    msg->set_fdm_pipe_config_print_speed(config.print_speed);
    msg->set_fdm_pipe_config_travel_speed(config.travel_speed);
    msg->set_fdm_pipe_config_extrusion_multiplier(config.extrusion_multiplier);

    if (config.support_lua_script)
        msg->set_fdm_pipe_config_support_lua_script(config.support_lua_script);
    if (config.support_lua_func)
        msg->set_fdm_pipe_config_support_lua_func(config.support_lua_func);
    if (config.infill_lua_script)
        msg->set_fdm_pipe_config_infill_lua_script(config.infill_lua_script);
    if (config.infill_lua_func)
        msg->set_fdm_pipe_config_infill_lua_func(config.infill_lua_func);

    if (config.output_path)
        msg->set_fdm_pipe_config_output_path(config.output_path);
}

void FdmResultToMsg(const HsBaFdmPipelineResult_t& result, HsbaProto::msg_fdm_pipe_result* msg)
{
    msg->set_fdm_pipe_result_success(result.success != 0);
    msg->set_fdm_pipe_result_total_layers(result.total_layers);
    if (result.gcode_content)
        msg->set_fdm_pipe_result_gcode_content(result.gcode_content);
    if (result.error_message)
        msg->set_fdm_pipe_result_error_message(result.error_message);
    msg->set_fdm_pipe_result_elapsed_seconds(result.elapsed_seconds);
}

void SlaConfigToMsg(const HsBaSlaPipelineConfig_t& config, HsbaProto::sla_pipe_config* msg)
{
    if (config.model_name)
        msg->set_sla_pipe_config_model_name(config.model_name);
    if (config.model_path)
        msg->set_sla_pipe_config_model_path(config.model_path);

    msg->set_sla_pipe_config_layer_height(config.layer_height);
    msg->set_sla_pipe_config_first_layer_height(config.first_layer_height);

    msg->set_sla_pipe_config_bottom_exposure_time(config.bottom_exposure_time);
    msg->set_sla_pipe_config_normal_exposure_time(config.normal_exposure_time);
    msg->set_sla_pipe_config_bottom_lift_distance(config.bottom_lift_distance);
    msg->set_sla_pipe_config_lift_distance(config.lift_distance);
    msg->set_sla_pipe_config_lift_speed(config.lift_speed);
    msg->set_sla_pipe_config_retract_speed(config.retract_speed);

    msg->set_sla_pipe_config_floor_raft_offset(config.floor_raft_offset);
    msg->set_sla_pipe_config_floor_border_width(config.floor_border_width);
    msg->set_sla_pipe_config_floor_fill_spacing(config.floor_fill_spacing);
    msg->set_sla_pipe_config_floor_fill_angle(config.floor_fill_angle);
    msg->set_sla_pipe_config_floor_border_count(config.floor_border_count);
    msg->set_sla_pipe_config_floor_use_convex_hull(config.floor_use_convex_hull != 0);

    msg->set_sla_pipe_config_support_enable(config.enable_support != 0);
    msg->set_sla_pipe_config_overhang_angle(config.overhang_angle);
    msg->set_sla_pipe_config_support_gap(config.support_gap);
    msg->set_sla_pipe_config_support_diameter(config.support_diameter);
    msg->set_sla_pipe_config_support_density(config.support_density);
    msg->set_sla_pipe_config_support_pattern(static_cast<HsbaProto::sla_support_pattern>(config.support_pattern));

    if (config.support_lua_script)
        msg->set_sla_pipe_config_support_lua_script(config.support_lua_script);
    if (config.support_lua_func)
        msg->set_sla_pipe_config_support_lua_func(config.support_lua_func);
    if (config.floor_lua_script)
        msg->set_sla_pipe_config_floor_lua_script(config.floor_lua_script);
    if (config.floor_lua_func)
        msg->set_sla_pipe_config_floor_lua_func(config.floor_lua_func);
    if (config.export_lua_script)
        msg->set_sla_pipe_config_export_lua_script(config.export_lua_script);
    if (config.export_lua_func)
        msg->set_sla_pipe_config_export_lua_func(config.export_lua_func);

    if (config.output_path)
        msg->set_sla_pipe_config_output_path(config.output_path);
    msg->set_sla_pipe_config_output_image_type(static_cast<HsbaProto::sla_image_type>(config.image_type));
    msg->set_sla_pipe_config_output_image_width(config.image_width);
    msg->set_sla_pipe_config_output_image_height(config.image_height);
}

void SlaResultToMsg(const HsBaSlaPipelineResult_t& result, HsbaProto::sla_pipe_result* msg)
{
    msg->set_sla_pipe_result_success(result.success != 0);
    msg->set_sla_pipe_result_total_layers(result.total_layers);
    if (result.export_path)
        msg->set_sla_pipe_result_export_path(result.export_path);
    if (result.error_message)
        msg->set_sla_pipe_result_error_message(result.error_message);
    msg->set_sla_pipe_result_elapsed_seconds(result.elapsed_seconds);
}

}  // namespace HsBa::Slicer
