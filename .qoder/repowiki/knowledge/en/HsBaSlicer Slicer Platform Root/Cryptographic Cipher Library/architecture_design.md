Built as a single static library `HsBaCipher` (CMakeLists.txt) with four orthogonal sub-components under namespace `HsBa::Slicer::Cipher`, each exposed as a stateless class:
- `encoder.{hpp,cpp}` — `Encoder` class wrapping Base64 and hex encode/decode.
- `hasher.{hpp,cpp}` — `Hasher` class exposing MD5, SHA1, SHA256 returning hex digests.
- `encrypt.{hpp,cpp}` — `Encrypt` class offering AES-256 (CBC/ECB), 3DES (CBC/ECB) with password-derived keys plus RSA PEM keypair generation and OAEP encrypt/decrypt.
- `LuaAdapter.{hpp,cpp}` — thin bridge that registers the three classes (`Encoder`, `Hasher`, `Encrypt`) into a `lua_State` via `RegisterLuaCipher`.

Dependency direction is one-way: `LuaAdapter` depends on the other three components; the other three depend only on OpenSSL (`OpenSSL::SSL`, `OpenSSL::Crypto`) and the shared base (`HsBaSlicerBase`). No inter-component coupling exists between encoder/hash/encrypt. All public APIs are static methods taking `std::vector<unsigned char>` / `std::string_view`, keeping the layer purely functional and side-effect free.