# Hasher (哈希类)

Hasher 类提供了多种哈希算法的实现，包括MD5、SHA1、SHA256等常用的哈希算法。

## 功能特点

- 支持多种哈希算法（MD5、SHA1、SHA256）
- 返回十六进制格式的哈希值
- 支持对字节数组和字符串进行哈希计算

## 方法说明

- `md5_hex` - 计算数据的MD5哈希值（返回十六进制字符串）
- `sha1_hex` - 计算数据的SHA1哈希值（返回十六进制字符串）
- `sha256_hex` - 计算数据的SHA256哈希值（返回十六进制字符串）

## 使用示例

```cpp
#include "cipher/hasher.hpp"

using namespace HsBa::Slicer::Cipher;

// 字符串哈希计算示例
std::string data = "hello world";
std::string md5_hash = Hasher::md5_hex(data);
std::string sha1_hash = Hasher::sha1_hex(data);
std::string sha256_hash = Hasher::sha256_hex(data);

// 字节数组哈希计算示例
std::vector<unsigned char> byte_data = {/* 数据 */};
std::string hash = Hasher::sha256_hex(byte_data);
```

## 注意事项

- MD5算法已被认为不够安全，不应用于安全敏感场景
- SHA256是目前推荐的安全哈希算法
- 哈希值可用于数据完整性校验
- 避免在安全应用中使用MD5或SHA1