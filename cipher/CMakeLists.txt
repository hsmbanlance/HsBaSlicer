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
	${LUA_INCLUDE_DIR}
)

target_link_libraries(HsBaCipher PUBLIC HsBaSlicerBase
	OpenSSL::SSL OpenSSL::Crypto
	${LUA_LIBRARIES}
)