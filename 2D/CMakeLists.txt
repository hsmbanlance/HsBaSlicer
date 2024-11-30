add_library(HsBaSlicer2D STATIC 
	IntPolygon.hpp
	IntPolygon.cpp
	2Dhull.hpp
	2Dhull.cpp
	FloatPolygons.hpp
	FloatPolygons.cpp)

target_link_libraries(HsBaSlicer2D PUBLIC 
	HsBaSlicerBase
	PkgConfig::Clipper2
)