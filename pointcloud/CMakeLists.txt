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
    ${Boost_LIBRARIES}
    TBB::tbb
    Imath::Imath
    HsBaSlicerBase
)

if(OpenVDB_FOUND)
  # Link the imported target instead of ${OpenVDB_LIBRARIES}: the latter is a
  # bare library path from vcpkg's FindOpenVDB and carries no transitive deps.
  target_link_libraries(HsBaSlicerPointCloud PUBLIC OpenVDB::openvdb)

  # vcpkg's FindOpenVDB attaches blosc/ZLIB only via
  # IMPORTED_LINK_DEPENDENT_LIBRARIES, which is ignored for static archives
  # (GNU ld reports undefined blosc_*/compress* references), so link them here.
  find_package(blosc CONFIG REQUIRED)
  find_package(ZLIB REQUIRED)
  find_package(Boost REQUIRED COMPONENTS iostreams)
  # blosc's exported target name differs between triplets (blosc_shared/blosc_static)
  if(TARGET blosc_shared)
    set(_HSBA_BLOSC_TARGET blosc_shared)
  elseif(TARGET blosc_static)
    set(_HSBA_BLOSC_TARGET blosc_static)
  else()
    set(_HSBA_BLOSC_TARGET blosc)
  endif()
  target_link_libraries(HsBaSlicerPointCloud PUBLIC
      ${_HSBA_BLOSC_TARGET}
      ZLIB::ZLIB
      Boost::iostreams
  )
endif()

if(HSBA_DESKTOP)
  target_link_libraries(HsBaSlicerPointCloud PUBLIC Boost::dll)
endif()

# OpenVDB level-set reconstruction templates exceed MSVC 65535 section limit
if(MSVC)
    set_source_files_properties(OpenVdbModel_mesh.cpp PROPERTIES COMPILE_OPTIONS "/bigobj")
endif()