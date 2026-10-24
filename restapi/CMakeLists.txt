# ---------------------------------------------------------------------------
# Optional RESTful API server (HsBaSlicerRestServer)
#
# Gated by the top-level HSBA_BUILD_REST_API option (desktop-only, OFF by
# default). The cpp-httplib dependency is provided by the vcpkg "rest-api"
# manifest feature; the top-level CMakeLists already validates the feature is
# active before this directory is added.
# ---------------------------------------------------------------------------

# The vcpkg port is named "cpp-httplib" but its CMake package is "httplib"
# (vcpkg_cmake_config_fixup(PACKAGE_NAME httplib)), exposing httplib::httplib.
find_package(httplib CONFIG REQUIRED)
find_package(Threads REQUIRED)

add_executable(HsBaSlicerRestServer
    main.cpp
    server_config.hpp
    server_config.cpp
    content_negotiate.hpp
    content_negotiate.cpp
    serialize_value.hpp
    config_bridge.hpp
    config_bridge.cpp
    pipeline_executor.hpp
    pipeline_executor.cpp
    job_manager.hpp
    job_manager.cpp
    restapi.hpp
    routes.cpp)

target_include_directories(HsBaSlicerRestServer PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/DllHsBaSlicer)

# The C ABI (run/save/load/version) comes from DllHsBaSlicer; the reflection
# bridge (FieldToAny/AnyToField), the param_reflect TypeInfo table, rw_ptree INI
# loading and the struct_json/struct_xml serializers come from the static libs.
# Reflecting a config in this process uses this binary's own inline TypeInfo
# copies and only ever passes completed plain-C structs across the DLL boundary,
# so there is no cross-module sharing of a single reflected object.
target_link_libraries(HsBaSlicerRestServer PRIVATE
    httplib::httplib
    Threads::Threads
    DllHsBaSlicer
    HsBaSlicerFileOperator
    HsBaSlicerUtils
    HsBaSlicerBase
    HsBaPipelineTypes)

if(NOT HSBA_GAME_CONSOLE)
    target_link_libraries(HsBaSlicerRestServer PRIVATE HsBaSlicerLog)
endif()

# UTF-8 active-code-page manifest (no-op on non-MSVC / non-Windows).
hsba_apply_utf8_codepage(HsBaSlicerRestServer)

# Copy restapi.ini next to the executable at build time so a fresh checkout can
# run the server without an explicit --config. CMAKE_RUNTIME_OUTPUT_DIRECTORY
# contains a $<CONFIG> generator expression, so use add_custom_command (not
# file(COPY)) which expands it correctly.
add_custom_command(TARGET HsBaSlicerRestServer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/restapi.ini"
        "$<TARGET_FILE_DIR:HsBaSlicerRestServer>/restapi.ini"
    COMMENT "Copying restapi.ini to the server output directory")
