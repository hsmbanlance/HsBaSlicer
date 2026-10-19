---
kind: external_dependency
name: OpenSSL 加密原语
slug: openssl
category: external_dependency
category_hints:
    - vendor_identity
    - auth_protocol
scope:
    - '**'
source_files:
    - cipher/encrypt.cpp
    - cipher/encrypt.hpp
---

### 身份与用途
- 密码学后端，由 vcpkg 提供 `openssl` 依赖，被 `cipher` 模块封装为 `HsBa::Slicer::Cipher::Encrypt`。
- 暴露三类算法：AES-256（CBC/ECB，口令派生或显式 IV）、3DES（DES-EDE3，CBC/ECB，口令派生或显式 IV）、RSA（PEM 密钥 + OAEP 填充，默认 2048 位）。所有接口返回原始字节数组，不直接暴露 OpenSSL handle。

### 稳定用法要点
- 参数存储加密走 AES-256-CBC，口令通过 PBKDF 派生（非明文直用）；RSA 仅用于 PEM 密钥对的生成/加解密，未参与 ParamStore 主流程。
- 对外只暴露无状态静态方法，调用方无需管理 EVP_CTX / RSA 对象生命周期。