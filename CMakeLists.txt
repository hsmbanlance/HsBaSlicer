# CMakeList.txt: 顶层 CMake 项目文件，在此处执行全局配置
# 并包含子项目。
#
cmake_minimum_required (VERSION 3.21)

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

project ("HsBaSlicer")

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
  message(FATAL_ERROR "Compiler does not support floating-point non-type template parameters (required)")
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

#Todo:add defined for gamer
if(SWITCH)
  add_compile_definitions(SWITCH)
  add_definitions(GAMER 3)
endif()

find_package(PkgConfig REQUIRED)

#Tripartite library needed

#Boost
if(WIN32)
ADD_DEFINITIONS(-DBOOST_LOG_DYN_LINK)
find_package(Boost REQUIRED log)
elseif(ANDROID)
  #no log on android, use __android_log_print
else()
find_package(Boost REQUIRED log_setup log)
endif()

include_directories(${Boost_INCLUDE_DIRS})

#Clipper2
pkg_check_modules(Clipper2 REQUIRED IMPORTED_TARGET Clipper2)

#Compress
#miniz
find_package(miniz CONFIG REQUIRED)
#use bit7z operator zipper
option(HSBA_HSBA_USE_BIT7Z "use bit7z" ON)

if (HSBA_HSBA_USE_BIT7Z AND NOT ANDROID)
	add_compile_definitions(HSBA_USE_BIT7Z)
	find_package(unofficial-bit7z CONFIG REQUIRED)
endif()

#PNG
find_package(PNG REQUIRED)

#libjpeg-turbo
find_package(libjpeg-turbo CONFIG REQUIRED)

#Protobuf
find_package(Protobuf CONFIG REQUIRED)

#RapidJSON
find_package(RapidJSON CONFIG REQUIRED)

#Eigen3
find_package(Eigen3 CONFIG REQUIRED)

#OpenSSL
find_package(OpenSSL REQUIRED)

#OpenCV
find_package(OpenCV REQUIRED)

#Option for kernel

#Mesh_kernel
find_package(libigl CONFIG REQUIRED)
find_package(CGAL CONFIG REQUIRED)
add_compile_definitions(USE_IGL)
message("Use igl as mesh kernel.")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_compile_definitions(DISABLE_BOOLEAN_OPERATIONS)
  message("Disable boolean operations in Debug configuration for CGAL and IGL.")
  message("Boolean operations may be slow and cause high memory usage in Debug mode.")
endif()

#CAD_kernel
#OpenCascade
#disable if andoird and IOS
#Todo: disable if not support OpenCascade, or use other CAD kernel
if(NOT ANDROID AND NOT IOS AND NOT SWITCH)
  find_package(OpenCASCADE CONFIG REQUIRED)
  message("Use occt as cad kernel.")
  add_compile_definitions(USE_OCCT)
  add_compile_definitions(USE_CAD_KERNEL)
endif()

#sqlpp11 with SQLite3 MySQL PostgreSQL
if(NOT ANDROID AND NOT IOS AND NOT SWITCH)
	find_package(Sqlpp11 CONFIG REQUIRED COMPONENTS SQLite3 MySQL PostgreSQL)
	add_compile_definitions(HSBA_USE_MYSQL)
	add_compile_definitions(HSBA_USE_PGSQL)
else()
	find_package(Sqlpp11 CONFIG REQUIRED COMPONENTS SQLite3)
endif()

#Lua
find_package(Lua REQUIRED)

#protobuf out
option(HSBA_PROTOBUF_OUT "protobuf out source" ON)

#Todo: if eigen3 version higher and has matrix hasher,add define eigen_std_hash

#copy file to exe_dir
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/utils/logcfg.ini DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/HsBaSlicer)

# static or dynamic
option(BUILD_SHARED_LIBS "Build using shared libraries" ON)
# set from VCPKG_TARGET_TRIPLE
if(DEFINED VCPKG_TARGET_TRIPLET)
  message("VCPKG_TARGET_TRIPLET is defined: ${VCPKG_TARGET_TRIPLET}")
# for WIN32 default dynamic, Linux and android default static
  if(VCPKG_TARGET_TRIPLET MATCHES "windows" AND NOT VCPKG_TARGET_TRIPLET MATCHES "static")
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
add_subdirectory("logger")
add_subdirectory("2D")
add_subdirectory("paths")
add_subdirectory("fileoperator")
add_subdirectory("meshmodel")
if(NOT ANDROID AND NOT IOS AND NOT SWITCH)
  add_subdirectory("cadmodel")
endif()
add_subdirectory("convert")
add_subdirectory("LibHsBaSlicer")
add_subdirectory("DllHsBaSlicer")
add_subdirectory ("HsBaSlicer")

#test
option(HSBA_SLICER_USE_TESTS "use tests" ON)

if(ANDROID OR IOS)
  set(HSBA_SLICER_USE_TESTS OFF)
  message("Disable tests on Android or IOS.")
endif()

if(HSBA_SLICER_USE_TESTS)
	add_subdirectory("tests")
endif()