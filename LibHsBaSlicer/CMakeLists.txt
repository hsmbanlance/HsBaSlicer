if(NOT BUILD_SHARED_LIBS)
    add_library(LibHsBaSlicer STATIC 
    pch_headers.hpp
    version_info.hpp
    version_info.cpp
    Slice/mesh_slice.hpp
    Slice/mesh_slice.cpp
    Preprocess/model_preprocess.hpp
    Preprocess/model_preprocess.cpp
    Support/fdm_support.hpp
    Support/fdm_support.cpp
    Fill/polygon_fill.hpp
    Fill/polygon_fill.cpp
    Path/path_generator.hpp
    Path/path_generator.cpp
    Floor/sla_floor.hpp
    Floor/sla_floor.cpp
    )
else()
    add_library(LibHsBaSlicer SHARED 
    pch_headers.hpp
    version_info.hpp
    version_info.cpp
    Slice/mesh_slice.hpp
    Slice/mesh_slice.cpp
    Preprocess/model_preprocess.hpp
    Preprocess/model_preprocess.cpp
    Support/fdm_support.hpp
    Support/fdm_support.cpp
    Fill/polygon_fill.hpp
    Fill/polygon_fill.cpp
    Path/path_generator.hpp
    Path/path_generator.cpp
    Floor/sla_floor.hpp
    Floor/sla_floor.cpp
    )
    # Define the export macro for Windows
    target_compile_definitions(LibHsBaSlicer PRIVATE HSBA_SLICER_EXPORTS)
    # Add the export header to the target
    target_sources(LibHsBaSlicer PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/export.h>
    )
endif()

target_link_libraries(LibHsBaSlicer PRIVATE
HsBaSlicerBase
HsBaSlicerUtils
HsBaSlicerMesh
HsBaSlicer2D
HsBaPreprocess
HsBaSupport
HsBaPaths
HsBaVersion
)

# OCCT / CAD kernel is not available on Android, iOS, or game consoles
if(NOT ANDROID AND NOT IOS AND NOT HSBA_GAME_CONSOLE)
    target_link_libraries(LibHsBaSlicer PRIVATE HsBaSlicerCADModel)
endif()

target_precompile_headers(LibHsBaSlicer PRIVATE pch_headers.hpp)

# Add the export header to the target
if(BUILD_SHARED_LIBS)
    target_sources(LibHsBaSlicer PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../DllHsBaSlicer/dllexport.h>
    )
endif()