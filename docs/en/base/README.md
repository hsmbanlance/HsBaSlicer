# Base Module Usage Guide

The Base module of HsBaSlicer provides multiple general-purpose C++ components and utilities, including singleton pattern, template helpers, delegates, coroutines, tuple iteration, type-safe any visits, and static reflection features.

## Component List

- [Singleton](./singleton.md) - Provides thread-safe singleton pattern implementation
- [Template Helper](./template_helper.md) - Provides various template-related utility functions
- [Delegate](./delegate.md) - Implements type-safe delegate/event system
- [Coroutine](./coroutine.md) - Provides coroutine and asynchronous task support
- [Tuple Each](./tuple_each.md) - Provides tuple element iteration and manipulation functions
- [Any Visit](./any_visit.md) - Provides type-safe access to std::any and boost::any
- [Static Reflect](./static_reflect.md) - Provides compile-time type reflection functionality
- [Any Object](./any_object.md) - Provides runtime type reflection and dynamic invocation capabilities
- [Object Pool](./object_pool.md) - Provides name-based object pool with automatic cleanup and capacity limits
- [Memory Pool](./memory_pool.md) - Provides shared and static memory pool allocator implementations
- [Thread Pool](./thread_pool.md) - Provides efficient thread pool with task submission and coroutine integration

## Usage

To use components in the Base module, you need to include the corresponding header files in your code:

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

All these components are header-only libraries and require no additional linking steps.