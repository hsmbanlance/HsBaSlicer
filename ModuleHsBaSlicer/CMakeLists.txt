# ---------------------------------------------------------------------------
# ModuleHsBaSlicer: C++20 module wrapper for LibHsBaSlicer
# ---------------------------------------------------------------------------
# Provides `import hsba.slicer;` with a class-based C++ API.
# Internally forwards to LibHsBaSlicer free functions.
# ---------------------------------------------------------------------------

# Single-file module: interface + implementation in one .cppm
# (avoids MSVC C2572 errors from implicit-import in separate impl units)
# Always STATIC: the module wrapper is a thin C++ API layer; consumers link
# it directly. Avoids DLL link issues (oleaut32.lib path) on some environments.
add_library(ModuleHsBaSlicer STATIC
    module_anchor.cpp  # ensures archiver produces .lib
)

# Module interface unit (PUBLIC: consumers get BMI via linking)
target_sources(ModuleHsBaSlicer PUBLIC
    FILE_SET hsba_slicer_modules TYPE CXX_MODULES FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/hsba_slicer.cppm
)

# Link against LibHsBaSlicer (PUBLIC: static lib consumers need transitive deps)
target_link_libraries(ModuleHsBaSlicer PUBLIC
    LibHsBaSlicer
    HsBaPipelineTypes
    Eigen3::Eigen
    PkgConfig::Clipper2
    ${LUA_LIBRARIES}
)

target_include_directories(ModuleHsBaSlicer PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${LUA_INCLUDE_DIR}
)

# MSVC C5050 fix: CGAL propagates /fp:strict and _SCL_SECURE_NO_WARNINGS via
# INTERFACE to ModuleHsBaSlicer (through LibHsBaSlicer). Module consumers must
# compile with the SAME flags, otherwise importing the BMI triggers C2572
# (std redefinition) errors. Re-export them as PUBLIC.
target_compile_definitions(ModuleHsBaSlicer PUBLIC
    $<$<CXX_COMPILER_ID:MSVC>:_SCL_SECURE_NO_DEPRECATE _SCL_SECURE_NO_WARNINGS>
)
target_compile_options(ModuleHsBaSlicer PUBLIC
    $<$<CXX_COMPILER_ID:MSVC>:/fp:strict /fp:except->
)
