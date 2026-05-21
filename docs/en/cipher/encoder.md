# Encoder (Encoding Class)

The Encoder class provides common encoding conversion functions, including Base64 and Hex encoding/decoding functions.

## Features

- Supports Base64 encoding and decoding
- Supports hexadecimal encoding and decoding
- Supports mutual conversion between byte arrays and strings

## Method Description

- `base64_encode` - Performs Base64 encoding of data
- `base64_decode` - Decodes Base64 string
- `base64_decode_to_string` - Decodes Base64 string to string
- `hex_encode` - Performs hexadecimal encoding of data
- `hex_decode` - Decodes hexadecimal string
- `hex_decode_to_string` - Decodes hexadecimal string to string

## Usage Examples

```cpp
#include "cipher/encoder.hpp"

using namespace HsBa::Slicer::Cipher;

// Base64 encoding example
std::string text = "hello world";
std::string encoded = Encoder::base64_encode(text);
std::string decoded = Encoder::base64_decode_to_string(encoded);

// Hex encoding example
std::string data = "hello";
std::string hex_encoded = Encoder::hex_encode(data);
std::string hex_decoded = Encoder::hex_decode_to_string(hex_encoded);

// Byte array encoding example
std::vector<unsigned char> byte_data = {/* data */};
std::string b64_result = Encoder::base64_encode(byte_data);
```

## Notes

- Base64 encoding increases data size by approximately 33%
- Hex encoding doubles data size (each byte encoded as two hexadecimal characters)
- Encoding is only for data representation conversion, does not provide security
- Decode failure may be caused by invalid input