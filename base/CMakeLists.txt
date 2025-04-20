add_library(HsBaSlicerBase STATIC
	error.hpp
    error.cpp
    IModel.hpp
    ModelFormat.hpp
    ModelFormat.cpp
    encoding_convert.hpp
    encoding_convert.cpp
    filename_check.hpp
    filename_check.cpp
    concepts.hpp
    template_helper.hpp
    coroutine.hpp
    any_visit.hpp
    tuple_each.hpp
    boost_std_convert.hpp
    base_interface.hpp
    eigen_translator.hpp
    eigen_translator.cpp
    units.hpp
)

find_package(boost_locale REQUIRED CONFIG)
find_package(boost_nowide REQUIRED CONFIG)

target_link_libraries(HsBaSlicerBase PUBLIC Eigen3::Eigen Boost::boost Boost::locale Boost::nowide)

