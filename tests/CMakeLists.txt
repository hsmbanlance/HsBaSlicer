find_package(Boost 1.54 REQUIRED unit_test_framework)

#force utf-8
if(MSVC)
  add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
  add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
endif()

#any_visit_test
add_executable(anyvisit_test AnyVisit/anyvisit_test.cpp)

target_link_libraries(anyvisit_test
	HsBaSlicerBase
	Boost::unit_test_framework
)

target_compile_definitions(anyvisit_test
  PRIVATE
      BOOST_TEST_DYN_LINK
  )

enable_testing()
add_test(
  NAME anyvisit_test
  COMMAND $<TARGET_FILE:anyvisit_test>
  -t any_visit
  )

#coroutines_test
add_executable(coroutines_test Coroutines/coroutines_test.cpp)

target_link_libraries(coroutines_test
	HsBaSlicerBase
	Boost::unit_test_framework
)

target_compile_definitions(coroutines_test
  PRIVATE
      BOOST_TEST_DYN_LINK
  )

enable_testing()
add_test(
  NAME coroutines_test
  COMMAND $<TARGET_FILE:coroutines_test>
  -t generator_tasks
  )

#filename check and operator
#test for filename check
add_executable(filename_check_test
	FilesOperator/filename_check_test.cpp
)

target_link_libraries(filename_check_test
	HsBaSlicerBase
	Boost::unit_test_framework
)

target_compile_definitions(filename_check_test
  PRIVATE
      BOOST_TEST_DYN_LINK
  )

enable_testing()
add_test(
  NAME filename_check_test
  COMMAND $<TARGET_FILE:filename_check_test>
  )

#ConfigMap
#test for configmap used std::variant
add_executable(configmap_test ConfigMap/configmap_test.cpp)

target_link_libraries(configmap_test
	HsBaSlicerFileOperator
	Eigen3::Eigen
	Boost::unit_test_framework
)

target_compile_definitions(configmap_test
  PRIVATE
      BOOST_TEST_DYN_LINK
  )

enable_testing()
add_test(
  NAME configmap_test
  COMMAND $<TARGET_FILE:configmap_test>
  -t configmap
  )

#boost std convert
add_executable(boost_std_convert_test BoostStdConvert/boost_std_convert_test.cpp)

target_link_libraries(boost_std_convert_test
	HsBaSlicerBase
	Boost::unit_test_framework
)

target_compile_definitions(boost_std_convert_test
  PRIVATE
      BOOST_TEST_DYN_LINK
  )

enable_testing()
add_test(
  NAME boost_std_convert_test
  COMMAND $<TARGET_FILE:boost_std_convert_test>
  -t boost_std_convert
  )


#enum simple convert test
add_executable(enum_simple_convert_test EnumSimpleConvert/enum_simple_convert_test.cpp)

target_link_libraries(enum_simple_convert_test
	HsBaSlicerBase
	Boost::unit_test_framework
)

target_compile_definitions(enum_simple_convert_test
  PRIVATE
      BOOST_TEST_DYN_LINK
  )

enable_testing()
add_test(
  NAME enum_simple_convert_test
  COMMAND $<TARGET_FILE:enum_simple_convert_test>
  -t enum_simple_convert
  )

add_custom_target(RunAllTests
  COMMAND ${CMAKE_CTEST_COMMAND} -V
  DEPENDS anyvisit_test coroutines_test filename_check_test configmap_test boost_std_convert_test enum_simple_convert_test cipher_test
)

add_custom_command(TARGET anyvisit_test POST_BUILD
  COMMAND ${CMAKE_CTEST_COMMAND} -V -R anyvisit_test
)

add_custom_command(TARGET coroutines_test POST_BUILD
  COMMAND ${CMAKE_CTEST_COMMAND} -V -R coroutines_test
)

add_custom_command(TARGET filename_check_test POST_BUILD
  COMMAND ${CMAKE_CTEST_COMMAND} -V -R filename_check_test
)

add_custom_command(TARGET configmap_test POST_BUILD
  COMMAND ${CMAKE_CTEST_COMMAND} -V -R configmap_test
)

add_custom_command(TARGET boost_std_convert_test POST_BUILD
  COMMAND ${CMAKE_CTEST_COMMAND} -V -R boost_std_convert_test 
)

add_custom_command(TARGET enum_simple_convert_test POST_BUILD
  COMMAND ${CMAKE_CTEST_COMMAND} -V -R enum_simple_convert_test 
)

#filename check and operator
#test for sql operator
add_executable(sqlite_test
	FilesOperator/sqlite_test.cpp
)

target_link_libraries(sqlite_test
	HsBaSlicerBase
	HsBaSlicerFileOperator
	Boost::unit_test_framework
)

target_compile_definitions(sqlite_test
  PRIVATE
      BOOST_TEST_DYN_LINK
  )

enable_testing()
add_test(
  NAME sqlite_test
  COMMAND $<TARGET_FILE:sqlite_test>
  )


add_custom_command(TARGET sqlite_test POST_BUILD
  COMMAND ${CMAKE_CTEST_COMMAND} -V -R sqlite_test 
)

#filename check and operator
#test for zipper operator
add_executable(zipper_test
	FilesOperator/zipper_test.cpp
)

target_link_libraries(zipper_test
	HsBaSlicerBase
	HsBaSlicerFileOperator
	Boost::unit_test_framework
)

target_compile_definitions(zipper_test
  PRIVATE
      BOOST_TEST_DYN_LINK
  )

enable_testing()
add_test(
  NAME zipper_test
  COMMAND $<TARGET_FILE:zipper_test>
  )


add_custom_command(TARGET zipper_test POST_BUILD
  COMMAND ${CMAKE_CTEST_COMMAND} -V -R zipper_test
)

#json convert test
#test for json convert
add_executable(json_convert_test
	"ConfigMap/json_convert_test.cpp"
)

target_link_libraries(json_convert_test
	HsBaSlicerBase
	HsBaSlicerUtils
	Boost::unit_test_framework
)

target_compile_definitions(json_convert_test
  PRIVATE
      BOOST_TEST_DYN_LINK
  )

enable_testing()
add_test(
  NAME json_convert_test
  COMMAND $<TARGET_FILE:json_convert_test>
  )


add_custom_command(TARGET json_convert_test POST_BUILD
  COMMAND ${CMAKE_CTEST_COMMAND} -V -R json_convert
)

# cipher tests
add_executable(cipher_test
    Cipher/cipher_test.cpp
)

target_link_libraries(cipher_test
    HsBaCipher
    Boost::unit_test_framework
)

target_compile_definitions(cipher_test
  PRIVATE
      BOOST_TEST_DYN_LINK
  )

enable_testing()
add_test(
  NAME cipher_test
  COMMAND $<TARGET_FILE:cipher_test>
  )

add_custom_command(TARGET cipher_test POST_BUILD
  COMMAND ${CMAKE_CTEST_COMMAND} -V -R cipher_test
)


#string utils test
#test for string utils
add_executable(string_utils_test
	"StringUtils/string_utils_test.cpp"
)

target_link_libraries(string_utils_test
	HsBaSlicerBase
	Boost::unit_test_framework
)

target_compile_definitions(string_utils_test
  PRIVATE
      BOOST_TEST_DYN_LINK
  )

enable_testing()
add_test(
  NAME string_utils_test
  COMMAND $<TARGET_FILE:string_utils_test>
  )


add_custom_command(TARGET string_utils_test POST_BUILD
  COMMAND ${CMAKE_CTEST_COMMAND} -V -R string_utils_test
)

#points path test
#test for points path
add_executable(points_path_test
	"PathsOut/points_path_test.cpp"
)

target_link_libraries(points_path_test
	HsBaSlicerBase
	HsBaPaths
	Boost::unit_test_framework
)

target_compile_definitions(points_path_test
  PRIVATE
      BOOST_TEST_DYN_LINK
  )

enable_testing()
add_test(
  NAME points_path_test
  COMMAND $<TARGET_FILE:points_path_test>
  )


add_custom_command(TARGET points_path_test POST_BUILD
  COMMAND ${CMAKE_CTEST_COMMAND} -V -R points_path_test
)

# layers path test
add_executable(layers_path_test
    "PathsOut/layers_path_test.cpp"
)

target_link_libraries(layers_path_test
    HsBaSlicerBase
    HsBaPaths
    Boost::unit_test_framework
)

target_compile_definitions(layers_path_test
  PRIVATE
      BOOST_TEST_DYN_LINK
)

enable_testing()
add_test(
  NAME layers_path_test
  COMMAND $<TARGET_FILE:layers_path_test>
)

add_custom_command(TARGET layers_path_test POST_BUILD
  COMMAND ${CMAKE_CTEST_COMMAND} -V -R layers_path_test
)

# images path test
add_executable(images_path_test
    "PathsOut/images_path_test.cpp"
)

target_link_libraries(images_path_test
    HsBaSlicerBase
    HsBaPaths
    HsBaCipher
    Boost::unit_test_framework
)

target_compile_definitions(images_path_test
  PRIVATE
      BOOST_TEST_DYN_LINK
)

enable_testing()
add_test(
  NAME images_path_test
  COMMAND $<TARGET_FILE:images_path_test>
)

add_custom_command(TARGET images_path_test POST_BUILD
  COMMAND ${CMAKE_CTEST_COMMAND} -V -R images_path_test
)

include_directories(${CMAKE_SOURCE_DIR})


# Only add OCCT test if CAD model library was built (OpenCASCADE found)
if(TARGET HsBaSlicerCADModel)
  find_package(Fontconfig REQUIRED)
  find_package(Freetype REQUIRED)
  add_executable(occt_model_test Models/occt_model_test.cpp)
  target_link_libraries(occt_model_test PRIVATE Boost::unit_test_framework
    HsBaSlicerCADModel Eigen3::Eigen)
  target_compile_definitions(occt_model_test PRIVATE BOOST_TEST_DYN_LINK)
  enable_testing()
  add_test(NAME occt_model_test COMMAND $<TARGET_FILE:occt_model_test>)
  add_custom_command(TARGET occt_model_test POST_BUILD
  COMMAND ${CMAKE_CTEST_COMMAND} -V -R occt_model_test
)
endif()

add_executable(cgal_model_test Models/cgal_model_test.cpp)
add_executable(igl_model_test Models/igl_model_test.cpp)

target_link_libraries(cgal_model_test PRIVATE Boost::unit_test_framework HsBaSlicerMesh Eigen3::Eigen)
target_link_libraries(igl_model_test PRIVATE Boost::unit_test_framework HsBaSlicerMesh Eigen3::Eigen)

# Always enable Boost test dynamic link
target_compile_definitions(cgal_model_test PRIVATE BOOST_TEST_DYN_LINK)
target_compile_definitions(igl_model_test PRIVATE BOOST_TEST_DYN_LINK)

# In Debug configuration, disable boolean operation tests for CGAL and IGL
target_compile_definitions(cgal_model_test PRIVATE $<$<CONFIG:Debug>:DISABLE_BOOLEAN_OPERATIONS_TESTS>)
target_compile_definitions(igl_model_test PRIVATE $<$<CONFIG:Debug>:DISABLE_BOOLEAN_OPERATIONS_TESTS>)

enable_testing()
add_test(NAME cgal_model_test COMMAND $<TARGET_FILE:cgal_model_test>)
add_test(NAME igl_model_test COMMAND $<TARGET_FILE:igl_model_test>)
add_custom_command(TARGET cgal_model_test POST_BUILD
  COMMAND ${CMAKE_CTEST_COMMAND} -V -R cgal_model_test
)
add_custom_command(TARGET igl_model_test POST_BUILD
  COMMAND ${CMAKE_CTEST_COMMAND} -V -R igl_model_test
)

add_executable(full_topo_model_test Models/full_topo_model_test.cpp)

target_compile_definitions(full_topo_model_test
  PRIVATE
      BOOST_TEST_DYN_LINK
  )

target_link_libraries(full_topo_model_test
	PRIVATE
	HsBaSlicerMesh
	Boost::unit_test_framework
)

enable_testing()
add_test(
  NAME full_topo_model_test
  COMMAND $<TARGET_FILE:full_topo_model_test>
  -t full_topo_model_test
  )

add_custom_command(TARGET full_topo_model_test POST_BUILD
  COMMAND ${CMAKE_CTEST_COMMAND} -V -R full_topo_model_test
)


## PolygonFill tests
add_executable(polygon_fill_test
    "PolygonFill/polygon_fill_test.cpp"
)

target_link_libraries(polygon_fill_test
    HsBaSlicer2D
    Boost::unit_test_framework
)

target_compile_definitions(polygon_fill_test
  PRIVATE
      BOOST_TEST_DYN_LINK
)

enable_testing()
add_test(
  NAME polygon_fill_test
  COMMAND $<TARGET_FILE:polygon_fill_test>
)

add_custom_command(TARGET polygon_fill_test POST_BUILD
  COMMAND ${CMAKE_CTEST_COMMAND} -V -R polygon_fill_test
)

## ImagePolygons tests
add_executable(image_polygons_test
    "PolygonFill/image_polygons_test.cpp"
)

target_link_libraries(image_polygons_test
    HsBaSlicer2D
    Boost::unit_test_framework
)

target_compile_definitions(image_polygons_test
  PRIVATE
      BOOST_TEST_DYN_LINK
)

enable_testing()
add_test(
  NAME image_polygons_test
  COMMAND $<TARGET_FILE:image_polygons_test>
)

add_custom_command(TARGET image_polygons_test POST_BUILD
  COMMAND ${CMAKE_CTEST_COMMAND} -V -R image_polygons_test
)


## Static Reflect tests
add_executable(static_reflect_test
    "Reflect/static_reflect_test.cpp"
)

target_link_libraries(static_reflect_test
    HsBaSlicerBase
    Boost::unit_test_framework
)

target_compile_definitions(static_reflect_test
  PRIVATE
      BOOST_TEST_DYN_LINK
)

enable_testing()
add_test(
  NAME static_reflect_test
  COMMAND $<TARGET_FILE:static_reflect_test>
)

add_custom_command(TARGET static_reflect_test POST_BUILD
  COMMAND ${CMAKE_CTEST_COMMAND} -V -R static_reflect_test
)