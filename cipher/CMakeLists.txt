add_library(HsBaCipher STATIC
	encoder.hpp
	encoder.cpp
	encrypt.hpp
	encrypt.cpp
	hasher.hpp
	hasher.cpp
)

target_link_libraries(HsBaCipher PUBLIC HsBaSlicerBase
	OpenSSL::SSL OpenSSL::Crypto)