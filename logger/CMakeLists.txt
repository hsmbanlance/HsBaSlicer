add_library(HsBaSlicerLog STATIC
	logger.hpp
	logger.cpp
)

if(WIN32)
target_link_libraries(HsBaSlicerLog PUBLIC
	HsBaSlicerBase
	Boost::boost
	Boost::log
)
elseif(ANDROID)
target_link_libraries(HsBaSlicerLog PUBLIC
	HsBaSlicerBase
	log
)
else()
target_link_libraries(HsBaSlicerUtils PUBLIC
	HsBaSlicerBase
	Boost::boost
	Boost::log_setup Boost::log
)
endif()