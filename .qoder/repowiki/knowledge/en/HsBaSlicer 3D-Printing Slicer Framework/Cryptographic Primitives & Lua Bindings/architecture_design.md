The module is a static CMake library (`HsBaCipher`) composed of four orthogonal, stateless utility classes under `HsBa::Slicer::Cipher` plus a thin Lua adapter:
- `Encoder` (encoder.{hpp,cpp}): Base64 and hex encode/decode helpers.
- `Hasher` (hasher.{hpp,cpp}): MD5, SHA1, SHA256 returning hex digests.
- `Encrypt` (encrypt.{hpp,cpp}): Password-derived AES-256 (CBC/ECB), 3DES (CBC/ECB), and RSA OAEP PEM keypair generation + encrypt/decrypt.
- `LuaAdapter` (LuaAdapter.{hpp,cpp}): Registers `Encoder`, `Hasher`, `Encrypt` into a `lua_State` so scripts can call them.

Dependency direction is one-way: `LuaAdapter` depends on the three utility classes; the utilities depend only on OpenSSL (`OpenSSL::SSL`, `OpenSSL::Crypto`) and the shared `HsBaSlicerBase` target. There is no internal sub-package hierarchy — each class lives in its own header/source pair, and `CMakeLists.txt` explicitly enumerates all sources for the static archive.