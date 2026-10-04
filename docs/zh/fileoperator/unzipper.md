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

## Bit7ZUnzipper（基于 7-Zip 的解压缩类，可选）

编译启用 `HSBA_USE_BIT7Z` 后，`Bit7ZUnzipper` 类（头文件 `fileoperator/bit7z_unzipper.hpp`）基于 bit7z（7-Zip）库提供解压能力，支持 ZIP、7Z、RAR 等归档格式。压缩 tar 归档——`.tar.gz` / `.tgz` / `.tar.xz` / `.txz`——可**一步直接解压**：根据扩展名识别内层 tar，透明地解压到临时文件并打开，`GetStream` 可直接访问 tar 内的真实文件。

```cpp
#include "fileoperator/bit7z_unzipper.hpp"

using namespace HsBa::Slicer;

auto unzipper = Bit7ZUnzipper::Create(HSBA_7Z_DLL);
unzipper->SetPassword("");                         // 可选，用于加密归档
unzipper->ReadFromFile("archive.tar.gz");          // 内层 tar 被透明展开
auto stream = unzipper->GetStream("document.txt"); // 直接读取 tar 内的真实文件
```

自由函数 `Bit7zExtract(archive, outdir, ...)` 与 `Bit7zExtract(archive, bufs, ...)`（头文件 `fileoperator/bit7z_zipper.hpp`）同样支持 `.tar.gz` / `.tar.xz` 路径，一次调用即可解压出内部全部文件。

### 压缩 tar 使用说明

- 识别基于扩展名；若文件并非真实的压缩 tar，会自动回退为按外层归档本身读取。
- 解压出的内层 tar 存于系统临时目录下的唯一临时文件，在重新打开其他归档或销毁对象时会自动删除。