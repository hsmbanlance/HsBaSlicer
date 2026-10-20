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

    # Merge the UTF-8 fragment into the executable AFTER it links, using the
    # manifest tool (mt.exe) that CMake already locates via CMAKE_MT. mt.exe
    # reads the default manifest CMake embedded during the link, merges our
    # fragment with it, and rewrites resource #1 (RT_MANIFEST) in place -- this
    # is exactly the step the Visual Studio toolchain performs internally, so it
    # is deterministic and generator-agnostic (Ninja, MSBuild, Makefiles).
    #
    # Do NOT try to inject the fragment at link time instead:
    #   * "/MANIFEST:EMBED /MANIFESTINPUT:..." makes link.exe embed a second
    #     MANIFEST while CMake also links its own manifest.res (default ID 1,
    #     lang 0x409) -> "fatal error CVT1100: duplicate resource: MANIFEST"
    #     -> "fatal error LNK1123" on every executable link.
    #   * "/MANIFESTINPUT:..." on its own fails with "LNK1220: MANIFESTINPUT
    #     requires /MANIFEST:EMBED".
    if(NOT CMAKE_MT)
        message(WARNING "HsBaWindowsUtf8: CMAKE_MT not found; UTF-8 code-page "
                        "manifest not applied to target '${target}'.")
        return()
    endif()

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${CMAKE_MT}"
            -manifest "${_manifest}"
            "-outputresource:$<TARGET_FILE:${target}>;#1"
        COMMENT "Merging UTF-8 active-code-page manifest into ${target}"
        VERBATIM)
endfunction()
