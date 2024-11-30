add_library(LibHsBaSlicer STATIC 
pch_headers.hpp
Slice/mesh_slice.hpp
Slice/mesh_slice.cpp
)

target_link_libraries(LibHsBaSlicer PRIVATE
HsBaSlicerBase
HsBaSlicerUtils
HsBaSlicerMesh
HsBaSlicerCADModel
HsBaSlicer2D
)

target_precompile_headers(LibHsBaSlicer PRIVATE pch_headers.hpp)