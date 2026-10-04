# FileOperator 模块文档

FileOperator 模块提供了压缩、解压缩、文件读写等多种文件操作功能。该模块基于miniz库实现了ZIP格式的压缩和解压缩功能，并支持通过Lua脚本进行文件操作。

## 组件列表

- [Zipper (压缩类)](./zipper.md) - 基于miniz的ZIP压缩功能，另含基于bit7z（7-Zip）的 Bit7zZipper 多格式压缩（直接支持 `.tar.gz`/`.tar.xz`）
- [Unzipper (解压缩类)](./unzipper.md) - 基于miniz的ZIP解压缩功能，另含基于bit7z（7-Zip）的 Bit7ZUnzipper 多格式解压（直接支持 `.tar.gz`/`.tar.xz`）
- [SQL Adapter (SQL适配器)](./sql_adapter.md) - SQLite数据库操作功能