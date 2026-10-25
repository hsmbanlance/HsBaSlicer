---
kind: error_handling
name: 基于 std::runtime_error 派生类的异常错误处理体系
category: error_handling
scope:
    - '**'
source_files:
    - base/error.hpp
    - base/error.cpp
    - restapi/routes.cpp
    - restapi/job_manager.cpp
    - restapi/main.cpp
    - restapi/config_bridge.cpp
    - restapi/pipeline_executor.cpp
---

## 1. 总体方案

HsBaSlicer 采用 C++ 标准异常机制，未使用返回值 + error_code、std::expected 或 panic/recover。所有业务错误通过抛出 HsBa::Slicer::*Error 异常类表达，这些类全部继承自 std::runtime_error（见 base/error.hpp）。REST API 层在路由处理器中统一 catch (const std::exception&)，将异常转换为 HTTP 状态码与 JSON/XML 响应体；异步作业执行器 JobManager::Execute 同时捕获 std::exception 与 ...，把异常信息写入 JobEntry::error_message 并标记 JobState::Failed。

## 2. 核心文件与类型

- base/error.hpp / base/error.cpp：定义异常类型层次结构，根为 RuntimeError，子类包括 OutOfRangeError、InvalidArgumentError、IOError、NotImplementedError、NullValueError、NotSupportedError、NotFoundError、AlreadyExistsError、PermissionDeniedError、TimeoutError、InterruptedError、CancelledError、OutOfMemoryError。每个子类仅转发构造函数与 what()，无额外字段。
- restapi/routes.cpp：HTTP 路由层，集中 catch (const std::exception& e) 调用 ReplyError(res, code, message, detail, fmt) 返回 400/404/409/429/503 等状态码。
- restapi/job_manager.cpp：JobManager::Execute 包裹底层切片 runner，捕获异常后记录到 job->error_message。
- restapi/main.cpp：进程入口，顶层 try/catch 解析配置失败时打印 stderr 并返回非零退出码。

## 3. 架构与约定

### 3.1 异常类型分层

std::runtime_error -> HsBa::Slicer::RuntimeError -> 各语义化子类（OutOfRangeError、InvalidArgumentError、IOError、NotImplementedError、NullValueError、NotSupportedError、NotFoundError、AlreadyExistsError、PermissionDeniedError、TimeoutError、InterruptedError、CancelledError、OutOfMemoryError）。

所有子类均通过 using ...::runtime_error; 复用基类构造与 what()，不引入自定义数据成员。

### 3.2 抛出处分布

- 基础库/算法层（如 2D/FloatPolygons.cpp、2D/ImageToPolygons.cpp、2D/PolygonFill.cpp）：对 FreeType/Lua 初始化失败、Lua 函数缺失、参数非法等场景直接 throw RuntimeError / NotSupportedError。
- REST API 桥接层（restapi/config_bridge.cpp、restapi/pipeline_executor.cpp）：对 null 指针、未知 pipeline kind、JSON/XML 解析失败、proto 编解码失败等抛出 InvalidArgumentError 或 RuntimeError。
- HTTP 路由层（restapi/routes.cpp）：不对业务逻辑 catch 具体异常类型，统一以 catch (const std::exception& e) 捕获，映射为 400（invalid config body / config conversion failed）、404（job not found / output file not readable）、409（job not finished successfully / job is running）、429（job queue full）、503（paramstore not configured）。

### 3.3 REST API 的错误呈现

路由处理器通过 ReplyError(res, status, title, detail, format) 输出结构化错误体，detail 字段通常取自 e.what()。对于可恢复的业务状态（如 job 不存在、job 正在运行），则通过返回值枚举（如 JobManager::RemoveResult）而非异常区分，避免把正常流程控制混入异常流。

### 3.4 异步作业的错误隔离

JobManager::Execute 内部用两层 try/catch：
- catch (const std::exception& e)：保存 e.what() 到 thrown。
- catch (...)：兜底捕获非标准异常，设置 thrown = "unknown pipeline error"。
随后释放配置内存，加锁更新 job->state = Failed 与 job->error_message，保证异常不会泄漏到工作线程之外。

### 3.5 进程级兜底

restapi/main.cpp 的 main() 仅在配置加载阶段 catch std::exception，打印 stderr 并返回 1；若服务器启动后发生未捕获异常，由 cpp-httplib 默认行为处理，项目内未发现全局 std::set_terminate 或 std::set_unexpected 钩子。

## 4. 观察到的约定与约束

- 异常基类约定：所有业务异常必须继承自 HsBa::Slicer::RuntimeError（即间接继承 std::runtime_error），并通过 using 复用基类接口——这是 base/error.hpp 中定义的单一来源。
- REST 层统一捕获：所有 HTTP 处理器对可能抛异常的代码块使用 try/catch (const std::exception&)，并以 ReplyError 输出，而不是让异常冒泡到 httplib —— 见 routes.cpp 多处 handler 与 pipeline_executor.cpp 中的 switch 分支。
- 异步执行器双重捕获：JobManager::Execute 同时捕获 std::exception 与 ...，确保任何异常都被转化为 JobState::Failed + error_message，而不会导致工作线程崩溃 —— 见 job_manager.cpp:298-311。
- 进程入口显式退出码：配置解析失败返回 1，参数错误返回 2 —— 见 restapi/main.cpp:66-70 与 restapi/main.cpp:55-57。
- 无 panic/recover：仓库中未发现 setjmp/longjmp、std::set_terminate、std::set_unexpected 或类似 Go 风格的 recover 机制。
- 无全局错误码枚举：除 JobManager::RemoveResult 这类局部返回值外，未见统一的错误码枚举贯穿整个代码库；错误主要通过异常类型和 SliceOutcome::error_message 传递。
- assert/static_assert 用于不变量：restapi/pipeline_executor.cpp 使用 static_assert(HSBA_PIPELINE_* == N, "kind order drift") 校验流水线枚举顺序，属于编译期错误检查而非运行时错误处理。