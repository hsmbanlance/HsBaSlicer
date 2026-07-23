# ---------------------------------------------------------------------------
# Sample: SLS Pipeline usage demo
# ---------------------------------------------------------------------------

# On mobile platforms (Android / iOS) build a static library so the code is
# still compiled and verified even though there is no console executable.
if(ANDROID OR IOS)
    add_library(sls_sample STATIC main.cpp)
else()
    add_executable(sls_sample main.cpp)
endif()

target_link_libraries(sls_sample PRIVATE DllHsBaSlicer)

# Ensure the sample can find the public headers
target_include_directories(sls_sample PRIVATE
    ${CMAKE_SOURCE_DIR}/DllHsBaSlicer
    ${CMAKE_SOURCE_DIR}
)

# Link logger on non-game-console platforms
if(NOT HSBA_GAME_CONSOLE)
    target_link_libraries(sls_sample PRIVATE HsBaSlicerLog)
endif()

# ---------------------------------------------------------------------------
# 资源文件：复用FDM示例的模型 + 复制Lua脚本到可执行程序所在目录
# ---------------------------------------------------------------------------
# 使用与FDM示例相同的模型文件
set(_model_dir "${CMAKE_SOURCE_DIR}/samples/FDM/models")
set(_bunny_stl "${_model_dir}/stanford_bunny.stl")

# 复制资源到exe所在目录
if(NOT ANDROID AND NOT IOS)
    add_custom_command(TARGET sls_sample POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:sls_sample>/models"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:sls_sample>/output"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_bunny_stl}"
            "$<TARGET_FILE_DIR:sls_sample>/models/stanford_bunny.stl"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts"
            "$<TARGET_FILE_DIR:sls_sample>/scripts"
        COMMENT "Copying SLS sample resources to output directory"
        COMMAND_EXPAND_LISTS
    )
endif()
