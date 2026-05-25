# Encrypt (Encryption Class)

The Encrypt class provides implementations of multiple encryption algorithms, including AES, 3DES, RSA and other encryption algorithms.

## Features

- Supports multiple encryption algorithms (AES, 3DES, RSA)
- Supports different encryption modes (CBC, ECB, etc.)
- Provides key pair generation functionality
- Supports encryption modes with IV

## Method Description

### AES Encryption Algorithms
- `aes256_cbc_encrypt` - Encrypts plaintext using AES-256-CBC mode
- `aes256_cbc_decrypt` - Decrypts ciphertext using AES-256-CBC mode
- `aes256_ecb_encrypt` - Encrypts plaintext using AES-256-ECB mode
- `aes256_ecb_decrypt` - Decrypts ciphertext using AES-256-ECB mode
- `aes256_cbc_encrypt_with_iv` - Encrypts using AES-256-CBC mode with IV
- `aes256_cbc_decrypt_with_iv` - Decrypts using AES-256-CBC mode with IV

### 3DES Encryption Algorithms
- `des3_ecb_encrypt` - Encrypts plaintext using 3DES-ECB mode
- `des3_ecb_decrypt` - Decrypts ciphertext using 3DES-ECB mode
- `des3_cbc_encrypt_with_iv` - Encrypts using 3DES-CBC mode with IV
- `des3_cbc_decrypt_with_iv` - Decrypts using 3DES-CBC mode with IV

### RSA Encryption Algorithms
- `rsa_public_encrypt_pem` - RSA encryption using PEM format public key (OAEP padding)
- `rsa_private_decrypt_pem` - RSA decryption using PEM format private key (OAEP padding)
- `rsa_generate_keypair_pem` - Generates RSA key pair (returns {public_pem, private_pem})

## Usage Examples

```cpp
#include "cipher/encrypt.hpp"

using namespace HsBa::Slicer::Cipher;

// AES encryption example
std::vector<unsigned char> plaintext = {/* original data */};
std::string password = "my_password";
auto encrypted = Encrypt::aes256_cbc_encrypt(plaintext, password);
auto decrypted = Encrypt::aes256_cbc_decrypt(encrypted, password);

// RSA key generation example
auto [public_key, private_key] = Encrypt::rsa_generate_keypair_pem(2048);
```

## Notes

- AES encryption is recommended to use CBC mode for better security
- RSA key length should be at least 2048 bits
- Sensitive data should be cleared from memory after processing
- Passwords should use sufficiently strong random passwords