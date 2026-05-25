# Utils 模块使用说明

Utils 模块提供了多个实用的 C++ 工具和功能，包括应用程序配置、JSON 序列化以及 Lua 集成等功能。

## 组件列表

- [AppConfig (应用程序配置)](./app_config.md) - 提供应用程序配置的单例模式实现
- [Struct JSON (结构体 JSON 序列化)](./struct_json.md) - 提供 C++ 结构体与 JSON 之间的序列化和反序列化功能
- [LuaNewObject (Lua 对象管理)](./luanewobject.md) - 提供 C++ 与 Lua 之间对象创建和内存管理的工具函数
- [LuaAnyObject (Lua 任意对象管理)](./luaanyobject.md) - 提供在 Lua 中注册和使用 AnyObject 类型的功能
- [PCH Headers (预编译头文件)](./pch_headers.md) - 预编译头文件，包含常用库头文件
- [LogCfg (日志配置文件)](./logcfg.md) - INI 格式的日志系统配置文件

## 使用方法

要使用 Utils 模块中的组件，您需要在代码中包含相应的头文件：

```cpp
#include "utils/app_config.hpp"
#include "utils/struct_json.hpp"
#include "utils/LuaNewObject.hpp"
```

这些组件都是头文件库，无需额外的链接步骤。