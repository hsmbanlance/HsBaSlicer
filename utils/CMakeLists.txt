add_library(HsBaSlicerUtils STATIC
	pch_headers.hpp
	app_config.hpp
	app_config.cpp
	struct_json.hpp)

find_package(boost_date_time CONFIG REQUIRED)

target_link_libraries(HsBaSlicerUtils PUBLIC
	Boost::date_time
	HsBaSlicerBase
	HsBaSlicerFileOperator
	rapidjson
)

target_precompile_headers(HsBaSlicerUtils PRIVATE pch_headers.hpp)