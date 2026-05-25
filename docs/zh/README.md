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