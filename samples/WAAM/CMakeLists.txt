# ---------------------------------------------------------------------------
# Sample: WAAM (Wire Arc Additive Manufacturing) Pipeline usage demo
# ---------------------------------------------------------------------------

# On mobile platforms (Android / iOS) build a static library so the code is
# still compiled and verified even though there is no console executable.
if(ANDROID OR IOS)
    add_library(waam_sample STATIC main.cpp)
else()
    add_executable(waam_sample main.cpp)
endif()

target_link_libraries(waam_sample PRIVATE DllHsBaSlicer)

# Ensure the sample can find the public headers
target_include_directories(waam_sample PRIVATE
    ${CMAKE_SOURCE_DIR}/DllHsBaSlicer
    ${CMAKE_SOURCE_DIR}
)

# Link logger on non-game-console platforms
if(NOT HSBA_GAME_CONSOLE)
    target_link_libraries(waam_sample PRIVATE HsBaSlicerLog)
endif()

# ---------------------------------------------------------------------------
# 资源文件：复用FDM示例的模型 + 复制可选的机器人路径Lua脚本
# ---------------------------------------------------------------------------
set(_model_dir "${CMAKE_SOURCE_DIR}/samples/FDM/models")
set(_bunny_stl "${_model_dir}/stanford_bunny.stl")

if(NOT ANDROID AND NOT IOS)
    add_custom_command(TARGET waam_sample POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:waam_sample>/models"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:waam_sample>/output"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_bunny_stl}"
            "$<TARGET_FILE_DIR:waam_sample>/models/stanford_bunny.stl"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts"
            "$<TARGET_FILE_DIR:waam_sample>/scripts"
        COMMENT "Copying WAAM sample resources to output directory"
        COMMAND_EXPAND_LISTS
    )
endif()
