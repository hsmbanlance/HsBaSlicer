add_library(HsBaSlicerCADModel STATIC
	OcctModel.hpp
	OcctModel.cpp
	UserCustomCADModel.hpp
	UserCustomCADModel.cpp)

find_package(OpenCASCADE CONFIG REQUIRED)

if(NOT ANDROID AND NOT IOS AND NOT HSBA_GAME_CONSOLE)
  find_package(Boost REQUIRED COMPONENTS dll)
endif()

target_link_libraries(HsBaSlicerCADModel PUBLIC
	HsBaSlicerBase
	${OpenCASCADE_LIBRARIES} 
	HsBaSlicerMesh
)

if(HSBA_DESKTOP)
  target_link_libraries(HsBaSlicerCADModel PUBLIC Boost::dll)
endif()