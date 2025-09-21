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
  DEPENDS anyvisit_test coroutines_test filename_check_test configmap_test boost_std_convert_test enum_simple_convert_test
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