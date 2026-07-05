# CMakeList.txt: HsBaSlicer 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#

#force utf-8
if(MSVC)
  add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
  add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
endif()

# 将源代码添加到此项目的可执行文件或共享库（在 Android 下）
if(ANDROID)
  # 在 Android 上，构建为共享库以便打包进 APK
  add_library(HsBaSlicer SHARED "HsBaSlicer.cpp" "HsBaSlicer.h")

  # 将库输出放到 CMAKE_BINARY_DIR/android_libs/<ABI>
  if(NOT ANDROID_ABI)
    set(ANDROID_ABI "arm64-v8a" CACHE STRING "Target Android ABI")
  endif()
  set_target_properties(HsBaSlicer PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/android_libs/${ANDROID_ABI}"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/android_libs/${ANDROID_ABI}"
  )

  target_link_libraries(HsBaSlicer PRIVATE
    HsBaSlicerUtils
    HsBaSlicerLog
    DllHsBaSlicer
  )

  # 确保 DllHsBaSlicer 的 .so 被复制到相同的 output 目录
  file(GLOB_RECURSE DLL_SO_FILES "${CMAKE_BINARY_DIR}/DllHsBaSlicer/*.so")
  if(DLL_SO_FILES)
    add_custom_command(TARGET HsBaSlicer POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/android_libs/${ANDROID_ABI}"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${DLL_SO_FILES} "${CMAKE_BINARY_DIR}/android_libs/${ANDROID_ABI}/"
      COMMENT "Copying DllHsBaSlicer .so to android_libs/${ANDROID_ABI}"
    )
  endif()
else()
  add_executable (HsBaSlicer "HsBaSlicer.cpp" "HsBaSlicer.h")

  target_link_libraries(HsBaSlicer PRIVATE
    HsBaSlicerUtils
    HsBaSlicerLog
    DllHsBaSlicer
  )
endif()

# ---------------------------------------------------------------------------
# Windows: 复制项目 PDB 调试符号到可执行程序目录
# vcpkg 第三方 DLL 由 vcpkg 的 applocal.ps1 自动部署。
# ---------------------------------------------------------------------------
if(WIN32)
    add_custom_command(TARGET HsBaSlicer POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DTARGET_BIN="$<TARGET_FILE:HsBaSlicer>"
            -DPDB_FILES="$<TARGET_PDB_FILE:DllHsBaSlicer>;$<TARGET_PDB_FILE:HsBaSlicerLog>"
            -P "${CMAKE_SOURCE_DIR}/cmake/deploy_dlls.cmake"
        COMMENT "Deploying PDBs for HsBaSlicer"
        COMMAND_EXPAND_LISTS
    )
endif()


# TODO: 如有需要，请添加测试并安装目标。
