Built as the `HsBaCipher` static library (`CMakeLists.txt`) with three orthogonal sub-features under namespace `HsBa::Slicer::Cipher`:
- `encoder.{hpp,cpp}` — stateless `Encoder` class offering Base64 and hex encode/decode over `std::vector<unsigned char>` / `std::string_view`.
- `hasher.{hpp,cpp}` — stateless `Hasher` class wrapping OpenSSL MD5/SHA1/SHA256 into hex digests.
- `encrypt.{hpp,cpp}` — stateless `Encrypt` class wrapping OpenSSL AES-256 (CBC/ECB), 3DES (CBC/ECB) and RSA (OAEP, PEM I/O, keygen).
- `LuaAdapter.{hpp,cpp}` — thin registration layer that exposes Encoder/Hasher/Encrypt to Lua via `RegisterLuaCipher(lua_State*)`.

Dependency direction is one-way: `LuaAdapter` depends on the other three headers; the crypto primitives depend only on OpenSSL (`OpenSSL::SSL`, `OpenSSL::Crypto`) and `HsBaSlicerBase`. There are no internal sub-packages or runtime state — every operation is invoked through static methods taking raw bytes and returning new buffers.