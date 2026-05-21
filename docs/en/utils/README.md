# Utils Module Usage Guide

The Utils module provides multiple utility C++ tools and functions, including application configuration, JSON serialization, and Lua integration.

## Component List

- [AppConfig](./app_config.md) - Provides singleton pattern implementation for application configuration
- [Struct JSON](./struct_json.md) - Provides serialization and deserialization between C++ structures and JSON
- [LuaNewObject](./luanewobject.md) - Provides utility functions for object creation and memory management between C++ and Lua
- [LuaAnyObject](./luaanyobject.md) - Provides functionality for registering and using AnyObject types in Lua
- [PCH Headers](./pch_headers.md) - Precompiled header file containing common library headers
- [LogCfg](./logcfg.md) - INI format log system configuration file

## Usage

To use components in the Utils module, you need to include the corresponding header files in your code:

```cpp
#include "utils/app_config.hpp"
#include "utils/struct_json.hpp"
#include "utils/LuaNewObject.hpp"
```

These components are header-only libraries and require no additional linking steps.