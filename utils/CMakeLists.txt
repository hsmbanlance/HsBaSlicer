add_library(HsBaSlicerUtils STATIC
	pch_headers.hpp
	logger.hpp
	logger.cpp
	app_config.hpp
	app_config.cpp)

find_package(boost_date_time CONFIG REQUIRED)

target_link_libraries(HsBaSlicerUtils PUBLIC
	Boost::date_time
	HsBaSlicerBase
	HsBaSlicerFileOperator
)

target_precompile_headers(HsBaSlicerUtils PRIVATE pch_headers.hpp)