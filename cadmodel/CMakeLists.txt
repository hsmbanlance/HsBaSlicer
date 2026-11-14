add_library(HsBaSlicerCADModel STATIC
	OcctModel.hpp
	OcctModel.cpp
	OcctCsg.hpp
	OcctCsg.cpp
	OcctTypeAliases.hpp
	UserCustomCADModel.hpp
	UserCustomCADModel.cpp)

find_package(OpenCASCADE CONFIG REQUIRED)

if(NOT ANDROID AND NOT IOS AND NOT HSBA_GAME_CONSOLE)
  find_package(Boost REQUIRED COMPONENTS dll)
endif()

target_link_libraries(HsBaSlicerCADModel PUBLIC
	HsBaSlicerBase
	${OpenCASCADE_LIBRARIES} 
	HsBaSlicerMesh
)

if(HSBA_DESKTOP)
  target_link_libraries(HsBaSlicerCADModel PUBLIC Boost::dll)
endif()

# The OCCT DRAW Test Harness (TKDraw/TKTopTest), which builds B-Rep shapes from text
# CSG scripts, only exists when the opencascade port was built with its 'tcl' feature;
# see overlay-ports/opencascade. The definition is public so every consumer of the CAD
# kernel sees the same shape of the API.
option(HSBA_REQUIRE_TEXT_CSG
       "Fail configuration when the OCCT DRAW based text CSG bridge is unavailable" ON)

if(OpenCASCADE_WITH_TCL AND TARGET TKDraw AND TARGET TKTopTest)
  target_compile_definitions(HsBaSlicerCADModel PUBLIC USE_OCCT_DRAW)
  message(STATUS "Text CSG (OCCT DRAW Test Harness): enabled")
else()
  # An OpenCASCADE package without the Draw module compiles and tests fine while every CSG
  # script fails at run time, so the degraded state has to be a deliberate choice rather
  # than something a stale vcpkg_installed tree or a restored binary cache slips in.
  if(TARGET TKDraw)
    set(HSBA_CSG_TKDRAW "present")
  else()
    set(HSBA_CSG_TKDRAW "missing")
  endif()
  message(STATUS "Text CSG (OCCT DRAW Test Harness): unavailable")
  if(HSBA_REQUIRE_TEXT_CSG)
    message(FATAL_ERROR
      "Text CSG needs OpenCASCADE built with the 'tcl' feature of overlay-ports/opencascade, "
      "but the package in use has no DRAW Test Harness (OpenCASCADE_WITH_TCL='${OpenCASCADE_WITH_TCL}', "
      "TKDraw=${HSBA_CSG_TKDRAW}). Check that overlay-ports/opencascade/ is committed, that "
      "vcpkg-configuration.json still declares the overlay directory, and that OpenCASCADE was really "
      "rebuilt: a vcpkg_installed tree or a binary cache entry from before the overlay landed also "
      "triggers this. Configure with -DHSBA_REQUIRE_TEXT_CSG=OFF to build without text CSG on purpose.")
  else()
    # Only a build that opted out reaches this point. The flag tells the test suite that the
    # missing DRAW module is intended, so it may assert the degradation instead of silently
    # passing on a build where it was an accident.
    target_compile_definitions(HsBaSlicerCADModel PUBLIC HSBA_TEXT_CSG_DISABLED)
  endif()
endif()