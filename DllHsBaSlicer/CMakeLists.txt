# DllHsBaSlicer: C export layer for cross-language integration
# On iOS, build as STATIC because iOS apps cannot load arbitrary dylibs.
# On other platforms, build as SHARED for dynamic loading by C#/Java/Python etc.
if(IOS OR CMAKE_SYSTEM_NAME STREQUAL "iOS")
  add_library(DllHsBaSlicer STATIC 
  dllexport.h
  ../cpp.hint
  initialize.h
  initialize.cpp
  fdm_pipeline.h
  fdm_pipeline.cpp
  sla_pipeline.h
  sla_pipeline.cpp
  sls_pipeline.h
  sls_pipeline.cpp
  file_transfer_pipeline.h
  file_transfer_pipeline.cpp
  model_preprocess.h
  model_preprocess.cpp
  pipeline_convert.h
  pipeline_convert.cpp
  lua_register.h
  lua_register.cpp
  version_info.h
  version_info.cpp
  )
else()
  add_library(DllHsBaSlicer SHARED 
  dllexport.h
  ../cpp.hint
  initialize.h
  initialize.cpp
  fdm_pipeline.h
  fdm_pipeline.cpp
  sla_pipeline.h
  sla_pipeline.cpp
  sls_pipeline.h
  sls_pipeline.cpp
  file_transfer_pipeline.h
  file_transfer_pipeline.cpp
  model_preprocess.h
  model_preprocess.cpp
  pipeline_convert.h
  pipeline_convert.cpp
  lua_register.h
  lua_register.cpp
  version_info.h
  version_info.cpp
  )
endif()

target_compile_definitions(DllHsBaSlicer PRIVATE HSBA_SLICER_EXPORTS)

target_link_libraries(DllHsBaSlicer PRIVATE
LibHsBaSlicer
HsBaPipelineTypes
# Transitive static dependencies required for link resolution.
# Code-level constraint: DllHsBaSlicer only calls LibHsBaSlicer exported functions.
HsBaSlicerBase
HsBaSlicerUtils
HsBaSlicerMesh
HsBaSlicer2D
HsBaPreprocess
HsBaSupport
HsBaPaths
HsBaSlicerFileOperator
HsBaVersion
HsBaSlicerConverter
HsBaSlicerProto
)