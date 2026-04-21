add_library(HsBaSlicerCADModel STATIC
	OcctModel.hpp
	OcctModel.cpp
	UserCustomCADModel.hpp
	UserCustomCADModel.cpp)

find_package(OpenCASCADE CONFIG REQUIRED)

if(NOT ANDROID AND NOT IOS AND NOT SWITCH)
  find_package(Boost REQUIRED COMPONENTS dll)
endif()

target_link_libraries(HsBaSlicerCADModel PUBLIC
	HsBaSlicerBase
	${OpenCASCADE_LIBRARIES} 
	HsBaSlicerMesh
)

if(NOT ANDROID AND NOT IOS AND NOT SWITCH)
  target_link_libraries(HsBaSlicerCADModel PUBLIC Boost::dll)
  target_compile_definitions(HsBaSlicerCADModel PUBLIC HAS_BOOST_DLL)
endif()