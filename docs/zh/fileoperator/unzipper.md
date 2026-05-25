# Unzipper (解压缩类)

Unzipper 类使用miniz库实现了ZIP格式的解压缩功能，支持从文件读取和流式访问。

## 功能特点

- 基于miniz库的轻量级解压缩实现
- 支持从文件直接读取压缩包内容
- 提供流式访问解压后的文件
- 支持内存缓存和临时文件缓存
- 可配置最大内存使用量

## 方法说明

### 实例创建
- `Create()` - 创建Unzipper实例的静态方法

### 解压缩功能
- `ReadFromFileImpl` - 从文件读取压缩包内容
- `GetStreamImpl` - 获取指定文件的流对象

### 内存管理
- `SetMaxMemSize` - 设置最大内存使用量（默认1GB）
- `max_mem_size_` - 静态成员变量，控制内存使用上限

## 使用示例

```cpp
#include "fileoperator/unzipper.hpp"

using namespace HsBa::Slicer;

// 解压缩示例
auto unzipper = Unzipper::Create();
unzipper->ReadFromFileImpl("archive.zip", false);

// 获取解压后的文件流
auto stream = unzipper->GetStreamImpl("document.txt");

// 设置最大内存使用量
Unzipper::SetMaxMemSize(512 * 1024 * 1024); // 512MB
```

## 注意事项

- 解压缩时应验证文件完整性，防止恶意文件攻击
- 处理大文件时应注意内存使用情况，合理设置缓存策略
- 使用完毕后应及时释放资源，避免内存泄漏
- 在多线程环境中使用时需要注意线程安全性