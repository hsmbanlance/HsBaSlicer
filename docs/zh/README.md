# HsBaSlicer 文档

欢迎来到 HsBaSlicer 文档。此仓库包含多种语言的文档。

## 语言版本

- [English Documentation](../en/) - English version of the documentation
- [中文文档](./) - 中文版本的文档

## 关于 HsBaSlicer

HsBaSlicer 是一个面向 3D 打印切片领域的高性能 C++ 软件框架，提供模块化、跨平台的切片核心能力。

## 模块概览

### 基础层

- **Base** - 基本类型、基础组件和接口定义（单例、模板辅助、委托、协程、对象池、线程池、静态反射等）
- **Utils** - 扩展工具集（应用配置、Lua 绑定、结构化 JSON/YAML/XML 等）
- **Logger** - 线程安全的单例日志系统

### 文件与数据

- **Cipher** - 加密、哈希和编解码工具（AES/3DES/RSA、MD5/SHA、Base64/Hex）
- **FileOperator** - 文件和属性配置树操作，包含 ZIP 压缩/解压、SQLite 数据库、Lua 适配器等
- **Proto** - Protobuf 消息定义（网格、切片配置、路径、点、变换等）
- **Convert** - 切片过程中的类型与 Protobuf 消息的相互转换

### 几何模型

- **2D** - 二维多边形处理（整数/浮点多边形、凸包、图像转多边形、多边形填充）
- **MeshModel** - 网格模型（CGAL / IGL / OpenCascade 三种后端）
- **CADModel** - CAD 模型（基于 OpenCascade 的 B-Rep 建模与布尔运算）
- **Preprocess** - 模型预处理与加载

### 切片核心

- **Paths** - 输出路径管理（层路径、点路径、图像路径、机器人路径）
- **Support** - 支撑生成（FDM/SLA 支撑、悬垂检测、Lua 自定义支撑）
- **[LibHsBaSlicer](./LibHsBaSlicer/)** - 底层 C++ 静态库，提供预处理、切片、支撑、填充、路径生成五大核心接口
- **DllHsBaSlicer** - 上层 C 动态库，提供基于协程优化的 FDM 全流程 Pipeline 接口
- **HsBaSlicer** - 最终应用程序入口

### 其他

- **Samples** - 使用示例（如 FDM 工艺流水线示例）
- **Tests / Static Tests** - 单元测试与静态测试套件
- **Android** - Android 平台工程
- **Version** - 版本信息

## 详细模块文档

### 基础层

- [Base 模块](./base/) - 单例、模板辅助、委托、协程、元组遍历、任意类型访问、静态反射、任意对象、对象池、内存池、线程池
- [Utils 模块](./utils/) - 应用配置、结构化 JSON、Lua 绑定、预编译头、日志配置
- [Logger 模块](./logger/) - 日志单例、日志状态

### 文件与数据

- [Cipher 模块](./cipher/) - 加密、哈希、编码
- [FileOperator 模块](./fileoperator/) - 压缩、解压、数据库操作

### 切片核心

- [LibHsBaSlicer 模块](./LibHsBaSlicer/) - 预处理、切片、支撑、填充、路径生成
- [DllHsBaSlicer 模块](./DllHsBaSlicer/) - C 导出层，FDM/SLA/SLS 全流程流水线，Qt/wxWidgets 与 Unity/UE 跨平台集成指南

### 快速开始

- [C++ 使用指南（CMake 集成）](./cpp_cmake_usage.md) - 如何在外部 C++ 项目中通过 CMake 使用 LibHsBaSlicer / DllHsBaSlicer / HsBaSlicer（模块与非模块版）

# HsBaSlicer 文档

欢迎来到 HsBaSlicer 文档。此仓库包含多种语言的文档。

## 语言版本

- [English Documentation](../en/) - English version of the documentation
- [中文文档](./) - 中文版本的文档

## 关于 HsBaSlicer

HsBaSlicer 是一个面向 3D 打印切片领域的高性能 C++ 软件框架，提供模块化、跨平台的切片核心能力。

## 模块概览

### 基础层

- **Base** - 基本类型、基础组件和接口定义（单例、模板辅助、委托、协程、对象池、线程池、静态反射等）
- **Utils** - 扩展工具集（应用配置、Lua 绑定、结构化 JSON/YAML/XML 等）
- **Logger** - 线程安全的单例日志系统

### 文件与数据

- **Cipher** - 加密、哈希和编解码工具（AES/3DES/RSA、MD5/SHA、Base64/Hex）
- **FileOperator** - 文件和属性配置树操作，包含 ZIP 压缩/解压、SQLite 数据库、Lua 适配器等
- **Proto** - Protobuf 消息定义（网格、切片配置、路径、点、变换等）
- **Convert** - 切片过程中的类型与 Protobuf 消息的相互转换

### 几何模型

- **2D** - 二维多边形处理（整数/浮点多边形、凸包、图像转多边形、多边形填充）
- **MeshModel** - 网格模型（CGAL / IGL / OpenCascade 三种后端）
- **CADModel** - CAD 模型（基于 OpenCascade 的 B-Rep 建模与布尔运算）
- **Preprocess** - 模型预处理与加载

### 切片核心

- **Paths** - 输出路径管理（层路径、点路径、图像路径、机器人路径）
- **Support** - 支撑生成（FDM/SLA 支撑、悬垂检测、Lua 自定义支撑）
- **LibHsBaSlicer** - 底层 C++ 静态库，提供预处理、切片、支撑、填充、路径生成五大核心接口
- **DllHsBaSlicer** - 上层 C 动态库，提供基于协程优化的 FDM 全流程 Pipeline 接口
- **HsBaSlicer** - 最终应用程序入口

### 其他

- **Samples** - 使用示例（如 FDM 工艺流水线示例）
- **Tests / Static Tests** - 单元测试与静态测试套件
- **Android** - Android 平台工程
- **Version** - 版本信息
# HsBaSlicer 文档

欢迎来到 HsBaSlicer 文档。此仓库包含多种语言的文档。

## 语言版本

- [English Documentation](../en/) - 英文版本的文档
- [中文文档](./) - 中文版本的文档

## 关于 HsBaSlicer

HsBaSlicer 是一款用于增材制造的3D切片软件，提供了处理3D模型和生成切片路径的各种实用工具和组件。

项目包含多个实用模块：

## Base 模块

- **Singleton** - 线程安全的单例模式实现
- **Template Helper** - 各种模板相关的实用函数
- **Delegate** - 类型安全的委托/事件系统
- **Coroutine** - 协程和异步任务支持
- **Tuple Each** - Tuple元素迭代和操作函数
- **Any Visit** - 对std::any和boost::any的类型安全访问
- **Static Reflect** - 编译时类型反射功能

## Utils 模块

- **AppConfig** - 应用程序配置的单例模式实现
- **Struct JSON** - C++结构体与JSON之间的序列化和反序列化
- **LuaNewObject** - C++与Lua之间对象创建和内存管理的工具函数
- **PCH Headers** - 预编译头文件，包含常用库头文件
- **LogCfg** - INI格式的日志系统配置文件

## Cipher 模块

- **Encrypt** - 提供多种加密算法（AES、3DES、RSA等）
- **Hasher** - 提供多种哈希算法（MD5、SHA1、SHA256等）
- **Encoder** - 提供Base64和Hex编码解码功能

## FileOperator 模块

- **Zipper** - 基于miniz的ZIP压缩功能
- **Unzipper** - 基于miniz的ZIP解压缩功能
- **SQL Adapter** - SQLite数据库操作功能

## Logger 模块

- **LoggerSingletone** - 线程安全的单例日志记录器
- **LogState** - 提供自定义字面量操作符的日志状态类