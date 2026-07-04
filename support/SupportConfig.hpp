#pragma once
#ifndef HSBA_SLICER_SUPPORT_CONFIG_HPP
#define HSBA_SLICER_SUPPORT_CONFIG_HPP

namespace HsBa::Slicer::Support
{
/**
 * @brief General support generation configuration.
 */
struct SupportConfig
{
    float overhang_angle_threshold = 45.0f;  ///< Overhang angle threshold in degrees
    float layer_height = 0.2f;               ///< Layer height in mm
    float support_gap = 0.5f;                ///< Gap between support and model in mm
    float support_diameter = 2.0f;           ///< Support column diameter in mm
    float support_density = 0.3f;            ///< Support fill density [0,1]
    int support_pattern = 0;                 ///< 0=Plane, 1=Tree, 2=Honeycomb
    float tree_branch_angle = 30.0f;         ///< Tree branch angle in degrees
    float tree_max_branch_radius = 5.0f;     ///< Tree max branch radius in mm
    float honeycomb_cell_size = 5.0f;        ///< Honeycomb cell size in mm
};

/**
 * @brief FDM-specific support configuration.
 */
struct FdmSupportConfig : SupportConfig
{
    int interface_layers = 2;         ///< Number of interface layers between support and model
    float interface_density = 0.5f;   ///< Interface layer fill density [0,1]
};

/**
 * @brief SLA-specific support configuration.
 */
struct SlaSupportConfig : SupportConfig
{
    float tip_diameter = 0.3f;    ///< Support tip diameter in mm (small for easy removal)
    float raft_thickness = 0.5f;  ///< Raft layer thickness in mm
};
}  // namespace HsBa::Slicer::Support

#endif  // HSBA_SLICER_SUPPORT_CONFIG_HPP
