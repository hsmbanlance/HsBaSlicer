add_library(HsBaSlicerCADModel STATIC
	OcctModel.hpp
	OcctModel.cpp
	UserCustomCADModel.hpp
	UserCustomCADModel.cpp)

find_package(OpenCASCADE CONFIG REQUIRED)

find_package(Boost REQUIRED COMPONENTS dll)

target_link_libraries(HsBaSlicerCADModel PUBLIC
	HsBaSlicerBase
	${OpenCASCADE_LIBRARIES} 
	HsBaSlicerMesh
	Boost::dll
)