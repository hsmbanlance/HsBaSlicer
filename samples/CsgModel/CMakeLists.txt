# ---------------------------------------------------------------------------
# Sample: 文本 CSG 的 C ABI（不读模型文件，几何完全由脚本描述）
# ---------------------------------------------------------------------------

# 移动端（Android / iOS）编译为静态库，保证代码仍会被编译校验，
# 但不生成控制台可执行文件。这些平台上没有 OCCT，示例在运行前会由
# HsBaTextCsgAvailable() 判定并直接退出，因此源码本身不需要内核宏。
if(ANDROID OR IOS)
    add_library(csg_sample STATIC main.cpp)
else()
    add_executable(csg_sample main.cpp)
endif()

target_link_libraries(csg_sample PRIVATE DllHsBaSlicer)

# 示例只用 model_preprocess.h / fdm_pipeline.h 两个公开 C 头
target_include_directories(csg_sample PRIVATE
    ${CMAKE_SOURCE_DIR}/DllHsBaSlicer
    ${CMAKE_SOURCE_DIR}
)

if(NOT HSBA_GAME_CONSOLE)
    target_link_libraries(csg_sample PRIVATE HsBaSlicerLog)
endif()

# 示例 5 把 G-code 写到可执行文件旁边的 output/
if(NOT ANDROID AND NOT IOS)
    add_custom_command(TARGET csg_sample POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:csg_sample>/output"
        COMMENT "Creating the output directory for the text CSG sample"
    )
endif()

# UTF-8 active-code-page manifest (no-op on non-MSVC / non-Windows).
hsba_apply_utf8_codepage(csg_sample)
