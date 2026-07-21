# ---------------------------------------------------------------------------
# Sample: LibHsBaSlicer 直接使用示例
# ---------------------------------------------------------------------------
# 演示如何绕过 DllHsBaSlicer C ABI 层，直接调用 LibHsBaSlicer C++ API。
# 提供两种使用方式：传统头文件 和 C++20 模块。
# ---------------------------------------------------------------------------

# --- 传统头文件版 ---
if(ANDROID OR IOS)
    add_library(lib_sample_header STATIC main_header.cpp)
else()
    add_executable(lib_sample_header main_header.cpp)
endif()

target_link_libraries(lib_sample_header PRIVATE
    LibHsBaSlicer
)

target_include_directories(lib_sample_header PRIVATE
    ${CMAKE_SOURCE_DIR}
)

# --- C++20 模块版 ---
if(HSBA_SLICER_MODULE)
    if(ANDROID OR IOS)
        add_library(lib_sample_module STATIC main_module.cpp)
    else()
        add_executable(lib_sample_module main_module.cpp)
    endif()

    target_link_libraries(lib_sample_module PRIVATE
        ModuleHsBaSlicer
    )

    target_include_directories(lib_sample_module PRIVATE
        ${CMAKE_SOURCE_DIR}
    )
endif()

# ---------------------------------------------------------------------------
# 资源文件：复用 FDM 示例的 Stanford Bunny 模型
# ---------------------------------------------------------------------------
set(_model_dir "${CMAKE_SOURCE_DIR}/samples/FDM/models")
set(_bunny_stl "${_model_dir}/stanford_bunny.stl")

if(NOT ANDROID AND NOT IOS)
    add_custom_command(TARGET lib_sample_header POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:lib_sample_header>/models"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_bunny_stl}"
            "$<TARGET_FILE_DIR:lib_sample_header>/models/stanford_bunny.stl"
        COMMENT "Copying model resources for lib_sample_header"
        COMMAND_EXPAND_LISTS
    )

    if(HSBA_SLICER_MODULE)
        add_custom_command(TARGET lib_sample_module POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:lib_sample_module>/models"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_bunny_stl}"
                "$<TARGET_FILE_DIR:lib_sample_module>/models/stanford_bunny.stl"
            COMMENT "Copying model resources for lib_sample_module"
            COMMAND_EXPAND_LISTS
        )
    endif()
endif()
