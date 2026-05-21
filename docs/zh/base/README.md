# Base 模块使用说明

HsBaSlicer 的 Base 模块提供了多个通用的 C++ 组件和工具，包括单例模式、模板辅助、委托、协程、元组遍历、任意类型访问和静态反射等功能。

## 组件列表

- [Singleton (单例模式)](./singleton.md) - 提供线程安全的单例模式实现
- [Template Helper (模板辅助)](./template_helper.md) - 提供各种模板相关的辅助功能
- [Delegate (委托)](./delegate.md) - 实现类型安全的委托/事件系统
- [Coroutine (协程)](./coroutine.md) - 提供协程和异步任务支持
- [Tuple Each (元组遍历)](./tuple_each.md) - 提供元组元素的遍历和操作功能
- [Any Visit (任意类型访问)](./any_visit.md) - 提供对 std::any 和 boost::any 的类型安全访问
- [Static Reflect (静态反射)](./static_reflect.md) - 提供编译时的类型反射功能
- [Any Object (任意对象)](./any_object.md) - 提供运行时类型反射和动态调用功能
- [Object Pool (对象池)](./object_pool.md) - 提供基于名称管理的对象池，支持自动清理和容量限制
- [Memory Pool (内存池)](./memory_pool.md) - 提供共享和静态两种内存池分配器实现
- [Thread Pool (线程池)](./thread_pool.md) - 提供高效的线程池实现，支持任务提交和协程集成

## 使用方法

要使用 Base 模块中的组件，您需要在代码中包含相应的头文件：

```cpp
#include "base/singleton.hpp"
#include "base/template_helper.hpp"
#include "base/delegate.hpp"
#include "base/coroutine.hpp"
#include "base/tuple_each.hpp"
#include "base/any_visit.hpp"
#include "base/static_reflect.hpp"
#include "base/any_object.hpp"
#include "base/object_pool.hpp"
#include "base/memory_pool.hpp"
#include "base/thread_pool.hpp"
```

这些组件都是头文件库，无需额外的链接步骤。