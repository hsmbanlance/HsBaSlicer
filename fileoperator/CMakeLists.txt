add_library(HsBaSlicerFileOperator STATIC
	IZipper.hpp
	rw_ptree.hpp
	rw_ptree.cpp 
	bit7z_zipper.hpp
	bit7z_zipper.cpp
	properties_doc.hpp
	properties_doc.cpp)

find_package(boost_log CONFIG REQUIRED)

target_link_libraries(HsBaSlicerFileOperator PUBLIC HsBaSlicerBase 
	Boost::log
	rapidjson
)

if(HSBA_USE_BIT7Z)
  find_package(unofficial-bit7z CONFIG REQUIRED)
  target_link_libraries(HsBaSlicerFileOperator PUBLIC unofficial::bit7z::bit7z64)
endif()