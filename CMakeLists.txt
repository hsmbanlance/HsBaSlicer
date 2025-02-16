# CMakeList.txt: 顶层 CMake 项目文件，在此处执行全局配置
# 并包含子项目。
#
cmake_minimum_required (VERSION 3.8)

# 如果支持，请为 MSVC 编译器启用热重载。
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

# 6f1ddd6b6878e7e66fcc35c65ba1d8feec2e01f8

#force utf-8
if(MSVC)
  add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
  add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
endif()

if(CMAKE_VERSION VERSION_GREATER 3.12)
  set(CMAKE_CXX_STANDARD 20)
endif()

#add Debug suffix
set(CMAKE_DEBUG_POSTFIX "d")  
set_target_properties(${TARGET_NAME} PROPERTIES DEBUG_POSTFIX "d") 

project ("HsBaSlicer")

include_directories(${CMAKE_SOURCE_DIR})

#Todo:add defined for gamer
if(SWITCH)
  add_compile_definitions(SWITCH)
  add_definitions(GAMER 3)
endif()

find_package(PkgConfig REQUIRED)

#Tripartite library needed

#Boost
ADD_DEFINITIONS(-DBOOST_LOG_DYN_LINK)
find_package(boost REQUIRED COMPONENTS asio log nowide polygon property-tree graph locale test)

#Clipper2
pkg_check_modules(Clipper2 REQUIRED IMPORTED_TARGET Clipper2)

#Compress

#miniz
find_package(miniz CONFIG REQUIRED)

#PNG
find_package(PNG REQUIRED)

#libjpeg-turbo
find_package(libjpeg-turbo CONFIG REQUIRED)

#Protobuf
find_package(protobuf CONFIG REQUIRED)

#RapidJSON
find_package(RapidJSON CONFIG REQUIRED)

#Eigen3
find_package(Eigen3 CONFIG REQUIRED)

#Option for kernel

#Mesh_kernel
find_package(libigl CONFIG REQUIRED)
find_package(CGAL CONFIG REQUIRED)
add_compile_definitions(USE_IGL)
message("Use igl as mesh kernel.")

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

#openvdb for points cloud
find_package(OpenVDB CONFIG REQUIRED)

#protobuf out
option(HSBA_PROTOBUF_OUT "protobuf out source" ON)

#Todo: if eigen3 version higher and has matrix hasher,add define eigen_std_hash

#copy file to exe_dir
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/utils/logcfg.ini DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/HsBaSlicer)

#use bit7z operator zipper
option(HSBA_USE_BIT7Z "use bit7z" ON)

if (HSBA_USE_BIT7Z)
	add_compile_definitions(USE_BIT7Z)
	find_package(unofficial-bit7z CONFIG REQUIRED)
endif()

# subdirectory。
add_subdirectory("third_parts_without_vcpkg")
add_subdirectory("proto")
add_subdirectory("base")
add_subdirectory("utils")
add_subdirectory("2D")
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

if(HSBA_SLICER_USE_TESTS)
	add_subdirectory("tests")
endif()