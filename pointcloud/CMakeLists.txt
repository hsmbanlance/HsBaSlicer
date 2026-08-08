find_package(TBB CONFIG REQUIRED)
find_package(Imath CONFIG REQUIRED)

add_library(HsBaSlicerPointCloud STATIC
    OpenVdbModel.hpp
    OpenVdbModel_internal.h
    OpenVdbModel.cpp
    OpenVdbModel_mesh.cpp
    OpenVdbModel_analysis.cpp
    UserCustomPointCloudModel.hpp
    UserCustomPointCloudModel.cpp
)

if(NOT ANDROID AND NOT IOS AND NOT HSBA_GAME_CONSOLE)
  find_package(Boost REQUIRED COMPONENTS dll)
endif()

target_link_libraries(HsBaSlicerPointCloud PUBLIC
    ${OpenVDB_LIBRARIES}
    ${Boost_LIBRARIES}
    TBB::tbb
    Imath::Imath
    HsBaSlicerBase
)

if(HSBA_DESKTOP)
  target_link_libraries(HsBaSlicerPointCloud PUBLIC Boost::dll)
endif()

# OpenVDB level-set reconstruction templates exceed MSVC 65535 section limit
if(MSVC)
    set_source_files_properties(OpenVdbModel_mesh.cpp PROPERTIES COMPILE_OPTIONS "/bigobj")
endif()