add_library(HsBaPaths STATIC
	IPath.hpp
	pointspath.hpp
	pointspath.cpp
)

target_include_directories(HsBaPaths PRIVATE ${LUA_INCLUDE_DIR})
target_link_libraries(HsBaPaths PUBLIC HsBaSlicerBase 
${LUA_LIBRARIES}
)