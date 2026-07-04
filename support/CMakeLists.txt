add_library(HsBaSupport STATIC
    ISupport.hpp
    ISupport.cpp
    SupportConfig.hpp
    OverhangDetector.hpp
    OverhangDetector.cpp
    FdmSupport.hpp
    FdmSupport.cpp
    SlaSupport.hpp
    SlaSupport.cpp
    LuaSupport.hpp
    LuaSupport.cpp
    LuaAdapter.hpp
    LuaAdapter.cpp)

target_include_directories(HsBaSupport PRIVATE ${LUA_INCLUDE_DIR})
target_link_libraries(HsBaSupport PUBLIC
    HsBaSlicerBase
    HsBaSlicer2D
    HsBaSlicerUtils
    PkgConfig::Clipper2
    ${LUA_LIBRARIES})
