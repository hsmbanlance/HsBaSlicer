add_library(base_static_tests STATIC
	base_static_tests.cpp
)

target_link_libraries(base_static_tests
	PRIVATE
		HsBaSlicerBase
)