# Encoder (编码类)

Encoder 类提供了常见的编码转换功能，包括Base64和Hex编码解码功能。

## 功能特点

- 支持Base64编码和解码
- 支持十六进制编码和解码
- 支持字节数组和字符串的相互转换

## 方法说明

- `base64_encode` - 对数据进行Base64编码
- `base64_decode` - 对Base64字符串进行解码
- `base64_decode_to_string` - 将Base64字符串解码为字符串
- `hex_encode` - 对数据进行十六进制编码
- `hex_decode` - 对十六进制字符串进行解码
- `hex_decode_to_string` - 将十六进制字符串解码为字符串

## 使用示例

```cpp
#include "cipher/encoder.hpp"

using namespace HsBa::Slicer::Cipher;

// Base64编码示例
std::string text = "hello world";
std::string encoded = Encoder::base64_encode(text);
std::string decoded = Encoder::base64_decode_to_string(encoded);

// Hex编码示例
std::string data = "hello";
std::string hex_encoded = Encoder::hex_encode(data);
std::string hex_decoded = Encoder::hex_decode_to_string(hex_encoded);

// 字节数组编码示例
std::vector<unsigned char> byte_data = {/* 数据 */};
std::string b64_result = Encoder::base64_encode(byte_data);
```

## 注意事项

- Base64编码会增加约33%的数据大小
- Hex编码会增加100%的数据大小（每个字节编码为两个十六进制字符）
- 编码仅用于数据表示转换，不提供安全性
- 解码失败可能由无效输入导致