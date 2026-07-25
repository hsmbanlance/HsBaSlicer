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

- `cmake` >= 3.28
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

## 安装与导出 / Install & Export

项目通过 `cmake --install` 提供完整的安装导出能力。安装后，下游项目可通过 `find_package(HsBaSlicer)` 直接引用。

### 库目标 / Library Targets

| 目标 | 类型 | 说明 |
|------|------|------|
| `DllHsBaSlicer` | 共享库 (.dll/.so) | C ABI 流水线接口，供外部语言（Java/C#/Python 等）调用 |
| `LibHsBaSlicer` | 静态/共享库 | C++ 全流程切片能力封装 |
| `HsBaSlicerLog` | 静态库 | 日志模块 |
| `HsBaSlicerBase` | 静态库 | 基础类型与工具 |
| `HsBaSlicerUtils` | 静态库 | 序列化、配置、Lua 绑定等辅助 |
| `HsBaSlicerMesh` | 静态库 | 网格模型（CGAL/IGL） |
| `HsBaSlicerCADModel` | 静态库 | CAD 模型（OpenCascade），桌面平台可用 |
| `HsBaSlicer2D` | 静态库 | 2D 多边形处理（Clipper2） |
| `HsBaPreprocess` | 静态库 | 模型加载与预处理 |
| `HsBaSupport` | 静态库 | FDM/SLA 支撑生成 |
| `HsBaPaths` | 静态库 | 路径输出 |
| `HsBaSlicerFileOperator` | 静态库 | 文件压缩、SQL、配置树 |
| `HsBaSlicerProto` | 静态库 | Protobuf 消息定义 |
| `HsBaCipher` | 静态库 | 加密与哈希 |
| `HsBaVersion` | 静态库 | 版本信息 |

安装后所有目标以 `HsBaSlicer::` 命名空间导出，例如 `HsBaSlicer::DllHsBaSlicer`、`HsBaSlicer::LibHsBaSlicer`。

### 头文件布局 / Header Layout

```
<install_prefix>/include/HsBaSlicer/
├── dllexport.h                  # DllHsBaSlicer C API
├── fdm_pipeline.h
├── sla_pipeline.h
├── initialize.h
├── version.hpp                  # 版本信息
├── base/                        # 基础类型
├── logger/                      # 日志
├── LibHsBaSlicer/               # C++ 全流程 API
│   ├── export.h
│   ├── version_info.hpp
│   ├── Slice/mesh_slice.hpp
│   ├── Preprocess/model_preprocess.hpp
│   ├── Support/fdm_support.hpp
│   ├── Fill/polygon_fill.hpp
│   ├── Path/path_generator.hpp
│   └── Floor/sla_floor.hpp
├── 2D/                          # 多边形处理
├── meshmodel/                   # 网格模型
├── support/                     # 支撑配置
├── paths/                       # 路径接口
└── proto/                       # Protobuf C++ 生成头 (.pb.h)
```

Proto 多语言导出文件（需启用 `HSBA_PROTOBUF_OUT`）：

```
<install_prefix>/share/HsBaSlicer/proto/
├── *.proto                      # 原始 Protobuf 定义
├── cpp/                         # C++ 生成源 (.pb.cc / .pb.h)
├── cs/                          # C# 生成源 (.cs)
├── java/                        # Java 生成源 (.java)
├── python/                      # Python 生成源 (_pb2.py)
└── php/                         # PHP 生成源 (.php)
```

### CMake 配置文件 / Config Files

```
<install_prefix>/lib/cmake/HsBaSlicer/
├── HsBaSlicerConfig.cmake
├── HsBaSlicerConfigVersion.cmake
└── HsBaSlicerTargets.cmake
```

下游项目引用示例：

```cmake
find_package(HsBaSlicer CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE HsBaSlicer::DllHsBaSlicer)
```

### 安装命令 / Install Command

```bash
cmake --install <build_dir> --prefix <install_prefix> --config Release
```

### 平台说明 / Platform Notes

- **桌面平台（Windows/Linux/macOS）**：所有库目标均参与安装。
- **Android / iOS / 游戏主机**：`HsBaSlicerCADModel` 不参与安装（因缺少 OpenCascade 支持）。

## 贡献 / Contributing

欢迎提交 issue 或 PR。对外部依赖和构建选项有疑问时，请先参考项目 `CMakeLists.txt` 与 `proto/CMakeLists.txt`。

## 许可证 / License

见项目根目录下的 LICENSE.txt。
