# Hasher (Hash Class)

The Hasher class provides implementations of multiple hash algorithms, including commonly used hash algorithms such as MD5, SHA1, SHA256.

## Features

- Supports multiple hash algorithms (MD5, SHA1, SHA256)
- Returns hash values in hexadecimal format
- Supports hash computation for byte arrays and strings

## Method Description

- `md5_hex` - Computes MD5 hash of data (returns hexadecimal string)
- `sha1_hex` - Computes SHA1 hash of data (returns hexadecimal string)
- `sha256_hex` - Computes SHA256 hash of data (returns hexadecimal string)

## Usage Examples

```cpp
#include "cipher/hasher.hpp"

using namespace HsBa::Slicer::Cipher;

// String hash computation example
std::string data = "hello world";
std::string md5_hash = Hasher::md5_hex(data);
std::string sha1_hash = Hasher::sha1_hex(data);
std::string sha256_hash = Hasher::sha256_hex(data);

// Byte array hash computation example
std::vector<unsigned char> byte_data = {/* data */};
std::string hash = Hasher::sha256_hex(byte_data);
```

## Notes

- MD5 algorithm is considered insecure and should not be used in security-sensitive scenarios
- SHA256 is currently recommended as a secure hash algorithm
- Hash values can be used for data integrity verification
- Avoid using MD5 or SHA1 in security applications