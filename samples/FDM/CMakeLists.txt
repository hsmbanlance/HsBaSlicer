# ---------------------------------------------------------------------------
# Sample: FDM Pipeline usage demo
# ---------------------------------------------------------------------------

# On mobile platforms (Android / iOS) build a static library so the code is
# still compiled and verified even though there is no console executable.
if(ANDROID OR IOS)
    add_library(fdm_sample STATIC main.cpp)
else()
    add_executable(fdm_sample main.cpp)
endif()

target_link_libraries(fdm_sample PRIVATE DllHsBaSlicer)

# Ensure the sample can find the public headers
target_include_directories(fdm_sample PRIVATE
    ${CMAKE_SOURCE_DIR}/DllHsBaSlicer
    ${CMAKE_SOURCE_DIR}
)

# Link logger on non-game-console platforms
if(NOT HSBA_GAME_CONSOLE)
    target_link_libraries(fdm_sample PRIVATE HsBaSlicerLog)
endif()

# ---------------------------------------------------------------------------
# 资源文件：下载开放模型 + 复制资源到可执行程序所在目录
# ---------------------------------------------------------------------------
# exe 输出到 CMAKE_RUNTIME_OUTPUT_DIRECTORY (bin/<CONFIG>/)
# 资源需复制到同一目录，以便运行时通过相对路径找到
set(_model_dir "${CMAKE_CURRENT_SOURCE_DIR}/models")
set(_bunny_stl "${_model_dir}/stanford_bunny.stl")

# 下载 Stanford Bunny（Wikimedia Commons 开放模型，公共领域）
if(NOT EXISTS "${_bunny_stl}")
    file(MAKE_DIRECTORY "${_model_dir}")
    message(STATUS "Downloading Stanford Bunny STL model...")
    file(DOWNLOAD
        "https://upload.wikimedia.org/wikipedia/commons/4/43/Stanford_Bunny.stl"
        "${_bunny_stl}"
        STATUS _download_status
        SHOW_PROGRESS
    )
    list(GET _download_status 0 _status_code)
    if(NOT _status_code EQUAL 0)
        list(GET _download_status 1 _status_msg)
        message(WARNING "Failed to download Stanford Bunny: ${_status_msg}"
                "\nPlease manually download from:"
                "\n  https://commons.wikimedia.org/wiki/File:Stanford_Bunny.stl"
                "\nand save to: ${_bunny_stl}")
    else()
        message(STATUS "Stanford Bunny STL downloaded successfully.")
    endif()
endif()

# 注意：不使用 file(COPY)，因为 CMAKE_RUNTIME_OUTPUT_DIRECTORY 含 $<CONFIG>
# 生成器表达式，configure 时不展开，会创建错误的字面量目录。
# 改用 add_custom_command 在构建时从源码目录直接复制到 exe 所在目录。
if(NOT ANDROID AND NOT IOS)
    add_custom_command(TARGET fdm_sample POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:fdm_sample>/models"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_bunny_stl}"
            "$<TARGET_FILE_DIR:fdm_sample>/models/stanford_bunny.stl"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts"
            "$<TARGET_FILE_DIR:fdm_sample>/scripts"
        COMMENT "Copying sample resources to output directory"
        COMMAND_EXPAND_LISTS
    )
endif()
