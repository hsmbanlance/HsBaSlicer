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
            "${search_root}/lib/libtcl[0-9]*.so"      # Linux dynamic
            "${search_root}/lib/libtcl[0-9]*.dylib"   # macOS dynamic
            "${search_root}/lib/libtcl[0-9]*.a")      # Linux/macOS static (x64-linux: libtcl9.0.a)
        list(FILTER HSBA_TCL_IMPLIBS EXCLUDE REGEX "tclstub") # tclstub.lib/.a is an extension stub
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

    # Tcl 9 implements its bignum support with libtommath, so the static Tcl archive
    # carries undefined mp_* references (mp_init_u64, mp_sub_d, ...) that the linker can
    # only satisfy from a separate libtommath archive. Feeding Tcl alone therefore breaks
    # every link that pulls TKDraw (the DRAWEXE harness first, the product library later).
    # With dynamic Tcl this is invisible because libtcl.so already DT_NEEDEDs libtommath.so,
    # so the extra archive is appended only for static non-Windows triplets. It goes through
    # CMAKE_CXX_STANDARD_LIBRARIES because that lands the archive at the very end of the link
    # line - after -ltcl - which is the position that resolves a left-to-right GNU ld pass.
    if(NOT VCPKG_TARGET_IS_WINDOWS AND VCPKG_LIBRARY_LINKAGE STREQUAL "static")
        macro(hsba_find_tommath out_var search_root)
            file(GLOB HSBA_TOMMATH_LIBS
                "${search_root}/lib/libtommath*.a"
                "${search_root}/lib/libtommath*.so"
                "${search_root}/lib/libtommath*.dylib")
            list(SORT HSBA_TOMMATH_LIBS ORDER DESCENDING)
            if(HSBA_TOMMATH_LIBS)
                list(GET HSBA_TOMMATH_LIBS 0 ${out_var})
            else()
                set(${out_var} "")
            endif()
        endmacro()

        hsba_find_tommath(HSBA_TOMMATH_RELEASE "${CURRENT_INSTALLED_DIR}")
        hsba_find_tommath(HSBA_TOMMATH_DEBUG "${CURRENT_INSTALLED_DIR}/debug")
        if(HSBA_TOMMATH_RELEASE)
            set(HSBA_TCL_OPTIONS_RELEASE ${HSBA_TCL_OPTIONS_RELEASE}
                "-DCMAKE_CXX_STANDARD_LIBRARIES=${HSBA_TOMMATH_RELEASE}")
        endif()
        if(HSBA_TOMMATH_DEBUG)
            set(HSBA_TCL_OPTIONS_DEBUG ${HSBA_TCL_OPTIONS_DEBUG}
                "-DCMAKE_CXX_STANDARD_LIBRARIES=${HSBA_TOMMATH_DEBUG}")
        endif()
    endif()

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

# The exported interface must also carry Tcl's own static dependency, not just OCCT's build.
# OCCT writes the Tcl link into OpenCASCADE*Targets.cmake as $<LINK_ONLY:libtcl9.0.a> - the
# bare archive name, so a consumer that links TKDraw/TKViewerTest (DllHsBaSlicer, the CAD
# tests) gets "-ltcl9.0" it cannot locate, and libtommath (Tcl's mp_* bignum backend) is
# never exported at all. Rewrite every installed Draw target file to reference the concrete
# Tcl archive by full path and append libtommath immediately after it, so the linker resolves
# both for every downstream consumer. The full paths point into the vcpkg_installed tree that
# is consumed in place; Tcl is referenced in its release form for all configs, mirroring the
# single (release) entry OCCT itself emitted.
if("tcl" IN_LIST FEATURES AND NOT VCPKG_TARGET_IS_WINDOWS AND VCPKG_LIBRARY_LINKAGE STREQUAL "static"
   AND HSBA_TCL_RELEASE_LIB AND HSBA_TOMMATH_RELEASE AND EXISTS "${HSBA_TOMMATH_RELEASE}")
    get_filename_component(HSBA_TCL_RELEASE_NAME "${HSBA_TCL_RELEASE_LIB}" NAME)
    file(GLOB HSBA_OCCT_TARGET_FILES
        "${CURRENT_PACKAGES_DIR}/share/opencascade/OpenCASCADE*Targets.cmake"
        "${CURRENT_PACKAGES_DIR}/debug/share/opencascade/OpenCASCADE*Targets.cmake")
    foreach(hsba_tgt_file IN LISTS HSBA_OCCT_TARGET_FILES)
        file(READ "${hsba_tgt_file}" hsba_tgt_contents)
        string(FIND "${hsba_tgt_contents}" "LINK_ONLY:${HSBA_TCL_RELEASE_NAME}>" hsba_tcl_pos)
        if(NOT hsba_tcl_pos EQUAL -1)
            string(REPLACE
                "LINK_ONLY:${HSBA_TCL_RELEASE_NAME}>"
                "LINK_ONLY:${HSBA_TCL_RELEASE_LIB}>;\$<LINK_ONLY:${HSBA_TOMMATH_RELEASE}>"
                hsba_tgt_contents "${hsba_tgt_contents}")
            file(WRITE "${hsba_tgt_file}" "${hsba_tgt_contents}")
        endif()
    endforeach()
endif()

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
