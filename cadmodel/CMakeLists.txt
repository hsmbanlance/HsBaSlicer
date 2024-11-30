add_library(HsBaSlicerCADModel STATIC
	OcctModel.hpp
	OcctModel.cpp
)

find_package(OpenCASCADE CONFIG REQUIRED)

target_link_libraries(HsBaSlicerCADModel PUBLIC
	HsBaSlicerBase
	${OpenCASCADE_LIBRARIES} 
	HsBaSlicerMesh
)