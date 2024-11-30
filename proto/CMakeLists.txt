#output protobuf sources

find_package(Protobuf REQUIRED)

#proto_files
file(GLOB proto_files "${CMAKE_CURRENT_SOURCE_DIR}/*.proto")

if(HSBA_PROTOBUF_OUT)

set(HSBA_PROTO_OUT_DIRECTORY ${CMAKE_BINARY_DIR}/proto_out CACHE STRING "Protobuf out directory, don't use in this project.")

if(NOT EXISTS HSBA_PROTO_OUT_DIRECTORY)
  file(MAKE_DIRECTORY ${HSBA_PROTO_OUT_DIRECTORY})
endif()

#copy protobuf sources to output directory
file(COPY ${proto_files} DESTINATION ${HSBA_PROTO_OUT_DIRECTORY}/protobuf)

#for each languages output sources

option(HSBA_PROTO_OUT_CPP "protobuf out c++ source" ON)

if(HSBA_PROTO_OUT_CPP)
  if(NOT EXISTS ${HSBA_PROTO_OUT_DIRECTORY}/cpp)
    file(MAKE_DIRECTORY ${HSBA_PROTO_OUT_DIRECTORY}/cpp)
  endif()
  FOREACH(proto_file ${proto_files})
    STRING(REGEX REPLACE "[^/]proto" "" proto_file_name ${proto_file})
    get_filename_component(proto_file_name ${proto_file_name} NAME)
    LIST(APPEND PROTO_SRCS "${proto_file_name}.pb.cc" "${proto_file_name}.pb.h")
	ADD_CUSTOM_COMMAND(
		OUTPUT "${HSBA_PROTO_OUT_DIRECTORY}/cpp/${proto_file_name}.pb.cc" "${HSBA_PROTO_OUT_DIRECTORY}/cpp/${proto_file_name}.pb.h"
		COMMAND ${PROTOBUF_PROTOC_EXECUTABLE} --proto_path=${CMAKE_CURRENT_SOURCE_DIR}
                       --cpp_out=${HSBA_PROTO_OUT_DIRECTORY}/cpp
                       ${proto_file} 
		DEPENDS ${proto_file}
	)
ENDFOREACH(proto_file)
endif()
    

option(HSBA_PROTO_OUT_CSHARP "protobuf out c# source" ON)

if(HSBA_PROTO_OUT_CSHARP)
  if(NOT EXISTS ${HSBA_PROTO_OUT_DIRECTORY}/cs)
    file(MAKE_DIRECTORY ${HSBA_PROTO_OUT_DIRECTORY}/cs)
  endif()
  FOREACH(proto_file ${proto_files})
    STRING(REGEX REPLACE "[^/]proto" "" proto_file_name ${proto_file})
    get_filename_component(proto_file_name ${proto_file_name} NAME)
    LIST(APPEND PROTO_SRCS "${HSBA_PROTO_OUT_DIRECTORY}/cs/${proto_file_name}.cs")
	ADD_CUSTOM_COMMAND(
		OUTPUT "${HSBA_PROTO_OUT_DIRECTORY}/cs/${proto_file_name}.cs"
		COMMAND ${PROTOBUF_PROTOC_EXECUTABLE} --proto_path=${CMAKE_CURRENT_SOURCE_DIR}
                       --csharp_out=${HSBA_PROTO_OUT_DIRECTORY}/cs
                       ${proto_file} 
		DEPENDS ${proto_file}
	)
ENDFOREACH(proto_file)
endif()

option(HSBA_PROTO_OUT_JAVA "protobuf out java source" ON)

if(HSBA_PROTO_OUT_JAVA)
  if(NOT EXISTS ${HSBA_PROTO_OUT_DIRECTORY}/java)
    file(MAKE_DIRECTORY ${HSBA_PROTO_OUT_DIRECTORY}/java)
  endif()
  FOREACH(proto_file ${proto_files})
    STRING(REGEX REPLACE "[^/]proto" "" proto_file_name ${proto_file})
    get_filename_component(proto_file_name ${proto_file_name} NAME)
    LIST(APPEND PROTO_SRCS "${HSBA_PROTO_OUT_DIRECTORY}/java/HsbaProto/${proto_file_name}.java")
	ADD_CUSTOM_COMMAND(
		OUTPUT "${HSBA_PROTO_OUT_DIRECTORY}/java/HsbaProto/${proto_file_name}.java"
		COMMAND ${PROTOBUF_PROTOC_EXECUTABLE} --proto_path=${CMAKE_CURRENT_SOURCE_DIR}
                       --java_out=${HSBA_PROTO_OUT_DIRECTORY}/java
                       ${proto_file} 
		DEPENDS ${proto_file}
	)
ENDFOREACH(proto_file)
endif()

option(HSBA_PROTO_OUT_PYTHON "protobuf out python source" ON)

if(HSBA_PROTO_OUT_PYTHON)
  if(NOT EXISTS ${HSBA_PROTO_OUT_DIRECTORY}/python)
    file(MAKE_DIRECTORY ${HSBA_PROTO_OUT_DIRECTORY}/python)
  endif()
  FOREACH(proto_file ${proto_files})
    STRING(REGEX REPLACE "[^/]proto" "" proto_file_name ${proto_file})
    get_filename_component(proto_file_name ${proto_file_name} NAME)
    LIST(APPEND PROTO_SRCS "${HSBA_PROTO_OUT_DIRECTORY}/python/${proto_file_name}_pb2.py")
	ADD_CUSTOM_COMMAND(
		OUTPUT "${HSBA_PROTO_OUT_DIRECTORY}/python/${proto_file_name}_pb2.py"
		COMMAND ${PROTOBUF_PROTOC_EXECUTABLE} --proto_path=${CMAKE_CURRENT_SOURCE_DIR}
                       --python_out=${HSBA_PROTO_OUT_DIRECTORY}/python
                       ${proto_file} 
		DEPENDS ${proto_file}
	)
ENDFOREACH(proto_file)
endif()

option(HSBA_PROTO_OUT_PHP "protobuf out php source" ON)

if(HSBA_PROTO_OUT_PHP)
  if(NOT EXISTS ${HSBA_PROTO_OUT_DIRECTORY}/php)
    file(MAKE_DIRECTORY ${HSBA_PROTO_OUT_DIRECTORY}/php)
  endif()
  FOREACH(proto_file ${proto_files})
    STRING(REGEX REPLACE "[^/]proto" "" proto_file_name ${proto_file})
    get_filename_component(proto_file_name ${proto_file_name} NAME)
    LIST(APPEND PROTO_SRCS "${HSBA_PROTO_OUT_DIRECTORY}/php/HsbaProto/msg_${proto_file_name}.php")
	ADD_CUSTOM_COMMAND(
		OUTPUT "${HSBA_PROTO_OUT_DIRECTORY}/php/HsbaProto/msg_${proto_file_name}.php"
		COMMAND ${PROTOBUF_PROTOC_EXECUTABLE} --proto_path=${CMAKE_CURRENT_SOURCE_DIR}
                       --php_out=${HSBA_PROTO_OUT_DIRECTORY}/php
                       ${proto_file} 
		DEPENDS ${proto_file}
	)
ENDFOREACH(proto_file)
endif()

add_custom_target(proto_install ALL DEPENDS ${PROTO_SRCS})

endif()

#protobuf project native
PROTOBUF_GENERATE_CPP(PROTOSRCS PROTOHDRS ${proto_files})
include_directories(./proto)
add_library(HsBaSlicerProto STATIC ${PROTOSRCS} ${PROTOHDRS}) 
target_link_libraries(HsBaSlicerProto protobuf::libprotobuf) 
target_include_directories(HsBaSlicerProto PUBLIC ${CMAKE_CURRENT_BINARY_DIR})