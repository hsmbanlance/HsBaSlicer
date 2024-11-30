add_library(DllHsBaSlicer SHARED 
dllexport.h
../cpp.hint
initialize.h
initialize.cpp
)

target_compile_definitions(DllHsBaSlicer PRIVATE HSBA_SLICER_EXPORTS)