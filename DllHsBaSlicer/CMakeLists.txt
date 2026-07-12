add_library(DllHsBaSlicer SHARED 
dllexport.h
../cpp.hint
initialize.h
initialize.cpp
fdm_pipeline.h
fdm_pipeline.cpp
sla_pipeline.h
sla_pipeline.cpp
version_info.h
version_info.cpp
)

target_compile_definitions(DllHsBaSlicer PRIVATE HSBA_SLICER_EXPORTS)

target_link_libraries(DllHsBaSlicer PRIVATE
LibHsBaSlicer
HsBaSlicerBase
HsBaSlicerUtils
HsBaSlicer2D
HsBaPreprocess
HsBaSupport
HsBaPaths
HsBaSlicerFileOperator
HsBaVersion
)