add_library(utils_static_tests STATIC
    utils_static_tests.cpp
)

target_link_libraries(utils_static_tests
    PRIVATE
        HsBaSlicerUtils
)