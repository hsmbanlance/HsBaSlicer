add_library(HsBaPaths STATIC
	IPath.hpp
	pointspath.hpp
	pointspath.cpp
	robotpath.hpp
	robotpath.cpp 
	imagespath.hpp
	imagespath.cpp
	layerspath.hpp
	layerspath.cpp)

target_include_directories(HsBaPaths PRIVATE ${LUA_INCLUDE_DIR})
target_link_libraries(HsBaPaths PUBLIC HsBaSlicerBase HsBaSlicerFileOperator HsBaCipher HsBaSlicer2D HsBaSlicerUtils
${LUA_LIBRARIES}
)