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
