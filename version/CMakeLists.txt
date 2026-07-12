# 生成version.cpp文件
set(VERSION_CPP_PATH "${CMAKE_CURRENT_BINARY_DIR}/version.cpp")

# 获取构建类型和平台信息
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

if(NOT DEFINED VCPKG_TARGET_TRIPLET)
    set(VCPKG_TARGET_TRIPLET "unknown")
endif()

# 获取配置时间
string(TIMESTAMP CONFIGURE_TIME "%Y-%m-%d %H:%M:%S")

# 在配置时执行PowerShell脚本生成version.cpp
message(STATUS "Generating version.cpp...")
message(STATUS "  PowerShell: ${POWERSHELL_EXECUTABLE}")
message(STATUS "  Output: ${VERSION_CPP_PATH}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Vcpkg Triplet: ${VCPKG_TARGET_TRIPLET}")

find_program(POWERSHELL_EXECUTABLE NAMES pwsh powershell)
if(NOT POWERSHELL_EXECUTABLE)
    message(FATAL_ERROR "PowerShell (pwsh or powershell) not found. Please install PowerShell to build this project.")
endif()

execute_process(
    COMMAND ${POWERSHELL_EXECUTABLE} -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/Generator_Version.ps1"
            -OutputPath "${VERSION_CPP_PATH}"
            -BuildType "${CMAKE_BUILD_TYPE}"
            -Platform "ignored"  # Platform parameter is ignored, determined by triplet
            -VcpkgTriplet "${VCPKG_TARGET_TRIPLET}"
            -VcpkgJsonPath "${CMAKE_SOURCE_DIR}/vcpkg.json"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    RESULT_VARIABLE SCRIPT_RESULT
    OUTPUT_VARIABLE SCRIPT_OUTPUT
    ERROR_VARIABLE SCRIPT_ERROR
)

if(NOT SCRIPT_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to generate version.cpp\nOutput: ${SCRIPT_OUTPUT}\nError: ${SCRIPT_ERROR}")
else()
    message(STATUS "Successfully generated version.cpp at: ${VERSION_CPP_PATH}")
endif()

add_library(HsBaVersion STATIC 
	version.hpp
	${VERSION_CPP_PATH}
)

target_include_directories(HsBaVersion PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(HsBaVersion PUBLIC HsBaSlicerBase)