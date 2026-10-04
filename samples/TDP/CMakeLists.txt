# ---------------------------------------------------------------------------
# Sample: 3DP (TDP) Pipeline usage demo
# ---------------------------------------------------------------------------

if(ANDROID OR IOS)
    add_library(tdp_sample STATIC main.cpp)
else()
    add_executable(tdp_sample main.cpp)
endif()

target_link_libraries(tdp_sample PRIVATE DllHsBaSlicer)

target_include_directories(tdp_sample PRIVATE
    ${CMAKE_SOURCE_DIR}/DllHsBaSlicer
    ${CMAKE_SOURCE_DIR}
)

if(NOT HSBA_GAME_CONSOLE)
    target_link_libraries(tdp_sample PRIVATE HsBaSlicerLog)
endif()

# ---------------------------------------------------------------------------
# 资源文件：复用FDM示例的模型 + 复制Lua脚本到可执行程序所在目录
# ---------------------------------------------------------------------------
set(_model_dir "${CMAKE_SOURCE_DIR}/samples/FDM/models")
set(_bunny_stl "${_model_dir}/stanford_bunny.stl")

if(NOT ANDROID AND NOT IOS)
    add_custom_command(TARGET tdp_sample POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:tdp_sample>/models"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:tdp_sample>/output"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_bunny_stl}"
            "$<TARGET_FILE_DIR:tdp_sample>/models/stanford_bunny.stl"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts"
            "$<TARGET_FILE_DIR:tdp_sample>/scripts"
        COMMENT "Copying 3DP sample resources to output directory"
        COMMAND_EXPAND_LISTS
    )
endif()
