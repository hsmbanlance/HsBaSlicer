add_library(HsBaSlicerMesh STATIC
	IglModel.hpp
    IglModel.cpp
    CgalModel.hpp
    CgalModel.cpp
    FullTopoModel.hpp
    FullTopoModel.cpp)

target_link_libraries(HsBaSlicerMesh PUBLIC 
    CGAL::CGAL
    igl::igl_core igl_copyleft::igl_copyleft_cgal igl_copyleft::igl_copyleft_core
    HsBaSlicerBase
    HsBaSlicerProto
    HsBaSlicer2D
)