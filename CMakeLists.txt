# CMakeList.txt: 顶层 CMake 项目文件，在此处执行全局配置
# 并包含子项目。
#
cmake_minimum_required (VERSION 3.21)

# All static libraries linked into shared libraries (DllHsBaSlicer.so) require -fPIC
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# b1e15efef6758eaa0beb0a8732cfa66f6a68a81d

#force utf-8
if(MSVC)
  add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
  add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
endif()

if(CMAKE_VERSION VERSION_GREATER 3.12)
  set(CMAKE_CXX_STANDARD 20)
  # Prefer C11 for C sources so features like _Generic are available when supported
  # if able use C23
  if(CMAKE_VERSION VERSION_GREATER 3.23)
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU" 
      AND CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 13)
        set(CMAKE_C_STANDARD 23)
    elseif(CMAKE_C_COMPILER_ID MATCHES "Clang" 
       AND CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 16)
      set(CMAKE_C_STANDARD 23)
    elseif(CMAKE_C_COMPILER_ID STREQUAL "MSVC"
       AND CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 19.36)
      set(CMAKE_C_STANDARD 23)
    else()
      set(CMAKE_C_STANDARD 11)
    endif()
  else()
    set(CMAKE_C_STANDARD 11)
  endif()
  set(CMAKE_C_STANDARD_REQUIRED ON)
endif()

#add Debug suffix
set(CMAKE_DEBUG_POSTFIX "d")  
set_target_properties(${TARGET_NAME} PROPERTIES DEBUG_POSTFIX "d") 

project("HsBaSlicer" VERSION 1.0.0)

include_directories(${CMAKE_SOURCE_DIR})

# Include static feature checks and run them early so results can influence configuration
include(${CMAKE_SOURCE_DIR}/static_check/feature_check.cmake)

# Run required feature checks (fatal if missing)
check_cxx_feature_concepts(HAS_CXX_CONCEPTS)
check_cxx_feature_ranges(HAS_CXX_RANGES)
check_cxx_feature_source_location(HAS_CXX_SOURCE_LOCATION)
check_cxx_feature_nttp_class(HAS_CXX_NTTP_CLASS)
check_cxx_feature_nttp_fp(HAS_CXX_NTTP_FP)
check_cxx_feature_template_template_matching(HAS_CXX_TT_MATCH)
check_c_feature_GENERIC(HAS_C_GENERIC_GENERIC)

if(NOT HAS_CXX_CONCEPTS)
  message(FATAL_ERROR "Compiler does not support C++ concepts (required)")
endif()
if(NOT HAS_CXX_RANGES)
  message(FATAL_ERROR "Compiler does not support C++ ranges (required)")
endif()
if(NOT HAS_CXX_SOURCE_LOCATION)
  message(FATAL_ERROR "Compiler does not support std::source_location (required)")
endif()
if(NOT HAS_CXX_NTTP_CLASS)
  message(FATAL_ERROR "Compiler does not support class-type non-type template parameters (required)")
endif()
if(NOT HAS_CXX_NTTP_FP)
  message(WARNING "Compiler does not support floating-point non-type template parameters; falling back where possible.")
  add_compile_definitions(HSBA_NO_NTTP_FP)
else()
  add_compile_definitions(HSBA_HAS_NTTP_FP)
endif()
if(NOT HAS_CXX_TT_MATCH)
  message(FATAL_ERROR "Compiler does not support template-template parameter matching required by P0522R0")
endif()
if(NOT HAS_C_GENERIC_GENERIC)
  message(WARNING "C compiler does not support _Generic. Some C code may rely on C11 _Generic; falling back where possible.")
  add_compile_definitions(HSBA_NO_C_GENERIC)
else()
  add_compile_definitions(HSBA_HAS_C_GENERIC)
endif()

# Optional checks: coroutines, explicit this, C23 constexpr
check_cxx_feature_coroutines(HAS_CXX_COROUTINES)
check_cxx_feature_explicit_this(HAS_CXX_EXPLICIT_THIS)
check_c_feature_c23_constexpr(HAS_C_C23_CONSTEXPR)

if(HAS_CXX_COROUTINES)
  message(STATUS "Compiler supports C++ coroutines")
  add_compile_definitions(HSBA_HAS_CXX_COROUTINES)
endif()
if(HAS_CXX_EXPLICIT_THIS)
  message(STATUS "Compiler supports explicit object parameter (explicit this)")
  add_compile_definitions(HSBA_HAS_CXX_EXPLICIT_THIS)
endif()
if(HAS_C_C23_CONSTEXPR)
  message(STATUS "C compiler appears to support C23 constexpr-style features")
  add_compile_definitions(HSBA_HAS_C23_CONSTEXPR)
endif()

#Todo:add defined for gamer console

message(STATUS "Configuring for platform: ${CMAKE_SYSTEM_NAME}")

#VCPKG triplet
if(DEFINED VCPKG_TARGET_TRIPLET)
  message(STATUS "VCPKG_TARGET_TRIPLET is defined: ${VCPKG_TARGET_TRIPLET}")
  # Xbox or other game console
  if(VCPKG_TARGET_TRIPLET MATCHES "xbox" OR VCPKG_TARGET_TRIPLET MATCHES "switch"
     OR VCPKG_TARGET_TRIPLET MATCHES "playstation" OR VCPKG_TARGET_TRIPLET MATCHES "stadia")
    add_compile_definitions(HSBA_IS_GAME_CONSOLE)
    add_compile_definitions(HSBA_GAME_CONSOLE_${VCPKG_TARGET_TRIPLET})
    #add_options for disable features on game console, such as logger, or use other logger for game console
    set(HSBA_GAME_CONSOLE ON CACHE BOOL "Building for game console" FORCE)
    message(STATUS "Building for game console: ${VCPKG_TARGET_TRIPLET}")
  else()
    set(HSBA_GAME_CONSOLE OFF CACHE BOOL "Not building for game console" FORCE)
  endif()
else()
  message(STATUS "VCPKG_TARGET_TRIPLET is not defined.")
endif()

if(WIN32 OR LINUX OR (APPLE AND NOT IOS))
  message(STATUS "Building for desktop platform: ${CMAKE_SYSTEM_NAME}")
  set(HSBA_DESKTOP ON CACHE BOOL "Building for desktop platform" FORCE)
elseif(ANDROID OR IOS)
  message(STATUS "Building for mobile platform: ${CMAKE_SYSTEM_NAME}")
  set(HSBA_MOBILE ON CACHE BOOL "Building for mobile platform" FORCE)
else()
  message(STATUS "Building for unknown platform: ${CMAKE_SYSTEM_NAME}")
endif()

find_package(PkgConfig REQUIRED)

#Tripartite library needed

#Boost
if(WIN32)
ADD_DEFINITIONS(-DBOOST_LOG_DYN_LINK)
find_package(Boost REQUIRED log)
elseif(ANDROID OR IOS OR HSBA_GAME_CONSOLE)
  #no log on android, use __android_log_print
  #no log on ios, use os_log
  #no log on game console, use other log or no log
  find_package(Boost REQUIRED)
else()
find_package(Boost REQUIRED log_setup log)
endif()

include_directories(${Boost_INCLUDE_DIRS})

#Clipper2
pkg_check_modules(Clipper2 REQUIRED IMPORTED_TARGET Clipper2)

#runtime dll/so/dylib loader
option(HSBA_USE_DLL_LOADER "Use dynamic library loader" ON)
if(HSBA_DESKTOP)
  message(STATUS "Dynamic library loader enabled for desktop platform.")
elseif(ANDROID)
  set(HSBA_USE_DLL_LOADER OFF CACHE BOOL "Disable dynamic library loader on non-desktop platforms" FORCE)
  add_compile_definitions(HSBA_NO_DLL_LOADER)
  message(STATUS "Dynamic library loader may have limited support on Android Native Development Kit (NDK) due to platform restrictions.")
else()
  set(HSBA_USE_DLL_LOADER OFF CACHE BOOL "Disable dynamic library loader on non-desktop platforms" FORCE)
  add_compile_definitions(HSBA_NO_DLL_LOADER)
  message(STATUS "Dynamic library loader disabled on non-desktop platform.")
endif()

#Compress
#miniz
find_package(miniz CONFIG REQUIRED)
#use bit7z operator zipper
option(HSBA_HSBA_USE_BIT7Z "use bit7z" ON)

if(NOT HSBA_DESKTOP)
  set(HSBA_HSBA_USE_BIT7Z OFF CACHE BOOL "Disable bit7z on non-desktop platforms" FORCE)
  message(STATUS "Bit7z disabled on non-desktop platform.")
endif()

if(HSBA_HSBA_USE_BIT7Z)
	add_compile_definitions(HSBA_USE_BIT7Z)
	find_package(unofficial-bit7z CONFIG REQUIRED)
endif()

option(HSBA_ENABLE_THREAD_POOL_COROUTINE "Enable coroutine support in thread pool" ON)

if(HSBA_ENABLE_THREAD_POOL_COROUTINE)
  add_compile_definitions(HSBA_ENABLE_THREAD_POOL_COROUTINE)
endif()

#Protobuf
find_package(Protobuf CONFIG REQUIRED)

#RapidJSON
find_package(RapidJSON CONFIG REQUIRED)

#Eigen3
find_package(Eigen3 CONFIG REQUIRED)

#OpenSSL
find_package(OpenSSL REQUIRED)

#OpenCV
if(NOT IOS AND NOT HSBA_GAME_CONSOLE)
  find_package(OpenCV REQUIRED)
  add_compile_definitions(HAS_OPENCV)
endif()

#Option for kernel

#Mesh_kernel
find_package(libigl CONFIG REQUIRED)
if(NOT IOS AND NOT HSBA_GAME_CONSOLE)
  find_package(CGAL CONFIG REQUIRED)
  add_compile_definitions(USE_CGAL)
  message("Use CGAL and igl enable boolean operations or other features.")
endif()
add_compile_definitions(USE_IGL)
message("Use igl as mesh kernel.")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_compile_definitions(DISABLE_BOOLEAN_OPERATIONS)
  message("Disable boolean operations in Debug configuration for CGAL and IGL.")
  message("Boolean operations may be slow and cause high memory usage in Debug mode.")
endif()

# Model object pool size: 10 when CGAL is available, 1 otherwise
if(NOT IOS AND NOT HSBA_GAME_CONSOLE)
  add_compile_definitions(HSBA_MODEL_POOL_SIZE=10)
else()
  add_compile_definitions(HSBA_MODEL_POOL_SIZE=1)
endif()

#CAD_kernel
#OpenCascade
#disable if andoird and IOS
#Todo: disable if not support OpenCascade, or use other CAD kernel
if(NOT ANDROID AND NOT IOS AND NOT HSBA_GAME_CONSOLE)
  find_package(OpenCASCADE CONFIG REQUIRED)
  message("Use occt as cad kernel.")
  add_compile_definitions(USE_OCCT)
  add_compile_definitions(USE_CAD_KERNEL)
endif()

#sqlpp11 with SQLite3 MySQL PostgreSQL
if(NOT ANDROID AND NOT IOS AND NOT HSBA_GAME_CONSOLE)
	find_package(Sqlpp11 CONFIG REQUIRED COMPONENTS SQLite3 MySQL PostgreSQL)
	add_compile_definitions(HSBA_USE_MYSQL)
	add_compile_definitions(HSBA_USE_PGSQL)
else()
	find_package(Sqlpp11 CONFIG REQUIRED COMPONENTS SQLite3)
endif()

#Lua
find_package(Lua REQUIRED)

#tinyxml2
find_package(tinyxml2 CONFIG REQUIRED)

#yaml-cpp
find_package(yaml-cpp CONFIG REQUIRED)

#magic_enum
find_package(magic_enum CONFIG REQUIRED)

#protobuf out
option(HSBA_PROTOBUF_OUT "protobuf out source" ON)

#Todo: if eigen3 version higher and has matrix hasher,add define eigen_std_hash

#copy logcfg.ini to exe output directory
# Unify output directories on all platforms so that executables, shared libraries,
# and resources end up in the same directory.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")

# file(COPY) runs at configure time, so use CMAKE_BUILD_TYPE (not $<CONFIG>)
if(CMAKE_BUILD_TYPE)
  file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/utils/logcfg.ini
       DESTINATION "${CMAKE_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE}")
endif()

# static or dynamic
option(BUILD_SHARED_LIBS "Build using shared libraries" ON)
# set from VCPKG_TARGET_TRIPLE
if(DEFINED VCPKG_TARGET_TRIPLET)
  message("VCPKG_TARGET_TRIPLET is defined: ${VCPKG_TARGET_TRIPLET}")
# for Windows and Xbox default dynamic, Linux and android default static
  if((VCPKG_TARGET_TRIPLET MATCHES "windows" OR VCPKG_TARGET_TRIPLET MATCHES "xbox") AND NOT VCPKG_TARGET_TRIPLET MATCHES "static")
	set(BUILD_SHARED_LIBS ON CACHE BOOL "Build using shared libraries" FORCE)
	message("Set BUILD_SHARED_LIBS to ON for Windows dynamic.")
# if match dynamic use dynamic
  elseif(VCPKG_TARGET_TRIPLET MATCHES "dynamic")
	set(BUILD_SHARED_LIBS ON CACHE BOOL "Build using shared libraries" FORCE)
	message("Set BUILD_SHARED_LIBS to ON for dynamic.")
  elseif(VCPKG_TARGET_TRIPLET MATCHES "linux" OR VCPKG_TARGET_TRIPLET MATCHES "android" OR VCPKG_TARGET_TRIPLET MATCHES "static")
	set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build using shared libraries" FORCE)
	message("Set BUILD_SHARED_LIBS to OFF for Linux or Android static.")
#other use static
  else()
	set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build using shared libraries" FORCE)
	message("Set BUILD_SHARED_LIBS to OFF for other static.")
  endif()
else()
  message("VCPKG_TARGET_TRIPLET is not defined.")
endif()

# subdirectory。
add_subdirectory("third_parts_without_vcpkg")
add_subdirectory("proto")
add_subdirectory("base")
add_subdirectory("cipher")
add_subdirectory("utils")
if(HSBA_GAME_CONSOLE)
  LOG(WARNING "Logger is not supported on Game Consoles")
else()
  add_subdirectory("logger")
endif()
add_subdirectory("2D")
add_subdirectory("paths")
add_subdirectory("fileoperator")
add_subdirectory("meshmodel")
if(NOT ANDROID AND NOT IOS AND NOT HSBA_GAME_CONSOLE)
  add_subdirectory("cadmodel")
endif()
add_subdirectory("preprocess")
add_subdirectory("support")
add_subdirectory("convert")
add_subdirectory("LibHsBaSlicer")
add_subdirectory("DllHsBaSlicer")
add_subdirectory ("HsBaSlicer")
add_subdirectory("version")

#samples
option(HSBA_SLICER_BUILD_SAMPLES "Build samples" ON)
if(HSBA_SLICER_BUILD_SAMPLES)
  add_subdirectory("samples/FDM")
  add_subdirectory("samples/SLA")
endif()

# ---------------------------------------------------------------------------
# Install & Export
# ---------------------------------------------------------------------------
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# 安装库目标（RUNTIME → bin, LIBRARY → lib, ARCHIVE → lib）
# 所有内部静态库必须加入导出集，否则 CMake 会报错
set(_HSBA_INSTALL_TARGETS
    DllHsBaSlicer LibHsBaSlicer HsBaSlicerLog
    HsBaSlicerBase HsBaSlicerUtils HsBaSlicerMesh
    HsBaSlicer2D HsBaPreprocess HsBaSupport HsBaPaths
    HsBaSlicerFileOperator
    HsBaSlicerProto HsBaCipher
    HsBaVersion
    HsBaSlicerConverter
)
if(NOT ANDROID AND NOT IOS AND NOT HSBA_GAME_CONSOLE)
  list(APPEND _HSBA_INSTALL_TARGETS HsBaSlicerCADModel)
endif()
install(TARGETS ${_HSBA_INSTALL_TARGETS}
    EXPORT HsBaSlicerTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}      # DLLs (Windows) / exe
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}      # .so (Linux/macOS)
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}      # .lib/.a (import lib)
)

# 安装公共头文件 —— DllHsBaSlicer C API
install(FILES
    DllHsBaSlicer/dllexport.h
    DllHsBaSlicer/fdm_pipeline.h
    DllHsBaSlicer/sla_pipeline.h
    DllHsBaSlicer/pipeline_convert.h
    DllHsBaSlicer/initialize.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/HsBaSlicer
)

# base
install(FILES
    base/IModel.hpp
    base/singleton.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/HsBaSlicer/base
)

# version
install(FILES
    version/version.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/HsBaSlicer
)

# logger
install(FILES
    logger/logger.hpp
    logger/export.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/HsBaSlicer/logger
)

# LibHsBaSlicer 公开 API
install(FILES
    LibHsBaSlicer/export.h
    LibHsBaSlicer/version_info.hpp
    LibHsBaSlicer/Slice/mesh_slice.hpp
    LibHsBaSlicer/Preprocess/model_preprocess.hpp
    LibHsBaSlicer/Support/fdm_support.hpp
    LibHsBaSlicer/Fill/polygon_fill.hpp
    LibHsBaSlicer/Path/path_generator.hpp
    LibHsBaSlicer/Floor/sla_floor.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/HsBaSlicer/LibHsBaSlicer
)

# LibHsBaSlicer 公开 API 的传递依赖头文件
install(FILES
    2D/FloatPolygons.hpp
    2D/IntPolygon.hpp
    2D/PolygonFill.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/HsBaSlicer/2D
)

install(FILES
    meshmodel/FullTopoModel.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/HsBaSlicer/meshmodel
)

install(FILES
    support/SupportConfig.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/HsBaSlicer/support
)

install(FILES
    paths/IPath.hpp
    paths/pointspath.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/HsBaSlicer/paths
)

# convert pipeline conversion headers
install(FILES
    convert/PipelineConfig2Msg.hpp
    convert/Msg2PipelineConfig.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/HsBaSlicer/convert
)

# proto 生成的头文件
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/proto/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/HsBaSlicer/proto
    FILES_MATCHING PATTERN "*.pb.h"
)

# proto 多语言导出文件
if(HSBA_PROTOBUF_OUT)
    install(DIRECTORY ${HSBA_PROTO_OUT_DIRECTORY}/protobuf/
        DESTINATION ${CMAKE_INSTALL_DATADIR}/HsBaSlicer/proto
    )
    if(HSBA_PROTO_OUT_CPP)
        install(DIRECTORY ${HSBA_PROTO_OUT_DIRECTORY}/cpp/
            DESTINATION ${CMAKE_INSTALL_DATADIR}/HsBaSlicer/proto/cpp
        )
    endif()
    if(HSBA_PROTO_OUT_CSHARP)
        install(DIRECTORY ${HSBA_PROTO_OUT_DIRECTORY}/cs/
            DESTINATION ${CMAKE_INSTALL_DATADIR}/HsBaSlicer/proto/cs
        )
    endif()
    if(HSBA_PROTO_OUT_JAVA)
        install(DIRECTORY ${HSBA_PROTO_OUT_DIRECTORY}/java/
            DESTINATION ${CMAKE_INSTALL_DATADIR}/HsBaSlicer/proto/java
        )
    endif()
    if(HSBA_PROTO_OUT_PYTHON)
        install(DIRECTORY ${HSBA_PROTO_OUT_DIRECTORY}/python/
            DESTINATION ${CMAKE_INSTALL_DATADIR}/HsBaSlicer/proto/python
        )
    endif()
    if(HSBA_PROTO_OUT_PHP)
        install(DIRECTORY ${HSBA_PROTO_OUT_DIRECTORY}/php/
            DESTINATION ${CMAKE_INSTALL_DATADIR}/HsBaSlicer/proto/php
        )
    endif()
endif()

# 导出 CMake Targets 文件
install(EXPORT HsBaSlicerTargets
    FILE HsBaSlicerTargets.cmake
    NAMESPACE HsBaSlicer::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/HsBaSlicer
)

# 生成并安装 Config / Version 文件
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/HsBaSlicerConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/HsBaSlicerConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/HsBaSlicer
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/HsBaSlicerConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/HsBaSlicerConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/HsBaSlicerConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/HsBaSlicer
)

#test
option(HSBA_SLICER_USE_TESTS "use tests" ON)
if(ANDROID OR IOS OR HSBA_GAME_CONSOLE)
  set(HSBA_SLICER_USE_TESTS OFF CACHE BOOL "use tests" FORCE)
  message(STATUS "Tests disabled on Android, iOS, or game consoles due to output limitations.")
endif()
if(NOT DEFINED HSBA_SLICER_RUN_TESTS_AFTER_BUILD)
  set(HSBA_SLICER_RUN_TESTS_AFTER_BUILD ON CACHE BOOL "Run tests after build" ON)
endif()
if(CMAKE_CROSSCOMPILING)
  set(HSBA_SLICER_RUN_TESTS_AFTER_BUILD OFF CACHE BOOL "Run tests after build" FORCE)
  message(STATUS "Cross-compiling: test executables will be built but not executed.")
endif()

if(HSBA_SLICER_USE_TESTS)
	add_subdirectory("tests")
endif()

#static_tests
option(HSBA_SLICER_USE_STATIC_TESTS "use static tests" ON)
if(HSBA_SLICER_USE_STATIC_TESTS)
    add_subdirectory("static_tests")
endif()

#docs
option(HSBA_SLICER_BUILD_DOCS "Build documentation" ON)

#if has not doxygen, disable docs
find_package(Doxygen)
if(NOT DOXYGEN_FOUND)
  set(HSBA_SLICER_BUILD_DOCS OFF CACHE BOOL "Doxygen not found, cannot build documentation" FORCE)
  message(STATUS "Doxygen not found: documentation will not be built.")
endif()

if(HSBA_SLICER_BUILD_DOCS)
  add_subdirectory("docs")
endif()
