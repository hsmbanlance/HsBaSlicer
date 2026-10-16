The module is a static CMake library `HsBaCipher` composed of three orthogonal utility classes plus a Lua bridge:
- `Encoder` (encoder.{hpp,cpp}): stateless Base64 and hex encode/decode helpers.
- `Hasher` (hasher.{hpp,cpp}): MD5, SHA1, SHA256 returning hex digests.
- `Encrypt` (encrypt.{hpp,cpp}): AES-256 (CBC/ECB), 3DES (ECB/CBC) password-based encryption and RSA PEM keypair generation + OAEP encrypt/decrypt.
- `LuaAdapter` (LuaAdapter.{hpp,cpp}): exposes the above three classes as a Lua table via `RegisterLuaCipher(lua_State*)`.

All public APIs live in the `HsBa::Slicer::Cipher` namespace. The three core classes are purely stateless static-method wrappers around OpenSSL; they depend only on `HsBaSlicerBase`, `OpenSSL::SSL`, and `OpenSSL::Crypto`. `LuaAdapter` is the only file that depends on the Lua C API (`lua.hpp`) and pulls in the other three classes — dependency direction is one-way: LuaAdapter → {Encoder, Hasher, Encrypt}, never the reverse. Build configuration is declared in `cipher/CMakeLists.txt`, which links against OpenSSL and finds Lua via `find_package(Lua REQUIRED)`.