# HsBaSlicer Documentation

Welcome to the HsBaSlicer documentation. This repository contains documentation in multiple languages.

## Language Versions

- [English Documentation](./en/) - English version of the documentation
- [中文文档](./zh/) - Chinese version of the documentation

## About HsBaSlicer

HsBaSlicer is a high-performance C++ software framework for 3D printing slicing, providing modular and cross-platform slicing core capabilities.

## Module Overview

### Foundation Layer

- **Base** - Core types, fundamental components, and interface definitions (Singleton, Template Helper, Delegate, Coroutine, Object Pool, Thread Pool, Static Reflect, etc.)
- **Utils** - Extended utilities (App Config, Lua bindings, structured JSON/YAML/XML, etc.)
- **Logger** - Thread-safe singleton logging system

### File & Data

- **Cipher** - Encryption, hashing, and encoding tools (AES/3DES/RSA, MD5/SHA, Base64/Hex)
- **FileOperator** - File and property tree operations, including ZIP compression/decompression, SQLite database, Lua adapter, etc.
- **Proto** - Protobuf message definitions (mesh, slice config, paths, points, transforms, etc.)
- **Convert** - Type and Protobuf message conversion for the slicing pipeline

### Geometry Models

- **2D** - 2D polygon processing (integer/float polygons, convex hull, image-to-polygon, polygon fill)
- **MeshModel** - Mesh models with three backends (CGAL / IGL / OpenCascade)
- **CADModel** - CAD models with OpenCascade-based B-Rep modeling and boolean operations
- **Preprocess** - Model preprocessing and loading

### Slicing Core

- **Paths** - Output path management (layer paths, point paths, image paths, robot paths)
- **Support** - Support generation (FDM/SLA support, overhang detection, Lua custom support)
- **[LibHsBaSlicer](./en/LibHsBaSlicer/)** - Core C++ static library providing five major interfaces: Preprocess, Slice, Support, Fill, Path Generation
- **DllHsBaSlicer** - Upper-level C dynamic library providing coroutine-optimized FDM full-pipeline interface
- **HsBaSlicer** - Final application entry point

### Other

- **Samples** - Usage examples (e.g., FDM process pipeline examples)
- **Tests / Static Tests** - Unit test and static test suites
- **Android** - Android platform project
- **Version** - Version information

## Detailed Module Documentation

### Foundation Layer

- [Base Module](./en/base/) - Singleton, Template Helper, Delegate, Coroutine, Tuple Each, Any Visit, Static Reflect, Any Object, Object Pool, Memory Pool, Thread Pool
- [Utils Module](./en/utils/) - AppConfig, Struct JSON, LuaNewObject, PCH Headers, LogCfg
- [Logger Module](./en/logger/) - LoggerSingletone, LogState

### File & Data

- [Cipher Module](./en/cipher/) - Encrypt, Hasher, Encoder
- [FileOperator Module](./en/fileoperator/) - Zipper, Unzipper, SQL Adapter

### Slicing Core

- [LibHsBaSlicer Module](./en/LibHsBaSlicer/) - Preprocess, Slice, Support, Fill, Path Generation
# HsBaSlicer Documentation

Welcome to the HsBaSlicer documentation. This repository contains documentation in multiple languages.

## Language Versions

- [English Documentation](./en/) - English version of the documentation
- [中文文档](./zh/) - Chinese version of the documentation

## About HsBaSlicer

HsBaSlicer is a high-performance C++ software framework for 3D printing slicing, providing modular and cross-platform slicing core capabilities.

## Module Overview

### Foundation Layer

- **Base** - Core types, fundamental components, and interface definitions (Singleton, Template Helper, Delegate, Coroutine, Object Pool, Thread Pool, Static Reflect, etc.)
- **Utils** - Extended utilities (App Config, Lua bindings, structured JSON/YAML/XML, etc.)
- **Logger** - Thread-safe singleton logging system

### File & Data

- **Cipher** - Encryption, hashing, and encoding tools (AES/3DES/RSA, MD5/SHA, Base64/Hex)
- **FileOperator** - File and property tree operations, including ZIP compression/decompression, SQLite database, Lua adapter, etc.
- **Proto** - Protobuf message definitions (mesh, slice config, paths, points, transforms, etc.)
- **Convert** - Type and Protobuf message conversion for the slicing pipeline

### Geometry Models

- **2D** - 2D polygon processing (integer/float polygons, convex hull, image-to-polygon, polygon fill)
- **MeshModel** - Mesh models with three backends (CGAL / IGL / OpenCascade)
- **CADModel** - CAD models with OpenCascade-based B-Rep modeling and boolean operations
- **Preprocess** - Model preprocessing and loading

### Slicing Core

- **Paths** - Output path management (layer paths, point paths, image paths, robot paths)
- **Support** - Support generation (FDM/SLA support, overhang detection, Lua custom support)
- **LibHsBaSlicer** - Core C++ static library providing five major interfaces: Preprocess, Slice, Support, Fill, Path Generation
- **DllHsBaSlicer** - Upper-level C dynamic library providing coroutine-optimized FDM full-pipeline interface
- **HsBaSlicer** - Final application entry point

### Other

- **Samples** - Usage examples (e.g., FDM process pipeline examples)
- **Tests / Static Tests** - Unit test and static test suites
- **Android** - Android platform project
- **Version** - Version information
# HsBaSlicer Documentation

Welcome to the HsBaSlicer documentation. This repository contains documentation in multiple languages.

## Language Versions

- [English Documentation](./en/) - English version of the documentation
- [中文文档](./zh/) - 中文版本的文档

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