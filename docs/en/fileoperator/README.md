# FileOperator Module Documentation

The FileOperator module provides various file operation functions such as compression, decompression, file reading/writing, etc. This module implements ZIP format compression and decompression functions based on the miniz library and supports file operations through Lua scripts.

## Component List

- [Zipper (Compression Class)](./zipper.md) - ZIP compression functionality based on miniz, plus the bit7z (7-Zip) based Bit7zZipper with multi-format compression (direct `.tar.gz`/`.tar.xz` support)
- [Unzipper (Decompression Class)](./unzipper.md) - ZIP decompression functionality based on miniz, plus the bit7z (7-Zip) based Bit7ZUnzipper with multi-format extraction (direct `.tar.gz`/`.tar.xz` support)
- [SQL Adapter (SQL Adapter)](./sql_adapter.md) - SQLite database operation functionality