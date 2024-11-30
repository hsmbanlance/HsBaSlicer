add_library(HsBaSlicerConverter STATIC
    Eigen2Msg.hpp
    Eigen2Msg.cpp
    Msg2Eigen.hpp
    Msg2Eigen.cpp
    )

target_link_libraries(HsBaSlicerConverter PUBLIC Eigen3::Eigen HsBaSlicerProto)