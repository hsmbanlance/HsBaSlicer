if(NOT BUILD_SHARED_LIBS)
    add_library(LibHsBaSlicer STATIC 
    pch_headers.hpp
    version_info.hpp
    Slice/mesh_slice.hpp
    Preprocess/model_preprocess.hpp
    Support/fdm_support.hpp
    Fill/polygon_fill.hpp
    Path/path_generator.hpp
    Path/sls_export.hpp
    Floor/sla_floor.hpp
    Transfer/file_transfer.hpp
    version_info.cpp
    Slice/mesh_slice.cpp
    Preprocess/model_preprocess.cpp
    Support/fdm_support.cpp
    Fill/polygon_fill.cpp
    Path/path_generator.cpp
    Path/sls_export.cpp
    Floor/sla_floor.cpp
    Transfer/file_transfer.cpp
    Extends/LuaAddFunction.hpp
    Extends/LuaAddFunction.cpp
    )
else()
    add_library(LibHsBaSlicer SHARED 
    pch_headers.hpp
    version_info.hpp
    Slice/mesh_slice.hpp
    Preprocess/model_preprocess.hpp
    Support/fdm_support.hpp
    Fill/polygon_fill.hpp
    Path/path_generator.hpp
    Path/sls_export.hpp
    Floor/sla_floor.hpp
    Transfer/file_transfer.hpp
    version_info.cpp
    Slice/mesh_slice.cpp
    Preprocess/model_preprocess.cpp
    Support/fdm_support.cpp
    Fill/polygon_fill.cpp
    Path/path_generator.cpp
    Path/sls_export.cpp
    Floor/sla_floor.cpp
    Transfer/file_transfer.cpp
    Extends/LuaAddFunction.hpp
    Extends/LuaAddFunction.cpp
    )
    # Define the export macro for Windows
    target_compile_definitions(LibHsBaSlicer PRIVATE HSBA_SLICER_EXPORTS)
    # Add the export header to the target
    target_sources(LibHsBaSlicer PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/export.h>
    )
endif()

# PUBLIC: types/symbols exposed in LibHsBaSlicer's public headers
target_link_libraries(LibHsBaSlicer PUBLIC
HsBaSlicerBase
HsBaSlicerMesh
HsBaSlicer2D
HsBaSupport
HsBaPaths
)

# PRIVATE: internal implementation only
target_link_libraries(LibHsBaSlicer PRIVATE
HsBaSlicerUtils
HsBaPreprocess
HsBaVersion
HsBaSlicerFileOperator
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
