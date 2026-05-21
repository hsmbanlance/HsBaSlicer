# HsBaSlicer Documentation

Welcome to the HsBaSlicer documentation. This repository contains documentation in multiple languages.

## Language Versions

- [English Documentation](./) - English version of the documentation
- [中文文档](../zh/) - Chinese version of the documentation

## About HsBaSlicer

HsBaSlicer is a 3D slicer for additive manufacturing that provides various utilities and components for processing 3D models and generating slicing paths.

The project includes several utility modules:

## Base Module

- **Singleton** - Thread-safe singleton pattern implementation
- **Template Helper** - Various template-related utility functions
- **Delegate** - Type-safe delegate/event system
- **Coroutine** - Coroutine and asynchronous task support
- **Tuple Each** - Tuple element iteration and manipulation functions
- **Any Visit** - Type-safe access to std::any and boost::any
- **Static Reflect** - Compile-time type reflection functionality

## Utils Module

- **AppConfig** - Singleton pattern implementation for application configuration
- **Struct JSON** - Serialization and deserialization between C++ structures and JSON
- **LuaNewObject** - Utility functions for object creation and memory management between C++ and Lua
- **PCH Headers** - Precompiled header files containing common library headers
- **LogCfg** - INI format log system configuration file

## Cipher Module

- **Encrypt** - Provides multiple encryption algorithms (AES, 3DES, RSA, etc.)
- **Hasher** - Provides multiple hash algorithms (MD5, SHA1, SHA256, etc.)
- **Encoder** - Provides Base64 and Hex encoding/decoding functions

## FileOperator Module

- **Zipper** - ZIP compression functionality based on miniz
- **Unzipper** - ZIP decompression functionality based on miniz
- **SQL Adapter** - SQLite database operation functionality

## Logger Module

- **LoggerSingletone** - Thread-safe singleton logger
- **LogState** - Log state class providing custom literal operators