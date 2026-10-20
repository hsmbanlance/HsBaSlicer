# 文件路径编码与非 ASCII 字符支持

本指南说明 HsBaSlicer 在**读取模型文件**与**保存结果文件**时如何处理包含非
ASCII 字符（中日韩文字、重音字母、emoji、空格）的路径，以及如何在跨平台条件下
保持该支持正确无误。

## 1. 核心约定：内部路径一律为 UTF-8

HsBaSlicer 内部所有文件路径都以 **UTF-8 `std::string`** 形式传递。公共 C ABI
同样遵循此约定（`DllHsBaSlicer` 中模型加载入口的 `file_path` 参数文档明确标注为
"Model file path (UTF-8)"）。

问题只出现在**边界**处——UTF-8 字符串遇到期望*另一种*编码的平台 API：

- 在 Windows 上，传给 `fopen` / `CreateFileA` 的窄 `char*` 路径会按**进程 ANSI
  代码页**（`GetACP()`）解释。旧代码页（如 936/GBK、1252）无法表示所有 Unicode
  名称。
- Windows 上 `std::filesystem::path` 的 native 表示是**宽字符**（`std::wstring`）。
  若直接用 UTF-8 `std::string` 构造（`std::filesystem::path(utf8Str)`），字节会被
  按 ANSI 代码页重新解析，从而**破坏**代码页之外的字符。

## 2. 转换工具（`base/encoding_convert.hpp`）

```cpp
namespace HsBa::Slicer {
std::string           utf8_to_local(const std::string& str);   // UTF-8 -> 本地(ANSI)字节
std::string           local_to_utf8(const std::string& str);   // 本地(ANSI)字节 -> UTF-8
std::filesystem::path utf8_to_path(const std::string& utf8Path);// UTF-8 -> native 宽路径
std::string           path_to_utf8(const std::filesystem::path& path); // native 路径 -> UTF-8
}
```

- `utf8_to_path` 在 Windows 上把 UTF-8 解码为宽字符（UTF-16）表示，结果与 ANSI
  代码页无关，可携带任意 Unicode 名称；在 POSIX/Android/iOS 上是直通（native 编码
  本身即 UTF-8）。
- `path_to_utf8` 是其逆操作；非 Windows 上等价于 `path.string()`。
- `utf8_to_local` / `local_to_utf8` 为*只能*接收窄 `char*` 的 API 转换到本地窄编码。
  当进程代码页已是 UTF-8（见 §4）时，它们变为直通。

## 3. 按 API 类型选择正确的转换

依据*被调用方*期望的编码来选择转换——切勿叠加两次转换，也切勿做 `path.string()`
往返：

| 目标 API 类型                              | 示例                                     | 应使用                                 |
|-------------------------------------------|------------------------------------------|----------------------------------------|
| 宽字符 / `std::filesystem` native         | `std::ofstream(fs::path)`、`std::ifstream`、`std::filesystem::*` | `utf8_to_path(utf8Str)` → 传入 `fs::path` |
| 期望 UTF-8 的窄 API                        | `sqlite3_open`、`Zipper::Save`、Lua `output_path` | `path_to_utf8(path)`（或原始 UTF-8 字符串） |
| 按 ANSI 代码页解释的窄 `char*`             | libigl/CGAL/OpenCASCADE/OpenVDB/miniz（`fopen`）    | `utf8_to_local(utf8Str)`               |

**应避免的反模式：** `std::filesystem::path(utf8Str).string()` —— 它把 UTF-8 字节
按 ANSI 解析（Windows），再把 ANSI 字符串交给期望 UTF-8 的 API，造成双重破坏。ZIP
打包写入方（`Zipper`、`Bit7zZipper`）以及 `ImagesPath`/`LayersPath` 结果保存方以前
正是这么做的，现已更正为按需单次使用 `path_to_utf8` / `utf8_to_local`。

该规则已应用于：

- **模型读取**：`IglModel`、`CgalModel`、`OcctModel`、`OpenVdbModel`（窄后端经
  `utf8_to_local`；OpenVDB 的 XYZ 读写现改用宽 `std::filesystem::path` 打开）。
- **结果保存**：`Zipper` / `Bit7zZipper`（归档路径）、`ImagesPath`（ZIP + Lua）、
  `LayersPath`（SQLite + Lua），以及 `LibHsBaSlicer` 中通过 `utf8_to_path` 构造输出
  `std::filesystem::path` 的导出调用方（`sla_floor`、`sls_export`、`waam_export`）。

## 4. Windows UTF-8 活动代码页清单（彻底方案）

由于 libigl / CGAL / OpenCASCADE / OpenVDB / miniz **仅接受窄**路径，仅靠 §3
无法在旧 ANSI 代码页下为它们保证完整 Unicode。HsBaSlicer 转而通过嵌入应用清单，
将整个进程的代码页提升为 UTF-8：

```xml
<activeCodePage xmlns="http://schemas.microsoft.com/SMI/2019/WindowsSettings">UTF-8</activeCodePage>
<longPathAware   xmlns="http://schemas.microsoft.com/SMI/2016/WindowsSettings">true</longPathAware>
```

- 文件：**`cmake/hsba_utf8.manifest`**；由 **`cmake/HsBaWindowsUtf8.cmake`** 中的可复用
  函数 `hsba_apply_utf8_codepage(<target>)` 应用（在构建后步骤中用清单工具
  `mt.exe` 将片段合并进已链接的可执行文件，因此兼容 Ninja / MSBuild / Makefile
  生成器；在非 MSVC / 非 Windows 工具链上为 no-op）。它由各可执行文件自己的
  `CMakeLists.txt` 逐目标调用。
- 它被应用于我们拥有的每个桌面可执行文件（`HsBaSlicer` 应用与全部 samples）。

效果：`GetACP()` 返回 65001，于是 `utf8_to_local` 变为直通
（`windows_chcp_utf8()` 为真），CRT 会把 UTF-8 窄路径正确转换为 UTF-16——**全部**
窄 API 格式仅凭一处构建配置改动即获得完整 Unicode 支持（并借助 `longPathAware`
支持超长路径）。

## 5. 限制与集成说明

- 进程清单来自**拥有该进程的可执行文件**。当本库以 **`DllHsBaSlicer`** 被外部
  C#/Java/Python 宿主加载时，代码页由*宿主可执行文件*的清单决定，HsBaSlicer 无法
  更改。此场景下宽 API 处理（`utf8_to_path` / `path_to_utf8`）仍能保护我们自己的
  文件 I/O；若要在窄的第三方路径上也获得完整 Unicode，宿主应在其自身可执行文件中
  声明相同的 `activeCodePage` UTF-8 清单。
- 在 POSIX/Android/iOS 上无需清单：native 路径编码本身即 UTF-8。
- 若要在 Windows 控制台读/写非 ASCII 文本，可在启动时调用 `set_windows_console_utf8()`。

## 另见

- [Base 模块](./base/) —— 编码与通用工具
- [FileOperator 模块](./fileoperator/) —— 压缩 / 解压
- [C++ 使用指南（CMake 集成）](./cpp_cmake_usage.md)
