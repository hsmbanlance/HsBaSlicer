add_library(HsBaSlicer2D STATIC 
	IntPolygon.hpp
	IntPolygon.cpp
	2Dhull.hpp
	2Dhull.cpp
	FloatPolygons.hpp
	FloatPolygons.cpp
	PolygonFill.hpp
	PolygonFill.cpp
	ImageToPolygons.cpp
	)

find_package(PNG REQUIRED)
find_package(JPEG REQUIRED)
find_package(Lua REQUIRED)

target_include_directories(HsBaSlicer2D PRIVATE ${LUA_INCLUDE_DIR})

target_link_libraries(HsBaSlicer2D PUBLIC 
	HsBaSlicerBase
	PkgConfig::Clipper2
	PNG::PNG JPEG::JPEG
	${LUA_LIBRARIES}
)
