if(NOT BUILD_SHARED_LIBS)
    add_library(HsBaSlicerLog STATIC
    logger.hpp
    logger.cpp
    )
else()
    add_library(HsBaSlicerLog SHARED
    logger.hpp
    logger.cpp
    )
    # Define the export macro for Windows
    target_compile_definitions(HsBaSlicerLog PRIVATE HSBA_SLICER_LOG_EXPORTS)
    # Add the export header to the target
    target_sources(HsBaSlicerLog PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/export.h>
    )
endif()

if(WIN32)
    target_link_libraries(HsBaSlicerLog PUBLIC
    HsBaSlicerBase
    HsBaSlicerFileOperator
    Boost::boost
    Boost::log
    )
elseif(ANDROID)
    target_link_libraries(HsBaSlicerLog PUBLIC
    HsBaSlicerBase
    HsBaSlicerFileOperator
    log
    )
else()
    target_link_libraries(HsBaSlicerLog PUBLIC
    HsBaSlicerBase
    HsBaSlicerFileOperator
    Boost::boost
    Boost::log_setup Boost::log
    )
endif()