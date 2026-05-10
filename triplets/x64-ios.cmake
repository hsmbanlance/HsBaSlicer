set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME iOS)

set(VCPKG_ENV_PASSTHROUGH 
    "cross_compiling;ac_cv_prog_cc_cross;ac_cv_func_malloc_0_nonnull;ac_cv_func_realloc_0_nonnull")

set(ENV{cross_compiling} "yes")
set(ENV{ac_cv_prog_cc_cross} "yes")
set(ENV{ac_cv_func_malloc_0_nonnull} "yes")
set(ENV{ac_cv_func_realloc_0_nonnull} "yes")