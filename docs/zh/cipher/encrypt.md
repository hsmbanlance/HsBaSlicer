# Encrypt (加密类)

Encrypt 类提供了多种加密算法的实现，包括AES、3DES、RSA等加密算法。

## 功能特点

- 支持多种加密算法（AES、3DES、RSA）
- 支持不同的加密模式（CBC、ECB等）
- 提供密钥对生成功能
- 支持带IV的加密模式

## 方法说明

### AES 加密算法
- `aes256_cbc_encrypt` - 使用AES-256-CBC模式加密明文
- `aes256_cbc_decrypt` - 使用AES-256-CBC模式解密密文
- `aes256_ecb_encrypt` - 使用AES-256-ECB模式加密明文
- `aes256_ecb_decrypt` - 使用AES-256-ECB模式解密密文
- `aes256_cbc_encrypt_with_iv` - 使用带IV的AES-256-CBC模式加密
- `aes256_cbc_decrypt_with_iv` - 使用带IV的AES-256-CBC模式解密

### 3DES 加密算法
- `des3_ecb_encrypt` - 使用3DES-ECB模式加密明文
- `des3_ecb_decrypt` - 使用3DES-ECB模式解密密文
- `des3_cbc_encrypt_with_iv` - 使用带IV的3DES-CBC模式加密
- `des3_cbc_decrypt_with_iv` - 使用带IV的3DES-CBC模式解密

### RSA 加密算法
- `rsa_public_encrypt_pem` - 使用PEM格式公钥进行RSA加密（OAEP填充）
- `rsa_private_decrypt_pem` - 使用PEM格式私钥进行RSA解密（OAEP填充）
- `rsa_generate_keypair_pem` - 生成RSA密钥对（返回{公钥PEM, 私钥PEM}）

## 使用示例

```cpp
#include "cipher/encrypt.hpp"

using namespace HsBa::Slicer::Cipher;

// AES加密示例
std::vector<unsigned char> plaintext = {/* 原文数据 */};
std::string password = "my_password";
auto encrypted = Encrypt::aes256_cbc_encrypt(plaintext, password);
auto decrypted = Encrypt::aes256_cbc_decrypt(encrypted, password);

// RSA密钥生成示例
auto [public_key, private_key] = Encrypt::rsa_generate_keypair_pem(2048);
```

## 注意事项

- AES加密推荐使用CBC模式以获得更好的安全性
- RSA密钥长度建议至少使用2048位
- 敏感数据处理完成后应及时清理内存
- 密码应使用足够强度的随机密码