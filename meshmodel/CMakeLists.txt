if(NOT IOS AND NOT HSBA_GAME_CONSOLE)
add_library(HsBaSlicerMesh STATIC
	IglModel.hpp
    IglModel.cpp
    FullTopoModel.hpp
    FullTopoModel.cpp)

target_include_directories(HsBaSlicerMesh PRIVATE ${LUA_INCLUDE_DIR})
target_link_libraries(HsBaSlicerMesh PUBLIC 
    igl::igl_core
    HsBaSlicerBase
    HsBaSlicerProto
    HsBaSlicer2D
    ${LUA_LIBRARIES}
)

# CGAL is a copyleft kernel, only available when HSBA_COPL is enabled
if(HSBA_COPL)
  target_sources(HsBaSlicerMesh PRIVATE
      CgalModel.hpp
      CgalModel.cpp)
  target_link_libraries(HsBaSlicerMesh PUBLIC 
      CGAL::CGAL
      igl_copyleft::igl_copyleft_cgal igl_copyleft::igl_copyleft_core
  )
endif()
else()
add_library(HsBaSlicerMesh STATIC
	IglModel.hpp
    IglModel.cpp
    FullTopoModel.hpp
    FullTopoModel.cpp)

target_include_directories(HsBaSlicerMesh PRIVATE ${LUA_INCLUDE_DIR})
target_link_libraries(HsBaSlicerMesh PUBLIC 
    igl::igl_core
    HsBaSlicerBase
    HsBaSlicerProto
    HsBaSlicer2D
    ${LUA_LIBRARIES}
)
endif()