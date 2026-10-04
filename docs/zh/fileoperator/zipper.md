# Zipper (压缩类)

Zipper 类使用miniz库实现了ZIP格式的压缩功能，支持多种压缩级别和文件添加方式。

## 功能特点

- 基于miniz库的轻量级压缩实现
- 支持多种压缩级别（无压缩、快速压缩、紧密压缩等）
- 支持将字节数组和文件添加到压缩包
- 提供事件回调机制，可监控压缩进度
- 支持重复文件名处理

## 方法说明

### 构造函数
- `Zipper()` - 默认构造函数
- `Zipper(MinizCompression compression)` - 指定压缩级别的构造函数

### 文件添加功能
- `AddByteFile` - 将字节数组作为文件添加到压缩包
- `AddFile` - 将指定路径的文件添加到压缩包
- `AddByteFileIgnoreDuplicate` - 添加字节数组文件（忽略重复文件名）
- `AddFileIgnoreDuplicate` - 添加文件（忽略重复文件名）

### 压缩保存功能
- `Save` - 将压缩内容保存到指定文件路径

### 压缩级别枚举
- `MinizCompression::Undefine` - 未定义
- `MinizCompression::No` - 无压缩
- `MinizCompression::Fast` - 快速压缩
- `MinizCompression::Tight` - 紧密压缩
- `MinizCompression::Unknown` - 未知

## 使用示例

```cpp
#include "fileoperator/zipper.hpp"

using namespace HsBa::Slicer;

// 创建压缩包示例
Zipper zipper(MinizCompression::Tight);
zipper.AddFile("document.txt", "/path/to/document.txt");
zipper.AddByteFile("config.json", R"({"setting": "value"})");
zipper.Save("archive.zip");

// 事件监听示例
zipper.Subscribe([](double progress, std::string_view filename) {
    std::cout << "Progress: " << progress << ", File: " << filename << std::endl;
});
```

## 注意事项

- 压缩级别选择需要在压缩率和处理速度之间权衡
- 处理大文件时应注意内存使用情况
- 使用完毕后应及时释放资源，避免内存泄漏
- 在多线程环境中使用时需要注意线程安全性

## Bit7zZipper（基于 7-Zip 的压缩类，可选）

编译启用 `HSBA_USE_BIT7Z` 后，`Bit7zZipper` 类（头文件 `fileoperator/bit7z_zipper.hpp`）基于 bit7z（7-Zip）库提供多格式压缩，格式由 `ZipperFormat` 定义：

| 格式 | 输出 | 说明 |
|------|------|------|
| `Zip` / `SevenZip` | `.zip` / `.7z` | 支持密码保护 |
| `XZ` / `BZIP2` / `GZIP` / `TAR` | `.xz` / `.bz2` / `.gz` / `.tar` | 单层格式 |
| `TarGz` | `.tar.gz` / `.tgz` | 直接两段式压缩：内存中先打包为 tar，再经 gzip 压缩，一次 `Save` 即生成 `.tar.gz` |
| `TarXz` | `.tar.xz` / `.txz` | 直接两段式压缩：内存中先打包为 tar，再经 xz 压缩，一次 `Save` 即生成 `.tar.xz` |

```cpp
#include "fileoperator/bit7z_zipper.hpp"

using namespace HsBa::Slicer;

// 直接创建 .tar.gz 压缩包（ZipperFormat::TarXz 用法相同）
Bit7zZipper zipper{HSBA_7Z_DLL, ZipperFormat::TarGz, ""};
zipper.AddFile("document.txt", "/path/to/document.txt");
zipper.AddByteFile("config.json", R"({"setting": "value"})");
zipper.Save("archive.tar.gz");
```

在 Lua 脚本中，可通过 `Bit7zZipper.new("TarGz", dll_path)` 使用同样的格式（参见 Lua 流水线 API 文档）。