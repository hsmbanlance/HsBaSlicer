# Encryption Module

<cite>
**Referenced Files in This Document**   
- [encrypt.cpp](file://cipher/encrypt.cpp)
- [encrypt.hpp](file://cipher/encrypt.hpp)
- [hasher.cpp](file://cipher/hasher.cpp)
- [hasher.hpp](file://cipher/hasher.hpp)
- [encoder.cpp](file://cipher/encoder.cpp)
- [encoder.hpp](file://cipher/encoder.hpp)
- [LuaAdapter.cpp](file://cipher/LuaAdapter.cpp)
- [LuaAdapter.hpp](file://cipher/LuaAdapter.hpp)
- [error.hpp](file://base/error.hpp)
</cite>

## Table of Contents
1. [Introduction](#introduction)
2. [Symmetric Encryption Implementation](#symmetric-encryption-implementation)
3. [Asymmetric Encryption with RSA-OAEP](#asymmetric-encryption-with-rsa-oaep)
4. [Key Derivation and IV Handling](#key-derivation-and-iv-handling)
5. [Error Handling and Security Practices](#error-handling-and-security-practices)
6. [Integration with System Components](#integration-with-system-components)
7. [Security Best Practices and Potential Vulnerabilities](#security-best-practices-and-potential-vulnerabilities)

## Introduction
The Encryption Module in HsBaSlicer provides comprehensive cryptographic functionality for securing sensitive data within the application. Built on OpenSSL, this module implements both symmetric and asymmetric encryption schemes to protect configuration files, model data, and other critical information. The implementation focuses on industry-standard algorithms including AES-256-CBC, AES-256-ECB, 3DES-ECB, 3DES-CBC, and RSA-OAEP, providing multiple options for different security requirements and performance considerations.

The module is designed with a clean, static interface through the `Encrypt` class, making cryptographic operations accessible throughout the application. It integrates seamlessly with other components through utility classes for encoding (Base64 and hex) and hashing (SHA-256), and provides Lua bindings for scripting access. The implementation follows security best practices while maintaining usability, with careful attention to error handling, memory management, and cryptographic hygiene.

**Section sources**
- [encrypt.hpp](file://cipher/encrypt.hpp#L1-L40)
- [encrypt.cpp](file://cipher/encrypt.cpp#L1-L557)

## Symmetric Encryption Implementation

### AES-256 Implementations
The module provides two AES-256 implementations: CBC (Cipher Block Chaining) and ECB (Electronic Codebook) modes. The AES-256-CBC implementation uses a 256-bit key with a 128-bit initialization vector (IV) to provide strong security through chaining, where each ciphertext block depends on all previous blocks. This prevents patterns from emerging in the encrypted data, making it more resistant to cryptanalysis.

The module offers both password-based encryption and IV-specified variants. The `aes256_cbc_encrypt` and `aes256_cbc_decrypt` methods derive both key and IV from a password using SHA-256, while the `_with_iv` variants allow explicit IV specification, providing greater control for advanced use cases. The ECB implementation, while less secure due to its deterministic nature, is provided for compatibility with legacy systems or specific use cases where pattern preservation is required.

```mermaid
flowchart TD
Start["AES-256 Encryption Process"] --> DeriveKey["Derive 32-byte key and 16-byte IV<br/>from password using SHA-256"]
DeriveKey --> CreateContext["Create EVP_CIPHER_CTX"]
CreateContext --> InitEncrypt["Initialize Encryption Context<br/>EVP_aes_256_cbc()"]
InitEncrypt --> Update["Process Plaintext Blocks<br/>EVP_EncryptUpdate()"]
Update --> Finalize["Finalize Encryption<br/>EVP_EncryptFinal_ex()"]
Finalize --> Cleanup["Free Context Memory"]
Cleanup --> Return["Return Ciphertext"]
```

**Diagram sources**
- [encrypt.cpp](file://cipher/encrypt.cpp#L52-L85)
- [encrypt.cpp](file://cipher/encrypt.cpp#L198-L235)

**Section sources**
- [encrypt.hpp](file://cipher/encrypt.hpp#L11-L23)
- [encrypt.cpp](file://cipher/encrypt.cpp#L52-L235)

### 3DES Implementations
The module implements Triple DES (3DES) encryption with both ECB and CBC modes, providing compatibility with systems requiring this legacy algorithm. The 3DES-ECB implementation uses a 192-bit key (24 bytes) derived from the password, while the CBC variant requires an 8-byte IV. The implementation uses the OpenSSL EVP_des_ede3_ecb() and EVP_des_ede3_cbc() cipher methods, which apply the DES algorithm three times with three different keys (EDE: Encrypt-Decrypt-Encrypt).

The 3DES-CBC implementation with explicit IV (`des3_cbc_encrypt_with_iv` and `des3_cbc_decrypt_with_iv`) follows the same pattern as the AES-256-CBC implementation but with the appropriate key and IV sizes for 3DES. This provides a consistent API across different symmetric algorithms while respecting their specific requirements.

```mermaid
classDiagram
class Encrypt {
+aes256_cbc_encrypt(plaintext, password)
+aes256_cbc_decrypt(cipher, password)
+aes256_ecb_encrypt(plaintext, password)
+aes256_ecb_decrypt(cipher, password)
+aes256_cbc_encrypt_with_iv(plaintext, password, iv)
+aes256_cbc_decrypt_with_iv(cipher, password, iv)
+des3_ecb_encrypt(plaintext, password)
+des3_ecb_decrypt(cipher, password)
+des3_cbc_encrypt_with_iv(plaintext, password, iv)
+des3_cbc_decrypt_with_iv(cipher, password, iv)
}
class Hasher {
+sha256_hex(data)
+md5_hex(data)
+sha1_hex(data)
}
class Encoder {
+base64_encode(data)
+base64_decode(b64)
+hex_encode(data)
+hex_decode(hex)
}
Encrypt --> Hasher : "uses for key derivation"
Encrypt --> Encoder : "uses for encoding/decoding"
```

**Diagram sources**
- [encrypt.hpp](file://cipher/encrypt.hpp#L8-L37)
- [hasher.hpp](file://cipher/hasher.hpp#L9-L24)
- [encoder.hpp](file://cipher/encoder.hpp#L9-L32)

**Section sources**
- [encrypt.hpp](file://cipher/encrypt.hpp#L25-L29)
- [encrypt.cpp](file://cipher/encrypt.cpp#L277-L388)

## Asymmetric Encryption with RSA-OAEP

### RSA-OAEP Public/Private Key Encryption
The module implements RSA-OAEP (Optimal Asymmetric Encryption Padding) encryption, providing secure asymmetric cryptography for scenarios where key exchange or digital signatures are required. The `rsa_public_encrypt_pem` method accepts a public key in PEM format and plaintext data, encrypting it using RSA with OAEP padding. The corresponding `rsa_private_decrypt_pem` method decrypts ciphertext using a private key in PEM format.

The implementation uses OpenSSL's EVP_PKEY interface, initializing the context with `EVP_PKEY_encrypt_init` and setting the padding scheme to `RSA_PKCS1_OAEP_PADDING`. This provides resistance against chosen ciphertext attacks and ensures semantic security. The method handles the two-phase encryption process: first querying the output buffer size, then performing the actual encryption.

```mermaid
sequenceDiagram
participant Application
participant Encrypt
participant OpenSSL
Application->>Encrypt : rsa_public_encrypt_pem(public_pem, plaintext)
Encrypt->>OpenSSL : BIO_new_mem_buf(public_pem)
OpenSSL-->>Encrypt : BIO object
Encrypt->>OpenSSL : PEM_read_bio_PUBKEY(BIO)
OpenSSL-->>Encrypt : EVP_PKEY object
Encrypt->>OpenSSL : EVP_PKEY_CTX_new(pkey)
OpenSSL-->>Encrypt : EVP_PKEY_CTX object
Encrypt->>OpenSSL : EVP_PKEY_encrypt_init(ctx)
Encrypt->>OpenSSL : EVP_PKEY_CTX_set_rsa_padding(ctx, OAEP)
Encrypt->>OpenSSL : EVP_PKEY_encrypt(ctx, nullptr, &outlen, plaintext)
OpenSSL-->>Encrypt : Required output length
Encrypt->>OpenSSL : EVP_PKEY_encrypt(ctx, out, &outlen, plaintext)
OpenSSL-->>Encrypt : Encrypted data
Encrypt->>Application : Return ciphertext vector
```

**Diagram sources**
- [encrypt.cpp](file://cipher/encrypt.cpp#L429-L476)
- [encrypt.cpp](file://cipher/encrypt.cpp#L508-L555)

### RSA Key Pair Generation
The module provides RSA key pair generation functionality through the `rsa_generate_keypair_pem` method, which creates a new RSA key pair of specified bit length (default 2048 bits) and returns both public and private keys in PEM format. This implementation uses OpenSSL's EVP_PKEY_CTX interface to generate the key pair securely.

The process involves creating a context for RSA key generation, initializing the key generation, setting the desired key length, and then generating the key. The resulting EVP_PKEY object is then written to memory BIOs (Buffered I/O) in PEM format for both the private and public components. The private key is returned in PKCS#8 format with "-----BEGIN PRIVATE KEY-----" headers, while the public key uses the standard X.509 format with "-----BEGIN PUBLIC KEY-----" headers.

```mermaid
flowchart TD
Start["RSA Key Pair Generation"] --> CreateContext["Create EVP_PKEY_CTX<br/>for RSA"]
CreateContext --> InitKeygen["Initialize Key Generation"]
InitKeygen --> SetBits["Set Key Length<br/>(e.g., 2048 bits)"]
SetBits --> GenerateKey["Generate Key Pair"]
GenerateKey --> CreateBIOs["Create Memory BIOs<br/>(private and public)"]
CreateBIOs --> WritePrivate["Write Private Key<br/>to BIO in PEM format"]
CreateBIOs --> WritePublic["Write Public Key<br/>to BIO in PEM format"]
WritePrivate --> ExtractPrivate["Extract Private Key PEM<br/>from BIO buffer"]
WritePublic --> ExtractPublic["Extract Public Key PEM<br/>from BIO buffer"]
ExtractPrivate --> Cleanup["Free BIO and PKEY resources"]
ExtractPublic --> Cleanup
Cleanup --> Return["Return {public_pem, private_pem}"]
```

**Diagram sources**
- [encrypt.cpp](file://cipher/encrypt.cpp#L478-L506)

**Section sources**
- [encrypt.hpp](file://cipher/encrypt.hpp#L35-L36)
- [encrypt.cpp](file://cipher/encrypt.cpp#L478-L506)

## Key Derivation and IV Handling

### Password-Based Key Derivation
The module implements a simple but effective key derivation scheme using SHA-256 hashing. For AES-256 encryption, the `derive_key_iv` function computes the SHA-256 hash of the provided password and uses the first 32 bytes as the encryption key and the first 16 bytes as the initialization vector. For 3DES encryption, the `derive_3des_key_iv` function uses the first 24 bytes of the SHA-256 hash as the key and the first 8 bytes as the IV.

This approach provides a deterministic way to derive cryptographic keys from passwords while ensuring sufficient key entropy. The use of SHA-256 ensures that even short passwords are expanded to the required key length, though the security ultimately depends on the strength of the original password. The implementation does not use salt or iteration counts, which means it is vulnerable to rainbow table attacks if weak passwords are used.

```mermaid
flowchart LR
Password["User Password"] --> SHA256["SHA-256 Hash<br/>(32 bytes)"]
SHA256 --> Key["First 32 bytes = AES Key"]
SHA256 --> IV["First 16 bytes = IV"]
Key --> AES["AES-256 Encryption"]
IV --> AES
```

**Diagram sources**
- [encrypt.cpp](file://cipher/encrypt.cpp#L19-L29)
- [encrypt.cpp](file://cipher/encrypt.cpp#L31-L40)

### Initialization Vector Management
The module handles initialization vectors (IVs) differently depending on the encryption mode and method. For CBC modes, IVs are essential for security, ensuring that identical plaintext blocks encrypt to different ciphertext blocks. The module provides two approaches: automatic IV derivation from the password and explicit IV specification.

When using the password-based methods (without IV parameters), the IV is derived from the same SHA-256 hash as the key, which means the IV is deterministic for a given password. This is less secure than using a random IV but simplifies the API by not requiring IV management. When using the `_with_iv` methods, the caller must provide a cryptographically random IV of the appropriate length (16 bytes for AES, 8 bytes for 3DES), which provides better security but requires more complex key management.

**Section sources**
- [encrypt.cpp](file://cipher/encrypt.cpp#L19-L40)
- [encrypt.cpp](file://cipher/encrypt.cpp#L200-L205)
- [encrypt.cpp](file://cipher/encrypt.cpp#L353-L358)

## Error Handling and Security Practices

### OpenSSL Error Handling
The module implements comprehensive error handling for OpenSSL operations, ensuring that cryptographic failures are properly reported and resources are cleaned up. A helper function `openssl_err` captures OpenSSL error codes using `ERR_get_error` and converts them to human-readable strings with `ERR_error_string_n`. This allows for detailed error reporting when cryptographic operations fail.

All cryptographic operations follow a consistent pattern: allocate resources, perform the operation, check for errors, clean up resources on failure, and return results on success. The implementation uses RAII-like patterns with manual cleanup, ensuring that `EVP_CIPHER_CTX`, `EVP_PKEY`, `BIO`, and other OpenSSL objects are properly freed even when errors occur. This prevents memory leaks and resource exhaustion.

```mermaid
flowchart TD
Start["Cryptographic Operation"] --> Allocate["Allocate OpenSSL Context"]
Allocate --> CheckAlloc["Check Allocation Success"]
CheckAlloc --> |Failure| HandleAllocError["Throw RuntimeError"]
CheckAlloc --> |Success| Initialize["Initialize Operation"]
Initialize --> CheckInit["Check Initialization Success"]
CheckInit --> |Failure| CleanupInit["Free Context<br/>Throw RuntimeError"]
CheckInit --> |Success| Process["Process Data"]
Process --> CheckProcess["Check Processing Success"]
CheckProcess --> |Failure| CleanupProcess["Free Context<br/>Throw RuntimeError"]
CheckProcess --> |Success| Finalize["Finalize Operation"]
Finalize --> CheckFinal["Check Finalization Success"]
CheckFinal --> |Failure| CleanupFinal["Free Context<br/>Throw RuntimeError"]
CheckFinal --> |Success| CleanupSuccess["Free Context"]
CleanupSuccess --> Return["Return Result"]
```

**Diagram sources**
- [encrypt.cpp](file://cipher/encrypt.cpp#L42-L49)
- [encrypt.cpp](file://cipher/encrypt.cpp#L58-L84)

### Secure Memory Practices
The module follows secure memory practices by using appropriate data types and ensuring proper cleanup of sensitive information. Cryptographic keys and IVs are stored in fixed-size arrays (`unsigned char key[32]`, `unsigned char iv[16]`) rather than in objects that might be moved or copied by the compiler. The implementation ensures that these sensitive buffers are properly initialized and that OpenSSL contexts are freed immediately after use.

Error messages are carefully crafted to avoid leaking sensitive information. While the `openssl_err` function includes OpenSSL error details, the higher-level error messages focus on the nature of the failure (e.g., "EVP_EncryptInit_ex failed") rather than exposing internal cryptographic details that could aid attackers. The use of standard exception types like `RuntimeError` from the base error module ensures consistent error handling across the application.

**Section sources**
- [encrypt.cpp](file://cipher/encrypt.cpp#L42-L49)
- [encrypt.cpp](file://cipher/encrypt.cpp#L58-L84)
- [encrypt.cpp](file://cipher/encrypt.cpp#L430-L438)
- [error.hpp](file://base/error.hpp#L12-L19)

## Integration with System Components

### Encoder and Hasher Utilities
The encryption module integrates closely with the `Encoder` and `Hasher` classes to provide complete cryptographic functionality. The `Encoder` class provides Base64 and hexadecimal encoding/decoding, which is essential for representing binary ciphertext in text formats. The `Hasher` class provides SHA-256 hashing, which is used for key derivation from passwords.

These utility classes follow a similar static interface pattern, making them easy to use throughout the application. The `Encoder` class provides both vector-based and string-based methods, with inline wrappers that convert string views to byte vectors automatically. This simplifies usage while maintaining type safety and performance.

**Section sources**
- [encoder.hpp](file://cipher/encoder.hpp#L9-L32)
- [hasher.hpp](file://cipher/hasher.hpp#L9-L24)
- [encrypt.cpp](file://cipher/encrypt.cpp#L22-L23)
- [encrypt.cpp](file://cipher/encrypt.cpp#L33-L34)

### Lua Integration
The module provides Lua bindings through the `LuaAdapter.cpp` file, exposing encoding functionality to Lua scripts. The `RegisterLuaCipher` function creates a Lua library with functions for Base64 and hexadecimal encoding/decoding. This allows Lua scripts to handle encoded data when interacting with encrypted content.

The Lua adapter follows the standard Lua C API pattern, with wrapper functions that convert between Lua strings and C++ byte vectors. Error handling is implemented using `lua_error`, ensuring that cryptographic failures are properly reported to Lua scripts. This integration enables secure data handling in Lua scripts, such as encoding configuration data before encryption or decoding encrypted payloads.

```mermaid
graph TB
subgraph "Lua Environment"
LuaScript["Lua Script"]
LuaState["Lua State"]
end
subgraph "C++ Implementation"
LuaAdapter["LuaAdapter.cpp"]
Encoder["Encoder Class"]
end
LuaScript --> LuaState
LuaState --> LuaAdapter
LuaAdapter --> Encoder
Encoder --> OpenSSL
```

**Diagram sources**
- [LuaAdapter.cpp](file://cipher/LuaAdapter.cpp#L1-L95)
- [encoder.cpp](file://cipher/encoder.cpp#L25-L107)

**Section sources**
- [LuaAdapter.cpp](file://cipher/LuaAdapter.cpp#L1-L95)
- [LuaAdapter.hpp](file://cipher/LuaAdapter.hpp#L1-L12)

## Security Best Practices and Potential Vulnerabilities

### Security Best Practices Followed
The implementation follows several important security best practices:
- **Use of Established Cryptographic Libraries**: The module relies on OpenSSL, a well-audited and widely-used cryptographic library, rather than implementing cryptographic primitives from scratch.
- **Proper Error Handling**: Comprehensive error checking ensures that cryptographic operations either succeed completely or fail safely, with proper resource cleanup.
- **Memory Safety**: The use of `std::vector<unsigned char>` for data buffers and proper cleanup of OpenSSL objects prevents memory leaks and buffer overflows.
- **Standard Padding Schemes**: The use of OAEP padding for RSA encryption provides resistance against chosen ciphertext attacks.
- **Clear API Design**: The static method interface makes cryptographic operations easy to use correctly and difficult to misuse.

### Potential Vulnerabilities and Limitations
Despite following best practices, the implementation has some potential vulnerabilities and limitations:
- **Weak Password Usage**: The key derivation scheme is vulnerable to brute force and dictionary attacks if users choose weak passwords, as there is no salt or key stretching (e.g., PBKDF2).
- **Deterministic IVs**: When using password-based encryption without explicit IVs, the IV is derived from the password, making it deterministic. This could potentially leak information if the same password is used to encrypt multiple messages.
- **Lack of Authentication**: The symmetric encryption modes do not provide message authentication, making them vulnerable to tampering. An attacker could modify ciphertext in ways that produce predictable changes to the plaintext.
- **No Perfect Forward Secrecy**: The RSA key generation and usage do not implement forward secrecy, meaning that compromise of a private key could allow decryption of all previously encrypted messages.
- **Limited Algorithm Agility**: The API is tightly coupled to specific algorithms (AES-256, 3DES, RSA), making it difficult to upgrade to newer algorithms in the future.

These limitations should be considered when using the module for high-security applications. For maximum security, it is recommended to use strong, randomly generated passwords or keys, use explicit random IVs for CBC modes, and combine encryption with separate message authentication when integrity is required.

**Section sources**
- [encrypt.cpp](file://cipher/encrypt.cpp#L19-L40)
- [encrypt.cpp](file://cipher/encrypt.cpp#L200-L205)
- [encrypt.cpp](file://cipher/encrypt.cpp#L452-L456)
- [encrypt.cpp](file://cipher/encrypt.cpp#L531-L535)