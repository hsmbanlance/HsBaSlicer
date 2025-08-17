[![CMake on multiple platforms](https://github.com/hsmbanlance/HsBaSlicer/actions/workflows/cmake-multi-platform.yml/badge.svg)](https://github.com/hsmbanlance/HsBaSlicer/actions/workflows/cmake-multi-platform.yml)

# 如何构建

你需要安装CMake工具链和Vcpkg来进行构建。

## Windows

建议安装Visual Studio 2020或以上版本，并在安装过程中勾选“使用C++的桌面开发”选项、CMake和Vcpkg。还需要安装Git。

如果需要复制dll和pdb文件，则需要在首次编译后再次配置并编译。
如果你安装了Visual Studio 2022但在Visual Studio Code等IDE中调试，把Vcpkg的安装路径添加到环境变量中。

## Linux

建议使用Ubuntu 20.04或Debian 10.0以上版本。

### 安装CMake和git

安装CMake：

```bash
sudo apt install cmake
```

如果使用的是Ubuntu 20.04，则使用snap安装CMake：

```bash
sudo snap install cmake --classic
```

CMake版本过低，构建时会发出警告。和Boost的构建有关。

因为使用Openscade,需要安装X11开发包。
如果使用的是Ubuntu 20.04或Debian 10.0以上版本，则可以直接安装X11开发包：
```bash
sudo apt install libx11-dev mesa-common-dev libglu1-mesa-dev libxi-dev libxmu-dev libxmu-headers
```
编译libmysql需要安装
```
sudo apt install libncurses-dev libtirpc-dev
```

安装git：

```bash
sudo apt install git
```

### 安装Vcpkg

```bash
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh
```

建议把vcpkg的安装路径添加到环境变量中。
如果添加到环境变量中，则可以使用CMake预设。

### 安装Ninja和编译工具链

```bash
sudo apt install g++ gdb make ninja-build rsync zip
```

### 安装其他依赖

```bash
sudo apt-get install pkg-config automake libtool m4 autoconf python3-distutils libx11-dev mesa-common-dev
```

### 构建

```bash
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build .
```

如果需要复制共享库，则需要在首次编译后再次配置并编译:

```bash
cmake .. -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake -DBUILD_SHARED_LIBS=ON
cmake --build .
```

## macOS(未测试)
