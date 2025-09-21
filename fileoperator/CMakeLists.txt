add_library(HsBaSlicerFileOperator STATIC
	IZipper.hpp
	IUnzipper.hpp
	rw_ptree.hpp
	rw_ptree.cpp
	zipper.hpp
	zipper.cpp
	bit7z_def.hpp
	bit7z_zipper.hpp
	bit7z_zipper.cpp
	properties_doc.hpp
	properties_doc.cpp
	sql_adapter.hpp
	sql_adapter.cpp
	unzipper.hpp
	unzipper.cpp
	bit7z_unzipper.hpp
	bit7z_unzipper.cpp
)

if(NOT ANDROID)
	find_package(boost_log CONFIG REQUIRED)
endif()

target_link_libraries(HsBaSlicerFileOperator PUBLIC HsBaSlicerBase 
	rapidjson
)

if(HSBA_USE_BIT7Z AND NOT ANDROID)
  find_package(unofficial-bit7z CONFIG REQUIRED)
  target_link_libraries(HsBaSlicerFileOperator PUBLIC unofficial::bit7z::bit7z64)
endif()


target_link_libraries(HsBaSlicerFileOperator PUBLIC miniz::miniz)

#Link Sqlpp11
if(NOT ANDROID AND NOT IOS AND NOT SWITCH)
	target_link_libraries(HsBaSlicerFileOperator PUBLIC sqlpp11::sqlite3 sqlpp11::mysql sqlpp11::postgresql)
else()
	target_link_libraries(HsBaSlicerFileOperator PUBLIC sqlpp11::sqlite3)
endif()