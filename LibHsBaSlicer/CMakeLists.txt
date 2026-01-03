if(NOT BUILD_SHARED_LIBS)
    add_library(LibHsBaSlicer STATIC 
    pch_headers.hpp
    Slice/mesh_slice.hpp
    Slice/mesh_slice.cpp
    )
else()
    add_library(LibHsBaSlicer SHARED 
    pch_headers.hpp
    Slice/mesh_slice.hpp
    Slice/mesh_slice.cpp
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
HsBaSlicerCADModel
HsBaSlicer2D
)

target_precompile_headers(LibHsBaSlicer PRIVATE pch_headers.hpp)

# Add the export header to the target
if(BUILD_SHARED_LIBS)
    target_sources(LibHsBaSlicer PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../DllHsBaSlicer/dllexport.h>
    )
endif()