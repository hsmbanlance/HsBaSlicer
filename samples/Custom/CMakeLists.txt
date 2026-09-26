# ---------------------------------------------------------------------------
# Sample: 完全由自定义 Lua 脚本驱动的流水线（Custom Lua Pipeline）
# ---------------------------------------------------------------------------

# 移动端（Android / iOS）编译为静态库，保证代码仍会被编译校验，
# 但不生成控制台可执行文件。
if(ANDROID OR IOS)
    add_library(custom_sample STATIC main.cpp)
else()
    add_executable(custom_sample main.cpp)
endif()

target_link_libraries(custom_sample PRIVATE DllHsBaSlicer)

# 示例需要访问公开头文件
target_include_directories(custom_sample PRIVATE
    ${CMAKE_SOURCE_DIR}/DllHsBaSlicer
    ${CMAKE_SOURCE_DIR}
)

# 非游戏主机平台链接日志库
if(NOT HSBA_GAME_CONSOLE)
    target_link_libraries(custom_sample PRIVATE HsBaSlicerLog)
endif()

# ---------------------------------------------------------------------------
# 资源文件：复用FDM示例的模型 + 复制Lua流水线脚本到可执行程序所在目录
# ---------------------------------------------------------------------------
set(_bunny_stl "${CMAKE_SOURCE_DIR}/samples/FDM/models/stanford_bunny.stl")

if(NOT ANDROID AND NOT IOS)
    add_custom_command(TARGET custom_sample POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:custom_sample>/models"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:custom_sample>/output"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_bunny_stl}"
            "$<TARGET_FILE_DIR:custom_sample>/models/stanford_bunny.stl"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts"
            "$<TARGET_FILE_DIR:custom_sample>/scripts"
        COMMENT "Copying Custom Lua pipeline sample resources to output directory"
        COMMAND_EXPAND_LISTS
    )
endif()
