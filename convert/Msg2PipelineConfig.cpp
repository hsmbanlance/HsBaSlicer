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

void MsgToFileTransferConfig(const HsbaProto::file_transfer_pipe_config& msg, HsBaFileTransferPipelineConfig_t* config)
{
    *config = HsBaFileTransferConfigDefault();

    config->host = DupString(msg.file_transfer_config_host());
    config->port = DupString(msg.file_transfer_config_port());
    config->pool_size = msg.file_transfer_config_pool_size();

    const int count = msg.file_transfer_config_file_paths_size();
    if (count > 0)
    {
        config->file_count = count;
        config->file_paths = static_cast<const char**>(std::malloc(sizeof(char*) * static_cast<size_t>(count)));
        if (config->file_paths)
        {
            for (int i = 0; i < count; ++i)
            {
                const_cast<char**>(config->file_paths)[i] = DupString(msg.file_transfer_config_file_paths(i));
            }
        }
    }
}

void MsgToFileTransferResult(const HsbaProto::file_transfer_pipe_result& msg, HsBaFileTransferPipelineResult_t* result)
{
    result->success = msg.file_transfer_result_success() ? 1 : 0;
    result->files_transferred = msg.file_transfer_result_files_transferred();
    result->total_files = msg.file_transfer_result_total_files();
    result->error_message = DupString(msg.file_transfer_result_error_message());
    result->elapsed_seconds = msg.file_transfer_result_elapsed_seconds();
}

void MsgToCustomConfig(const HsbaProto::custom_pipe_config& msg, HsBaCustomPipelineConfig_t* config)
{
    *config = HsBaCustomConfigDefault();

    config->pipeline_lua_script = DupString(msg.custom_pipe_config_pipeline_lua_script());
    config->pipeline_lua_source = DupString(msg.custom_pipe_config_pipeline_lua_source());
    // Empty entry_func stays NULL: the Lib layer then falls back to "run_pipeline".
    config->entry_func = DupString(msg.custom_pipe_config_entry_func());
    config->config_json = DupString(msg.custom_pipe_config_config_json());

    config->model_name = DupString(msg.custom_pipe_config_model_name());
    config->model_path = DupString(msg.custom_pipe_config_model_path());

    config->output_path = DupString(msg.custom_pipe_config_output_path());
}

void MsgToCustomResult(const HsbaProto::custom_pipe_result& msg, HsBaCustomPipelineResult_t* result)
{
    result->success = msg.custom_pipe_result_success() ? 1 : 0;
    result->total_layers = msg.custom_pipe_result_total_layers();
    result->output_path = DupString(msg.custom_pipe_result_output_path());
    result->result_string = DupString(msg.custom_pipe_result_result_string());
    result->error_message = DupString(msg.custom_pipe_result_error_message());
    result->elapsed_seconds = msg.custom_pipe_result_elapsed_seconds();
}

void MsgToSlmConfig(const HsbaProto::slm_pipe_config& msg, HsBaSlmPipelineConfig_t* config)
{
    *config = HsBaSlmConfigDefault();

    config->model_name = DupString(msg.slm_pipe_config_model_name());
    config->model_path = DupString(msg.slm_pipe_config_model_path());

    config->layer_height = msg.slm_pipe_config_layer_height();
    config->first_layer_height = msg.slm_pipe_config_first_layer_height();

    config->laser_power = msg.slm_pipe_config_laser_power();
    config->scan_speed = msg.slm_pipe_config_scan_speed();
    config->hatch_spacing = msg.slm_pipe_config_hatch_spacing();
    config->hatch_rotation = msg.slm_pipe_config_hatch_rotation();
    config->bed_temperature = msg.slm_pipe_config_bed_temperature();

    config->material = static_cast<HsBaSlmMaterial_t>(msg.slm_pipe_config_material());
    config->light_source = static_cast<HsBaSlmLight_t>(msg.slm_pipe_config_light_source());
    config->protect_gas = static_cast<HsBaMetalProtectGas_t>(msg.slm_pipe_config_protect_gas());

    config->export_lua_script = DupString(msg.slm_pipe_config_export_lua_script());
    config->export_lua_func = DupString(msg.slm_pipe_config_export_lua_func());

    config->output_path = DupString(msg.slm_pipe_config_output_path());
}

void MsgToSlmResult(const HsbaProto::slm_pipe_result& msg, HsBaSlmPipelineResult_t* result)
{
    result->success = msg.slm_pipe_result_success() ? 1 : 0;
    result->total_layers = msg.slm_pipe_result_total_layers();
    result->export_path = DupString(msg.slm_pipe_result_export_path());
    result->error_message = DupString(msg.slm_pipe_result_error_message());
    result->elapsed_seconds = msg.slm_pipe_result_elapsed_seconds();
}

void MsgToLomConfig(const HsbaProto::lom_pipe_config& msg, HsBaLomPipelineConfig_t* config)
{
    *config = HsBaLomConfigDefault();

    config->model_name = DupString(msg.lom_pipe_config_model_name());
    config->model_path = DupString(msg.lom_pipe_config_model_path());

    config->layer_height = msg.lom_pipe_config_layer_height();
    config->first_layer_height = msg.lom_pipe_config_first_layer_height();

    config->cut_speed = msg.lom_pipe_config_cut_speed();
    config->cut_margin = msg.lom_pipe_config_cut_margin();
    config->cut_power = msg.lom_pipe_config_cut_power();

    config->bond_temperature = msg.lom_pipe_config_bond_temperature();
    config->bond_pressure = msg.lom_pipe_config_bond_pressure();
    config->bond_time = msg.lom_pipe_config_bond_time();

    config->seal_contour = msg.lom_pipe_config_seal_contour() ? 1 : 0;
    config->cut_mode = static_cast<HsBaLomCutMode_t>(msg.lom_pipe_config_cut_mode());

    config->export_lua_script = DupString(msg.lom_pipe_config_export_lua_script());
    config->export_lua_func = DupString(msg.lom_pipe_config_export_lua_func());

    config->output_path = DupString(msg.lom_pipe_config_output_path());
}

void MsgToLomResult(const HsbaProto::lom_pipe_result& msg, HsBaLomPipelineResult_t* result)
{
    result->success = msg.lom_pipe_result_success() ? 1 : 0;
    result->total_layers = msg.lom_pipe_result_total_layers();
    result->export_path = DupString(msg.lom_pipe_result_export_path());
    result->error_message = DupString(msg.lom_pipe_result_error_message());
    result->elapsed_seconds = msg.lom_pipe_result_elapsed_seconds();
}

void MsgToTdpConfig(const HsbaProto::tdp_pipe_config& msg, HsBaTdpPipelineConfig_t* config)
{
    *config = HsBaTdpConfigDefault();

    config->model_name = DupString(msg.tdp_pipe_config_model_name());
    config->model_path = DupString(msg.tdp_pipe_config_model_path());

    config->layer_height = msg.tdp_pipe_config_layer_height();
    config->first_layer_height = msg.tdp_pipe_config_first_layer_height();

    config->head_count = msg.tdp_pipe_config_head_count();
    config->drop_spacing = msg.tdp_pipe_config_drop_spacing();
    config->binder_saturation = msg.tdp_pipe_config_binder_saturation();
    config->ink_curing_time = msg.tdp_pipe_config_ink_curing_time();
    config->bed_temperature = msg.tdp_pipe_config_bed_temperature();

    config->binder_mode = static_cast<HsBaTdpBinderMode_t>(msg.tdp_pipe_config_binder_mode());

    config->export_lua_script = DupString(msg.tdp_pipe_config_export_lua_script());
    config->export_lua_func = DupString(msg.tdp_pipe_config_export_lua_func());

    config->output_path = DupString(msg.tdp_pipe_config_output_path());
}

void MsgToTdpResult(const HsbaProto::tdp_pipe_result& msg, HsBaTdpPipelineResult_t* result)
{
    result->success = msg.tdp_pipe_result_success() ? 1 : 0;
    result->total_layers = msg.tdp_pipe_result_total_layers();
    result->export_path = DupString(msg.tdp_pipe_result_export_path());
    result->error_message = DupString(msg.tdp_pipe_result_error_message());
    result->elapsed_seconds = msg.tdp_pipe_result_elapsed_seconds();
}

void MsgToWaamConfig(const HsbaProto::waam_pipe_config& msg, HsBaWaamPipelineConfig_t* config)
{
    *config = HsBaWaamConfigDefault();

    config->model_name = DupString(msg.waam_pipe_config_model_name());
    config->model_path = DupString(msg.waam_pipe_config_model_path());

    config->layer_height = msg.waam_pipe_config_layer_height();
    config->first_layer_height = msg.waam_pipe_config_first_layer_height();

    config->bead_width = msg.waam_pipe_config_bead_width();
    config->travel_speed = msg.waam_pipe_config_travel_speed();

    config->wire_feed_speed = msg.waam_pipe_config_wire_feed_speed();
    config->arc_current = msg.waam_pipe_config_arc_current();
    config->arc_voltage = msg.waam_pipe_config_arc_voltage();
    config->gas_flow_rate = msg.waam_pipe_config_gas_flow_rate();

    config->material = static_cast<HsBaWaamMaterial_t>(msg.waam_pipe_config_material());
    config->welding_process = static_cast<HsBaWaamWeldProcess_t>(msg.waam_pipe_config_welding_process());
    config->protection = static_cast<HsBaWaamProtection_t>(msg.waam_pipe_config_protection());
    config->protect_gas = static_cast<HsBaMetalProtectGas_t>(msg.waam_pipe_config_protect_gas());
    config->interpass_temperature = msg.waam_pipe_config_interpass_temperature();

    config->robot_type = static_cast<HsBaWaamRobotType_t>(msg.waam_pipe_config_robot_type());

    config->path_lua_script = DupString(msg.waam_pipe_config_path_lua_script());
    config->path_lua_func = DupString(msg.waam_pipe_config_path_lua_func());

    config->output_path = DupString(msg.waam_pipe_config_output_path());
}

void MsgToWaamResult(const HsbaProto::waam_pipe_result& msg, HsBaWaamPipelineResult_t* result)
{
    result->success = msg.waam_pipe_result_success() ? 1 : 0;
    result->total_layers = msg.waam_pipe_result_total_layers();
    result->output_path = DupString(msg.waam_pipe_result_output_path());
    result->error_message = DupString(msg.waam_pipe_result_error_message());
    result->elapsed_seconds = msg.waam_pipe_result_elapsed_seconds();
}

}  // namespace HsBa::Slicer
