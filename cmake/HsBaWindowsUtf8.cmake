# ---------------------------------------------------------------------------
# HsBaWindowsUtf8.cmake
#
# Enables full non-ASCII (CJK / accented / emoji) model-file and result-file
# paths on Windows by embedding a UTF-8 "active code page" application manifest
# into the process-owned executables.
#
# Background
# ----------
# Several model loaders and package writers hand a *narrow* char* path to
# third-party libraries that internally call fopen / CreateFileA (libigl, CGAL,
# OpenCASCADE, OpenVDB io::File, miniz/bit7z). On Windows a narrow path is
# interpreted using the process ANSI code page (GetACP()), so with a legacy
# code page (e.g. 936/GBK, 1252) Unicode file names cannot round-trip.
#
# Declaring <activeCodePage>UTF-8</activeCodePage> in the executable manifest
# makes GetACP() return 65001 for the whole process. HsBaSlicer's
# utf8_to_local() then becomes a pass-through (windows_chcp_utf8() is true) and
# the CRT converts the UTF-8 narrow path to UTF-16 correctly, unlocking full
# Unicode support for every format at once -- no per-library changes required.
#
# Note the manifest applies to the *executable that owns the process*. When the
# library is consumed as DllHsBaSlicer from an external C#/Java/Python host, the
# host executable's manifest governs the process, which is outside our control.
# This function therefore targets the desktop executables we build ourselves.
# ---------------------------------------------------------------------------

include_guard(GLOBAL)

# hsba_apply_utf8_codepage(<target>)
#
# Embed the UTF-8 active-code-page manifest into an executable target. No-op on
# every platform/toolchain other than MSVC targeting Windows.
function(hsba_apply_utf8_codepage target)
    if(NOT MSVC OR NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
        return()
    endif()

    if(NOT TARGET ${target})
        return()
    endif()

    set(_manifest "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/hsba_utf8.manifest")
    if(NOT EXISTS "${_manifest}")
        message(WARNING "HsBaWindowsUtf8: manifest not found: ${_manifest}")
        return()
    endif()

    # Feed the fragment to the MSVC linker as an extra manifest input and force
    # an embedded, merged manifest. link.exe combines /MANIFESTINPUT with its
    # auto-generated default manifest (trust info + VC-runtime dependency), so
    # this works deterministically for both the Visual Studio and Ninja
    # generators (no reliance on .manifest source-file auto-detection).
    target_link_options(${target} PRIVATE
        "/MANIFEST:EMBED"
        "/MANIFESTINPUT:${_manifest}")
endfunction()
