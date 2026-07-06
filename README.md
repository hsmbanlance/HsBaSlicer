# HsBaSlicer

HsBaSlicer 是一个面向 3D 打印切片领域的高性能 C++ 软件框架，提供模块化、跨平台的切片核心能力。

## 项目结构

### 基础层

| 文件夹 | 说明 |
|--------|------|
| **base** | 基本类型、基础组件和接口定义（单例、模板辅助、委托、协程、对象池、线程池、静态反射等） |
| **utils** | 相对于 base 的扩展工具（应用配置、Lua 绑定、结构化 JSON/YAML/XML 等） |
| **logger** | 线程安全的单例日志系统 |

### 文件与数据

| 文件夹 | 说明 |
|--------|------|
| **fileoperator** | 文件和属性配置树操作，包含 ZIP 压缩/解压、SQLite 数据库、Lua 适配器等 |
| **cipher** | 加密、哈希和编解码工具（AES/3DES/RSA、MD5/SHA、Base64/Hex） |
| **proto** | Protobuf 消息定义（网格、切片配置、路径、点、变换等） |
| **convert** | 切片过程中的类型与 Protobuf 消息的相互转换 |

### 几何模型

| 文件夹 | 说明 |
|--------|------|
| **2D** | 二维多边形处理（整数/浮点多边形、凸包、图像转多边形、多边形填充） |
| **meshmodel** | 网格模型（CGAL / IGL / OpenCascade 三种后端） |
| **cadmodel** | CAD 模型（基于 OpenCascade 的 B-Rep 建模与布尔运算） |
| **preprocess** | 模型预处理与加载 |

### 切片核心

| 文件夹 | 说明 |
|--------|------|
| **paths** | 输出路径管理（层路径、点路径、图像路径、机器人路径） |
| **support** | 支撑生成（FDM/SLA 支撑、悬垂检测、Lua 自定义支撑） |
| **LibHsBaSlicer** | 底层 C++ 静态库，提供预处理、切片、支撑、填充、路径生成五大核心接口 |
| **DllHsBaSlicer** | 上层 C 动态库，提供基于协程优化的 FDM 全流程 Pipeline 接口 |
| **HsBaSlicer** | 最终应用程序入口 |

### 其他

| 文件夹 | 说明 |
|--------|------|
| **samples** | 使用示例（如 FDM 工艺流水线示例） |
| **tests / static_tests** | 单元测试与静态测试套件 |
| **docs** | 项目文档（含中英文版本） |
| **android** | Android 平台工程 |
| **version** | 版本信息 |

## 如何构建

[![CMake on multiple platforms](https://github.com/hsmbanlance/HsBaSlicer/actions/workflows/cmake-multi-platform.yml/badge.svg)](https://github.com/hsmbanlance/HsBaSlicer/actions/workflows/cmake-multi-platform.yml)

你需要安装 CMake 工具链和 Vcpkg 来进行构建。至少需要 CMake 3.12 版本，且编译器至少需要支持 C++20 标准。

### Windows

建议安装 Visual Studio 2022 或以上版本，并在安装过程中勾选"使用 C++ 的桌面开发"选项、CMake 和 Vcpkg。还需要安装 Git。

如果需要复制 DLL 和 PDB 文件，则需要在首次编译后再次配置并编译。
如果你安装了 Visual Studio 2022 但在 Visual Studio Code 等 IDE 中调试，把 Vcpkg 的安装路径添加到环境变量中。

直接选择预设 `windows-release` 配置和编译即可。

> `windows-debug` 仅用于测试部分内存问题，因为 CGAL 不适用 Debug 版本。

不支持 MinGW/MSYS2 等。

### Linux

建议使用 Ubuntu 20.04 或 Debian 10.0 以上版本。

#### 安装 CMake 和 git

安装 CMake：

```bash
sudo apt install cmake
```

如果使用的是 Ubuntu，则可以使用 snap 安装 CMake：

```bash
sudo snap install cmake --classic
```

CMake 版本过低，构建时会发出警告。和 Boost 的构建有关。

安装 Python 和 Python 虚拟环境：

```bash
sudo apt install python3 python3-venv
```

因为使用 OpenCASCADE，需要安装 X11 开发包。
如果使用的是 Ubuntu 20.04 或 Debian 10.0 以上版本，则可以直接安装 X11 开发包：

```bash
sudo apt install libx11-dev mesa-common-dev libglu1-mesa-dev libxi-dev libxmu-dev libxmu-headers libxrender-dev libxtst-dev autoconf-archive
```

编译 libmysql 需要安装：

```bash
sudo apt install libncurses-dev libtirpc-dev
```

编译 libpq 需要安装：

```bash
sudo apt install bison flex
```

如果需要使用 OpenCASCADE，需要安装以下依赖：

```bash
sudo apt install -y libfontconfig1-dev
```

安装 git：

```bash
sudo apt install git
```

#### 安装 Vcpkg

```bash
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh
```

建议把 Vcpkg 的安装路径添加到环境变量中。
如果添加到环境变量中，则可以使用 CMake 预设。

#### 安装 Ninja 和编译工具链

```bash
sudo apt install g++ gdb make ninja-build rsync zip
```

#### 安装其他依赖

```bash
sudo apt-get install pkg-config automake libtool m4 autoconf python3-distutils libx11-dev mesa-common-dev
sudo apt-get install bison flex
```

#### 构建

```bash
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build .
```

或者直接使用 CMake 预设：

```bash
cmake . --preset linux-release
cd out/build/linux-release
cmake --build .
```

### macOS（未测试）

仅测试了 GitHub Action 可以编译和单元测试通过。

需要安装依赖：

```bash
brew install cmake ninja python autoconf automake libtool autoconf-archive
```

### 编译 Android 版本

在 Linux 中编译，从可以编译 Linux 版本的设备进行编译。

先下载 Android SDK 和 NDK，需要 SDK 版本至少支持到 28，NDK 至少 r27d，以支持 C++20。

然后把 Android NDK 路径添加到环境变量 `ANDROID_NDK_HOME` 中。

可以直接使用 CMake 预设：

```bash
cmake . --preset android-release

cd out/build/android-release
cmake ..
```

### iOS（未测试）

仅测试了 GitHub Action 可以编译通过，目标平台至少为 16.3 以支持 C++20。

需要安装依赖：

```bash
brew install cmake ninja python autoconf automake libtool autoconf-archive
```
# HsBaSlicer

（未完成）

以下为计划的项目结构。

## 项目结构

### 文件夹base

一些基本类型、基础支持和接口定义。

### 文件夹utils

相对于base扩展的工具。

### 文件夹fileoperator

文件和属性配置树的操作，数据库等也被视为文件。 

### 名称类似为xxxmodel的文件夹

模型和定义和支持。

### 文件夹convert

Slicer过程中的类型和protobuf定义的类型的交换。 

### 文件夹LibHsBaSiler

导出出口的C++静态库。

### 文件夹DllHsBaSlicer

最终导出的系统API导出的动态库。

### 文件夹HsBaSlicer

导出的应用程序。

## 如何构建

[![CMake on multiple platforms](https://github.com/hsmbanlance/HsBaSlicer/actions/workflows/cmake-multi-platform.yml/badge.svg)](https://github.com/hsmbanlance/HsBaSlicer/actions/workflows/cmake-multi-platform.yml)

你需要安装CMake工具链和Vcpkg来进行构建。至少需要CMake 3.12版本，且编译器至少需要支持C++20标准。

### Windows

建议安装Visual Studio 2022或以上版本，并在安装过程中勾选“使用C++的桌面开发”选项、CMake和Vcpkg。还需要安装Git。

如果需要复制dll和pdb文件，则需要在首次编译后再次配置并编译。
如果你安装了Visual Studio 2022但在Visual Studio Code等IDE中调试，把Vcpkg的安装路径添加到环境变量中。

直接选择预设windows-release配置和编译即可。

`windows-debug仅用于测试部分内存问题，因为cgal不适用Debug版本。`

不支持MingW/Msys2等。

### Linux

建议使用Ubuntu 20.04或Debian 10.0以上版本。

#### 安装CMake和git

安装CMake：

```bash
sudo apt install cmake
```

如果使用的是Ubuntu，则可以使用snap安装CMake：

```bash
sudo snap install cmake --classic
```

CMake版本过低，构建时会发出警告。和Boost的构建有关。

安装python和python虚拟环境：

```bash
sudo apt install python3 python3-venv
```

因为使用Openscade,需要安装X11开发包。
如果使用的是Ubuntu 20.04或Debian 10.0以上版本，则可以直接安装X11开发包：

```bash
sudo apt install libx11-dev mesa-common-dev libglu1-mesa-dev libxi-dev libxmu-dev libxmu-headers libxrender-dev libxtst-dev autoconf-archive
```

编译libmysql需要安装

```bash
sudo apt install libncurses-dev libtirpc-dev
```

编译libpq需要安装

```bash
sudo apt install bison flex
```

如果需要使用OpenCASCADE，需要安装以下依赖：

```bash
sudo apt install -y libfontconfig1-dev
```

安装git：

```bash
sudo apt install git
```

#### 安装Vcpkg

```bash
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh
```

建议把vcpkg的安装路径添加到环境变量中。
如果添加到环境变量中，则可以使用CMake预设。

#### 安装Ninja和编译工具链

```bash
sudo apt install g++ gdb make ninja-build rsync zip
```

#### 安装其他依赖

```bash
sudo apt-get install pkg-config automake libtool m4 autoconf python3-distutils libx11-dev mesa-common-dev
sudo apt-get install bison flex
```

#### 构建

```bash
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build .
```

或者直接使用CMake预设：

```bash
cmake . --preset linux-release
cd out/build/linux-release
cmake --build .
```

### macOS(未测试)

仅测试了Github Action可以编译和单元测试通过。

需要安装依赖

```bash
brew install cmake ninja python autoconf automake libtool autoconf-archive
```

### 编译安卓版本

在Linux中编译，从可以编译Linux版本的设备的进行编译。

先下载安卓SDK和NDK，需要SDK版本至少支持到28，NDK至少r27d，以支持C++20。

然后把Android NDK路径添加到环境变量ANDROID_NDK_HOME中。

可以直接使用CMake预设。

```bash
cmake . --preset android-release

cd out/build/android-release
cmake ..
```

### IOS（未测试）

仅测试了Github Action可以编译通过，目标平台至少为16.3以支持C++20。

需要安装依赖:

```bash
brew install cmake ninja python autoconf automake libtool autoconf-archive
```
