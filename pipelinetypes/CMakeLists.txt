# HsBaPipelineTypes: header-only INTERFACE library providing standalone
# C-compatible pipeline config/result type definitions.
# No dependency on DllHsBaSlicer or dllexport.h.
add_library(HsBaPipelineTypes INTERFACE)
target_include_directories(HsBaPipelineTypes INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
