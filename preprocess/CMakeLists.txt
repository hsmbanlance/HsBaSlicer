add_library(HsBaPreprocess STATIC
    ModelLoader.hpp
    ModelLoader.cpp)

target_link_libraries(HsBaPreprocess PUBLIC
    HsBaSlicerBase
    HsBaSlicerMesh
)

# OCCT / CAD kernel is a copyleft kernel, not available without HSBA_COPL or on Android, iOS, or game consoles
if(NOT ANDROID AND NOT IOS AND NOT HSBA_GAME_CONSOLE)
    target_link_libraries(HsBaPreprocess PUBLIC HsBaSlicerPointCloud)
    if(HSBA_COPL)
        target_link_libraries(HsBaPreprocess PUBLIC HsBaSlicerCADModel)
    endif()
endif()
