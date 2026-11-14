string(REPLACE "." "_" VERSION_STR "V${VERSION}")
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO Open-Cascade-SAS/OCCT
    REF "${VERSION_STR}"
    SHA512 bbe7099071cbf5397940ebc6e66ec05f8023d5e5aae6142870e14b93aa6f8f94c30980ef421e717f0fbfbc23b3520c3ccfe8a939c4caba3ccbf325060e26eb52
    HEAD_REF master
    PATCHES
        0001-cmake-keep-build-use-vcpkg-explicit.patch
        0002-cmake-load-exported-package-dependencies.patch
        0003-image-remove-freeimage-msvc-autolink.patch
        0004-cmake-add-additional-path-extraction-for-OpenCASCADE.patch
        0005-drop-bin-letter.patch
)

if (VCPKG_LIBRARY_LINKAGE STREQUAL "dynamic")
    set(BUILD_TYPE "Shared")
else()
    set(BUILD_TYPE "Static")
endif()

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        freeimage   USE_FREEIMAGE
        freetype    USE_FREETYPE
        rapidjson   USE_RAPIDJSON
        tbb         USE_TBB
        tcl         USE_TCL
        vtk         USE_VTK
)

# The 'tcl' feature turns on the DRAW Test Harness (BUILD_MODULE_Draw), which upstream
# vcpkg keeps OFF with the comment that it "requires TCL 8.6 and TK 8.6 specifically".
# Two facts make it work with the Tcl 9 port:
#   * only TKDraw/TKTopTest need Tcl, and they use the stable public API; Tk stays OFF
#     (Draw_Window.cxx guards every <tk.h> include with _TK), so no tk port is needed;
#   * the real blocker is adm/cmake/occt_csf.cmake hard-coding the Windows import name
#     to "tcl86" while adm/cmake/tcl.cmake searches with NO_DEFAULT_PATH. We bypass the
#     name guess by feeding the concrete 3RDPARTY_TCL_* cache entries of the tcl port.
# Cost of the feature: BUILD_MODULE_Draw builds the whole Draw module, and OpenCASCADE's
# config file then lists every toolkit of it (TKXSDRAW* and friends) in OpenCASCADE_LIBRARIES,
# not just the two that need Tcl. Consumers link that list, so the feature is what grows the
# binary and the set of LGPL components in the product - see cadmodel/CMakeLists.txt.
set(HSBA_TCL_OPTIONS_RELEASE "")
set(HSBA_TCL_OPTIONS_DEBUG "")
set(BUILD_MODULE_Draw OFF)
if ("tcl" IN_LIST FEATURES)
    set(BUILD_MODULE_Draw ON)

    macro(hsba_find_tcl_implib out_var search_root)
        file(GLOB HSBA_TCL_IMPLIBS
            "${search_root}/lib/tcl[0-9]*.lib"       # x64-windows: tcl90.lib
            "${search_root}/lib/libtcl[0-9]*.so"      # Linux
            "${search_root}/lib/libtcl[0-9]*.dylib")  # macOS
        list(FILTER HSBA_TCL_IMPLIBS EXCLUDE REGEX "tclstub") # tclstub.lib is an extension stub
        if(NOT HSBA_TCL_IMPLIBS)
            message(FATAL_ERROR "Feature 'tcl' is enabled but no Tcl import library was found under ${search_root}/lib")
        endif()
        # Newest version wins: this portfile relies on Tcl 9 behaviour (Tk stays off), so a
        # leftover tcl86 library must never be preferred over tcl90 by a lexicographic sort.
        list(SORT HSBA_TCL_IMPLIBS ORDER DESCENDING)
        list(LENGTH HSBA_TCL_IMPLIBS HSBA_TCL_IMPLIB_COUNT)
        if(HSBA_TCL_IMPLIB_COUNT GREATER 1)
            message(STATUS "Multiple Tcl import libraries under ${search_root}/lib, using the newest: ${HSBA_TCL_IMPLIBS}")
        endif()
        list(GET HSBA_TCL_IMPLIBS 0 ${out_var})
    endmacro()

    hsba_find_tcl_implib(HSBA_TCL_RELEASE_LIB "${CURRENT_INSTALLED_DIR}")
    hsba_find_tcl_implib(HSBA_TCL_DEBUG_LIB "${CURRENT_INSTALLED_DIR}/debug")
    get_filename_component(HSBA_TCL_RELEASE_LIBDIR "${HSBA_TCL_RELEASE_LIB}" DIRECTORY)
    get_filename_component(HSBA_TCL_DEBUG_LIBDIR "${HSBA_TCL_DEBUG_LIB}" DIRECTORY)

    # Every entry is quoted as a whole: the install root may well contain spaces, and an
    # unquoted -DVAR=path would be split into two arguments. OCCT then rejects the mismatching
    # 3RDPARTY_TCL_* set silently (COMPLIANCE_PRODUCT_CONSISTENCY empties it without warning),
    # which surfaces much later as an unresolved Tcl symbol while linking TKDraw.
    set(HSBA_TCL_OPTIONS_RELEASE
        "-D3RDPARTY_TCL_DIR=${CURRENT_INSTALLED_DIR}"
        "-D3RDPARTY_TCL_INCLUDE_DIR=${CURRENT_INSTALLED_DIR}/include"
        "-D3RDPARTY_TCL_LIBRARY=${HSBA_TCL_RELEASE_LIB}"
        "-D3RDPARTY_TCL_LIBRARY_DIR=${HSBA_TCL_RELEASE_LIBDIR}")
    set(HSBA_TCL_OPTIONS_DEBUG
        "-D3RDPARTY_TCL_DIR=${CURRENT_INSTALLED_DIR}"
        "-D3RDPARTY_TCL_INCLUDE_DIR=${CURRENT_INSTALLED_DIR}/include"
        "-D3RDPARTY_TCL_LIBRARY=${HSBA_TCL_DEBUG_LIB}"
        "-D3RDPARTY_TCL_LIBRARY_DIR=${HSBA_TCL_DEBUG_LIBDIR}")

    if(VCPKG_TARGET_IS_WINDOWS AND VCPKG_LIBRARY_LINKAGE STREQUAL "dynamic")
        # OCCT only needs the DLL location for runtime search paths; TKDraw links the
        # import library above. Debug and release trees must not be swapped.
        macro(hsba_find_tcl_dll out_var search_root)
            file(GLOB HSBA_TCL_DLLS "${search_root}/bin/tcl[0-9]*.dll")
            if(NOT HSBA_TCL_DLLS)
                message(FATAL_ERROR "Feature 'tcl' is enabled but no Tcl runtime DLL was found under ${search_root}/bin")
            endif()
            list(SORT HSBA_TCL_DLLS ORDER DESCENDING)
            list(GET HSBA_TCL_DLLS 0 ${out_var})
        endmacro()

        hsba_find_tcl_dll(HSBA_TCL_RELEASE_DLL "${CURRENT_INSTALLED_DIR}")
        hsba_find_tcl_dll(HSBA_TCL_DEBUG_DLL "${CURRENT_INSTALLED_DIR}/debug")
        get_filename_component(HSBA_TCL_RELEASE_DLLDIR "${HSBA_TCL_RELEASE_DLL}" DIRECTORY)
        get_filename_component(HSBA_TCL_DEBUG_DLLDIR "${HSBA_TCL_DEBUG_DLL}" DIRECTORY)
        set(HSBA_TCL_OPTIONS_RELEASE ${HSBA_TCL_OPTIONS_RELEASE}
            "-D3RDPARTY_TCL_DLL=${HSBA_TCL_RELEASE_DLL}"
            "-D3RDPARTY_TCL_DLL_DIR=${HSBA_TCL_RELEASE_DLLDIR}")
        set(HSBA_TCL_OPTIONS_DEBUG ${HSBA_TCL_OPTIONS_DEBUG}
            "-D3RDPARTY_TCL_DLL=${HSBA_TCL_DEBUG_DLL}"
            "-D3RDPARTY_TCL_DLL_DIR=${HSBA_TCL_DEBUG_DLLDIR}")
    endif()
endif()

# We use the Unix install layout for Windows as it matches vcpkg
vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        ${FEATURE_OPTIONS}
        -DBUILD_LIBRARY_TYPE=${BUILD_TYPE}
        -DBUILD_MODULE_Draw=${BUILD_MODULE_Draw}
        -DBUILD_DOC_Overview=OFF
        -DINSTALL_DIR_LAYOUT=Unix
        -DINSTALL_DIR_DOC=share/trash
        -DINSTALL_DIR_SCRIPT=share/trash # not relocatable
        -DINSTALL_TEST_CASES=OFF
        -DUSE_TK=OFF
    OPTIONS_RELEASE
        ${HSBA_TCL_OPTIONS_RELEASE}
    OPTIONS_DEBUG
        ${HSBA_TCL_OPTIONS_DEBUG}
)

vcpkg_cmake_install()
vcpkg_copy_pdbs()
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/opencascade)

#make occt includes relative to source_file
file(GLOB extra_headers
    LIST_DIRECTORIES false
    RELATIVE "${CURRENT_PACKAGES_DIR}/include/opencascade"
    "${CURRENT_PACKAGES_DIR}/include/opencascade/*.h"
)
list(JOIN extra_headers "|" extra_headers)
file(GLOB files "${CURRENT_PACKAGES_DIR}/include/opencascade/*.[hgl]xx")
foreach(file_name IN LISTS files)
    vcpkg_replace_string("${file_name}" "(# *include) <([a-zA-Z0-9_]*[.][hgl]xx|${extra_headers})>" [[\1 "\2"]] REGEX IGNORE_UNCHANGED)
endforeach()

if(VCPKG_LIBRARY_LINKAGE STREQUAL "static")
    vcpkg_replace_string("${CURRENT_PACKAGES_DIR}/include/opencascade/Standard_Macro.hxx" "defined(OCCT_STATIC_BUILD)" "(1)")
endif()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/share/opencascade/samples/qt")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/share/trash")

if ("tcl" IN_LIST FEATURES)
    # DRAWEXE is the interactive Tcl/Tk shell of the Draw module. Embedding the DRAW
    # commands needs only TKDraw/TKTopTest, so the standalone executable is dropped.
    file(GLOB HSBA_DRAW_EXES
        "${CURRENT_PACKAGES_DIR}/bin/DRAWEXE*"
        "${CURRENT_PACKAGES_DIR}/debug/bin/DRAWEXE*")
    if(HSBA_DRAW_EXES)
        file(REMOVE ${HSBA_DRAW_EXES})
    endif()
endif()

vcpkg_install_copyright(
    FILE_LIST
        "${SOURCE_PATH}/LICENSE_LGPL_21.txt"
        "${SOURCE_PATH}/OCCT_LGPL_EXCEPTION.txt"
)
