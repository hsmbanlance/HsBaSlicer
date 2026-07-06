add_library(HsBaCipher STATIC
	encoder.hpp
	encoder.cpp
	encrypt.hpp
	encrypt.cpp
	hasher.hpp
	hasher.cpp
	LuaAdapter.cpp
)

find_package(Lua REQUIRED)

target_include_directories(HsBaCipher PUBLIC
    $<BUILD_INTERFACE:${LUA_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(HsBaCipher PUBLIC HsBaSlicerBase
	OpenSSL::SSL OpenSSL::Crypto
	${LUA_LIBRARIES}
)