add_library(HsBaSlicerUtils STATIC
	pch_headers.hpp
	logger.hpp
	logger.cpp
	app_config.hpp
	app_config.cpp
	struct_json.hpp)

find_package(boost_date_time CONFIG REQUIRED)

if(WIN32)
target_link_libraries(HsBaSlicerUtils PUBLIC
	Boost::date_time
	HsBaSlicerBase
	HsBaSlicerFileOperator
	rapidjson
	Boost::log
)
elseif(ANDROID)
target_link_libraries(HsBaSlicerUtils PUBLIC
	Boost::date_time
	HsBaSlicerBase
	HsBaSlicerFileOperator
	rapidjson
	log
)
else()
target_link_libraries(HsBaSlicerUtils PUBLIC
	Boost::date_time
	HsBaSlicerBase
	HsBaSlicerFileOperator
	rapidjson
	Boost::log_setup Boost::log
)
endif()

target_precompile_headers(HsBaSlicerUtils PRIVATE pch_headers.hpp)