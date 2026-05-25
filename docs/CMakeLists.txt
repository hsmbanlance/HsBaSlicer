# Doxygen docs target
find_package(Doxygen COMPONENTS dot)

if(DOXYGEN_FOUND)
    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM
    )
else()
    message(WARNING "Doxygen not found. Skipping 'docs' target. Install Doxygen to enable documentation generation.")
endif()