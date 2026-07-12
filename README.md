"""
Unified README inserted by script.
"""

# HsBaSlicer / README

HsBaSlicer is a high-performance C++ framework for 3D-printing slicers. It is modular, cross-platform, and intended for research and production-level slicing pipelines.

HsBaSlicer 是一个面向 3D 打印切片领域的高性能 C++ 软件框架，提供模块化、跨平台的切片核心能力，适用于研发与生产级别的切片流水线。

## 项目结构 / Project Structure

主要模块（概览）：

"""
Updated README content below replaces the previous duplicated and inconsistent sections with a unified bilingual overview and build instructions.
"""

# HsBaSlicer / README

HsBaSlicer is a high-performance C++ framework for 3D-printing slicers.  It is modular, cross-platform, and intended for research and production-level slicing pipelines.

HsBaSlicer 是一个面向 3D 打印切片领域的高性能 C++ 软件框架，提供模块化、跨平台的切片核心能力，适用于研发与生产级别的切片流水线。

## 项目结构 / Project Structure

主要模块（概览）：

- `base` / 基础：基本类型、模板辅助、单例、线程池、静态反射等。
- `utils` / 工具：配置、序列化、Lua 绑定等辅助模块。
- `fileoperator` / 文件：ZIP、SQLite、配置树、Lua 适配器。
- `meshmodel` / 网格：CGAL / IGL / OpenCascade 后端。
- `cadmodel` / CAD：基于 OpenCascade 的 B-Rep 与布尔运算。
- `paths` / 路径：层路径、点路径、机器人路径输出。
- `support` / 支撑：FDM/SLA 支撑生成与悬垂检测。
- `LibHsBaSlicer` / 静态库：核心切片接口集合。
- `DllHsBaSlicer` / 动态库：导出 C ABI 的运行时流水线接口。

（更多模块和细节见源码树）

## 构建说明 / Build

必需工具 / Requirements:

- `cmake` >= 3.12
- 支持 C++20 的编译器
- `vcpkg`（建议用于依赖管理）
- PowerShell（Windows 下建议安装 PowerShell 7；Windows PowerShell 5.1 也可用）

Required tools:

- `cmake` >= 3.12
- A C++ compiler with C++20 support
- `vcpkg` (recommended)
- PowerShell (on Windows, PowerShell 7 recommended; Windows PowerShell 5.1 is acceptable)

### Windows

建议：安装 Visual Studio 2022 或更高版本，勾选 "Desktop development with C++"、CMake 工具和 vcpkg。并安装 Git。将 vcpkg 路径加入系统环境变量以便在 IDE/命令行中使用。

建议：使用 `windows-release` 预设进行发布构建。`windows-debug` 在包含 CGAL 的子模块时可能会遇到问题。

### Linux / macOS

示例安装（以 Ubuntu/Debian 为例）：

```bash
sudo apt update
sudo apt install -y cmake git python3 python3-venv g++ gdb make ninja-build pkg-config automake libtool m4 autoconf python3-distutils libx11-dev mesa-common-dev libglu1-mesa-dev libxi-dev libxrender-dev libxtst-dev bison flex libncurses-dev libtirpc-dev libfontconfig1-dev rsync zip
```

安装 vcpkg：

```bash
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh
```

构建项目（示例）：

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release
```

或者使用 CMake 预设：

```bash
cmake . --preset linux-release
cd out/build/linux-release
cmake --build . --config Release
```

### Android / iOS

Android 编译通常在 Linux 环境下完成，需安装 Android SDK/NDK，并设置 `ANDROID_NDK_HOME`。iOS 在 CI 上已验证，macOS 本地构建未进行广泛测试。

## 贡献 / Contributing

欢迎提交 issue 或 PR。对外部依赖和构建选项有疑问时，请先参考项目 `CMakeLists.txt` 与 `proto/CMakeLists.txt`。

## 许可证 / License

见项目根目录下的 LICENSE.txt。
