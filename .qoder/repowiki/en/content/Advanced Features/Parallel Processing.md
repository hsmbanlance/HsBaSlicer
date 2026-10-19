# Parallel Processing

<cite>
**Referenced Files in This Document**
- [thread_pool.hpp](file://base/thread_pool.hpp)
- [coroutine.hpp](file://base/coroutine.hpp)
- [parallel.hpp](file://base/parallel.hpp)
- [pipeline_parallel.hpp](file://DllHsBaSlicer/pipeline_parallel.hpp)
- [ISupport.hpp](file://support/ISupport.hpp)
- [ISupport.cpp](file://support/ISupport.cpp)
- [path_generator.cpp](file://LibHsBaSlicer/Path/path_generator.cpp)
- [gcodepath.cpp](file://paths/gcodepath.cpp)
- [thread_pool_test.cpp](file://tests/Pool/thread_pool_test.cpp)
</cite>

## Update Summary
**Changes Made**
- Added new section on Process-Wide Pipeline Infrastructure with ThreadPool singleton and atomic cursor scheduling
- Updated Core Components section to include ParallelForLayers and ParallelMapLayers functions
- Enhanced Architecture Overview to show the new pipeline parallel execution model
- Added detailed analysis of atomic cursor-based load balancing mechanism
- Updated Performance Considerations with new optimization techniques
- Added backward compatibility shim documentation

## Table of Contents
1. [Introduction](#introduction)
2. [Project Structure](#project-structure)
3. [Core Components](#core-components)
4. [Process-Wide Pipeline Infrastructure](#process-wide-pipeline-infrastructure)
5. [Architecture Overview](#architecture-overview)
6. [Detailed Component Analysis](#detailed-component-analysis)
7. [Dependency Analysis](#dependency-analysis)
8. [Performance Considerations](#performance-considerations)
9. [Troubleshooting Guide](#troubleshooting-guide)
10. [Conclusion](#conclusion)
11. [Appendices](#appendices)

## Introduction
This document explains HsBaSlicer's parallel processing capabilities with a focus on the ThreadPool implementation, its coroutine integration through IExecutor and ThreadPoolExecutor, and the new process-wide pipeline infrastructure that provides atomic cursor-based load balancing for layer-parallel operations. It covers thread management, task submission, synchronization mechanisms, performance tuning, thread safety, exception handling in concurrent contexts, debugging strategies, and benchmarking recommendations.

## Project Structure
The parallel execution primitives are implemented under base:
- ThreadPool and ThreadPoolExecutor live in base/thread_pool.hpp.
- The coroutine abstraction, including IExecutor, Task, DispatchAwaiter, NoopExecutor, AsyncExecutor, and Generator utilities, lives in base/coroutine.hpp.
- **New**: Process-wide pipeline infrastructure with ThreadPool singleton and atomic cursor scheduling lives in base/parallel.hpp.
- Backward compatibility shim in DllHsBaSlicer/pipeline_parallel.hpp maintains existing includes.
- Unit tests demonstrating usage patterns are in tests/Pool/thread_pool_test.cpp.

```mermaid
graph TB
subgraph "base"
TP["ThreadPool<br/>task queue + worker threads"]
TPE["ThreadPoolExecutor<br/>IExecutor adapter"]
CE["Coroutine layer<br/>IExecutor / Task / Awaiters"]
PP["Pipeline Parallel<br/>ThreadPool singleton + atomic cursor"]
end
subgraph "DllHsBaSlicer"
BC["Backward Compat Shim<br/>pipeline_parallel.hpp"]
end
subgraph "tests"
TPT["thread_pool_test.cpp"]
end
CE --> TPE
TPE --> TP
PP --> TP
BC --> PP
TPT --> TP
TPT --> CE
```

**Diagram sources**
- [thread_pool.hpp:29-162](file://base/thread_pool.hpp#L29-L162)
- [thread_pool.hpp:170-206](file://base/thread_pool.hpp#L170-L206)
- [coroutine.hpp:47-70](file://base/coroutine.hpp#L47-L70)
- [parallel.hpp:64-68](file://base/parallel.hpp#L64-L68)
- [pipeline_parallel.hpp:12-14](file://DllHsBaSlicer/pipeline_parallel.hpp#L12-L14)
- [thread_pool_test.cpp:14-85](file://tests/Pool/thread_pool_test.cpp#L14-L85)

**Section sources**
- [thread_pool.hpp:1-214](file://base/thread_pool.hpp#L1-L214)
- [coroutine.hpp:1-1091](file://base/coroutine.hpp#L1-L1091)
- [parallel.hpp:1-263](file://base/parallel.hpp#L1-L263)
- [pipeline_parallel.hpp:1-15](file://DllHsBaSlicer/pipeline_parallel.hpp#L1-L15)
- [thread_pool_test.cpp:1-87](file://tests/Pool/thread_pool_test.cpp#L1-L87)

## Core Components
- **ThreadPool**: A bounded pool of worker threads that execute submitted tasks concurrently. It provides task submission via futures, pending/active task counters, and a WaitAll barrier.
- **IExecutor**: An abstract executor interface used by the coroutine layer to dispatch work. Implementations include NoopExecutor (inline), AsyncExecutor (std::async), and ThreadPoolExecutor (backed by ThreadPool).
- **ThreadPoolExecutor**: Bridges coroutines to ThreadPool by submitting lambdas to the underlying pool. It supports a default pool and per-instance pools.
- **Task and awaiters**: Coroutine-friendly abstractions that suspend/resume across executors, propagate exceptions safely, and support chaining callbacks.
- **Pipeline Pool**: Process-wide ThreadPool singleton accessed via `PipelinePool()` that provides shared thread pool across all pipeline operations.
- **ParallelForLayers**: Layer-parallel execution using atomic cursor scheduling for dynamic load balancing.
- **ParallelMapLayers**: New function that produces one result per layer in parallel, returning ordered results vector.

Key responsibilities:
- Thread lifecycle management and graceful shutdown in ThreadPool.
- Synchronization between producer (submitter) and consumers (workers) using mutexes and condition variables.
- Executor-driven coroutine scheduling via IExecutor.
- **New**: Process-wide resource sharing and atomic cursor-based work distribution.

**Section sources**
- [thread_pool.hpp:29-162](file://base/thread_pool.hpp#L29-L162)
- [thread_pool.hpp:170-206](file://base/thread_pool.hpp#L170-L206)
- [coroutine.hpp:47-70](file://base/coroutine.hpp#L47-L70)
- [coroutine.hpp:81-122](file://base/coroutine.hpp#L81-L122)
- [coroutine.hpp:184-198](file://base/coroutine.hpp#L184-L198)
- [coroutine.hpp:205-382](file://base/coroutine.hpp#L205-L382)
- [coroutine.hpp:388-555](file://base/coroutine.hpp#L388-L555)
- [parallel.hpp:64-68](file://base/parallel.hpp#L64-L68)
- [parallel.hpp:192-219](file://base/parallel.hpp#L192-L219)

## Process-Wide Pipeline Infrastructure

### ThreadPool Singleton Pattern
The new infrastructure introduces a process-wide ThreadPool singleton that eliminates per-call thread pool creation overhead and enables efficient resource sharing across all pipeline stages.

**Updated** Major infrastructure enhancement with process-wide ThreadPool singleton, atomic cursor-based load balancing, new ParallelMapLayers function, and backward compatibility shim

```mermaid
flowchart TD
Start(["First PipelineCall"]) --> CheckEnv["ResolvePipelineThreads()<br/>Read HSBA_PIPELINE_THREADS env"]
CheckEnv --> CreatePool["Create static ThreadPool<br/>with resolved thread count"]
CreatePool --> ReturnPool["Return reference to singleton"]
subgraph "Subsequent Calls"
NextCall["Any Pipeline Call"] --> DirectAccess["Direct access to<br/>existing ThreadPool"]
end
DirectAccess --> UsePool["Use shared thread pool"]
```

**Diagram sources**
- [parallel.hpp:37-54](file://base/parallel.hpp#L37-L54)
- [parallel.hpp:64-68](file://base/parallel.hpp#L64-L68)

### Atomic Cursor-Based Load Balancing
The core innovation is an atomic cursor-based work distribution mechanism that provides dynamic load balancing without fixed block sizes or barriers between blocks.

```mermaid
sequenceDiagram
participant Main as "Main Thread"
participant Cursor as "atomic<int> cursor"
participant Worker1 as "Worker 1"
participant Worker2 as "Worker 2"
participant Done as "atomic<int> done"
Main->>Cursor : Initialize cursor = 0
Main->>Worker1 : Submit worker lambda
Main->>Worker2 : Submit worker lambda
loop Dynamic Work Distribution
Worker1->>Cursor : fetch_add(1) -> i
alt i < total
Worker1->>Worker1 : work(i)
Worker1->>Done : fetch_add(1)
else i >= total
Worker1->>Worker1 : Break loop
end
Worker2->>Cursor : fetch_add(1) -> j
alt j < total
Worker2->>Worker2 : work(j)
Worker2->>Done : fetch_add(1)
else j >= total
Worker2->>Worker2 : Break loop
end
end
Note over Main : Poll done counter every 32 layers
```

**Diagram sources**
- [parallel.hpp:114-136](file://base/parallel.hpp#L114-L136)
- [parallel.hpp:138-168](file://base/parallel.hpp#L138-L168)

**Section sources**
- [parallel.hpp:37-54](file://base/parallel.hpp#L37-L54)
- [parallel.hpp:64-68](file://base/parallel.hpp#L64-L68)
- [parallel.hpp:90-174](file://base/parallel.hpp#L90-L174)

### ParallelForLayers and ParallelMapLayers Functions
Two primary interfaces provide layer-parallel execution with different return semantics:

**ParallelForLayers**: Executes work for each layer index with optional progress callbacks. Workers write only to their indexed output slots, ensuring thread safety while maintaining order preservation.

**ParallelMapLayers**: New function that produces one result per layer in parallel, returning a pre-sized vector where results are written by index to maintain input order correspondence.

Both functions support `HSBA_PIPELINE_THREADS=1` for serial execution, enabling regression testing against original behavior.

**Section sources**
- [parallel.hpp:192-219](file://base/parallel.hpp#L192-L219)

### Backward Compatibility Shim
The `DllHsBaSlicer/pipeline_parallel.hpp` file serves as a compatibility shim that redirects includes to the new `base/parallel.hpp`, allowing existing pipeline code to continue working without modification.

**Section sources**
- [pipeline_parallel.hpp:1-15](file://DllHsBaSlicer/pipeline_parallel.hpp#L1-L15)

## Architecture Overview
The system now composes four layers:
- Application code submits CPU-bound or IO-bound work as std::function objects or coroutine Tasks.
- **New**: Pipeline infrastructure provides process-wide ThreadPool singleton and atomic cursor scheduling.
- ThreadPoolExecutor adapts IExecutor to ThreadPool.submit.
- Coroutine machinery uses IExecutor to resume coroutines on the appropriate execution context.

```mermaid
sequenceDiagram
participant App as "Application Code"
participant PP as "Pipeline Parallel"
participant TP as "ThreadPool"
participant TPE as "ThreadPoolExecutor"
participant Cor as "Coroutine Layer"
App->>PP : ParallelForLayers(total, work)
PP->>TP : submit(worker lambdas)
TP-->>App : futures
App->>Cor : co_await Task<R, TPE>
Cor->>TPE : execute(resume_handle)
TPE->>TP : submit([resume handle])
TP->>TP : WorkerLoop() executes task
TP-->>Cor : notify completion
Cor-->>App : resume coroutine
```

**Diagram sources**
- [thread_pool.hpp:67-90](file://base/thread_pool.hpp#L67-L90)
- [thread_pool.hpp:118-154](file://base/thread_pool.hpp#L118-L154)
- [thread_pool.hpp:180-184](file://base/thread_pool.hpp#L180-L184)
- [coroutine.hpp:99-106](file://base/coroutine.hpp#L99-L106)
- [coroutine.hpp:187-193](file://base/coroutine.hpp#L187-L193)
- [parallel.hpp:192-219](file://base/parallel.hpp#L192-L219)

## Detailed Component Analysis

### ThreadPool Implementation
ThreadPool manages:
- A vector of std::thread workers.
- A FIFO queue of std::function<void()> tasks.
- A mutex protecting the queue and stop flag.
- Two condition variables: one for waking workers when new tasks arrive, another for signaling completion to WaitAll.
- Atomic counters for active tasks and a boolean stop flag.

Lifecycle:
- Constructor validates thread count > 0 and launches workers.
- Destructor sets stop flag, notifies all workers, and joins threads.
- submit wraps callable into packaged_task, enqueues it, increments active_tasks_, and notifies a worker.
- PendingTasks returns queue size; ActiveTasks returns atomic counter.
- WaitAll blocks until queue is empty and active_tasks_ is zero.

Worker loop:
- Waits on condition variable until stop or non-empty queue.
- Pops a task, executes it inside try/catch, decrements active_tasks_, and signals finished_condition_.

Thread safety notes:
- All shared state access is guarded by queue_mutex_.
- Exceptions thrown by tasks are caught and do not crash workers; active_tasks_ still decrements.
- Stop flag prevents new submissions after shutdown.

```mermaid
flowchart TD
Start(["Submit"]) --> CheckStop{"Stopped?"}
CheckStop --> |Yes| ThrowErr["Throw RuntimeError"]
CheckStop --> |No| Enqueue["Enqueue task<br/>Increment active_tasks_"]
Enqueue --> Notify["Notify one worker"]
Notify --> ReturnFuture["Return future"]
subgraph "Worker Loop"
WStart(["Worker start"]) --> WaitCV["Wait on condition_variable"]
WaitCV --> CheckStopW{"stop_ && empty?"}
CheckStopW --> |Yes| Exit["Exit worker"]
CheckStopW --> |No| PopTask{"Queue empty?"}
PopTask --> |Yes| WaitCV
PopTask --> |No| Execute["Execute task"]
Execute --> DecActive["Decrement active_tasks_"]
DecActive --> NotifyAll["Notify finished_condition_"]
NotifyAll --> WaitCV
end
```

**Diagram sources**
- [thread_pool.hpp:67-90](file://base/thread_pool.hpp#L67-L90)
- [thread_pool.hpp:118-154](file://base/thread_pool.hpp#L118-L154)

**Section sources**
- [thread_pool.hpp:29-162](file://base/thread_pool.hpp#L29-L162)

### IExecutor and ThreadPoolExecutor Integration
IExecutor defines a single execute method taking an rvalue std::function<void()>. Implementations:
- NoopExecutor: runs inline synchronously.
- AsyncExecutor: uses std::async with async launch policy.
- ThreadPoolExecutor: forwards to ThreadPool.submit.

ThreadPoolExecutor behavior:
- Can be constructed with a specific ThreadPool or use a default pool.
- SetDefaultPool allows global configuration of the default executor.
- get_pool resolves instance pool, then default pool, then creates a static default pool sized to hardware_concurrency if none provided.

```mermaid
classDiagram
class IExecutor {
<<interface>>
+execute(func) void
}
class NoopExecutor {
+execute(func) void
}
class AsyncExecutor {
+execute(func) void
}
class ThreadPool {
+submit(f,args) future
+PendingTasks() size_t
+ActiveTasks() size_t
+WaitAll() void
}
class ThreadPoolExecutor {
-pool_ : ThreadPool*
-default_pool_ : ThreadPool*
+execute(func) void
+SetDefaultPool(pool) void
}
NoopExecutor --|> IExecutor
AsyncExecutor --|> IExecutor
ThreadPoolExecutor --|> IExecutor
ThreadPoolExecutor --> ThreadPool : "submits tasks"
```

**Diagram sources**
- [coroutine.hpp:47-70](file://base/coroutine.hpp#L47-L70)
- [thread_pool.hpp:170-206](file://base/thread_pool.hpp#L170-L206)

**Section sources**
- [coroutine.hpp:47-70](file://base/coroutine.hpp#L47-L70)
- [thread_pool.hpp:170-206](file://base/thread_pool.hpp#L170-L206)

### Coroutine-Based Async Operations
The coroutine layer provides:
- Task<T, Executor>: a promise-based asynchronous result wrapper with initial suspension via DispatchAwaiter.
- TaskAwaiter: bridges awaiting a Task to resuming the caller coroutine on the executor.
- DispatchAwaiter: schedules coroutine resumption through IExecutor.
- CustomAllocatorTask and related awaiters: allow custom allocators for coroutine allocations.

Execution flow:
- When a coroutine awaits a Task, TaskAwaiter registers a finally callback that resumes the coroutine on the executor once the Task completes.
- Initial suspension ensures the first step runs on the executor rather than the calling thread.

```mermaid
sequenceDiagram
participant Caller as "Caller Coroutine"
participant TA as "TaskAwaiter"
participant Exe as "IExecutor"
participant TP as "ThreadPool"
participant Task as "Task<T, Exe>"
Caller->>TA : await Task
TA->>Exe : execute(resume handle)
Exe->>TP : submit([resume handle])
TP-->>Exe : task done
Exe-->>Caller : resume coroutine
Caller->>Task : get_result()
```

**Diagram sources**
- [coroutine.hpp:99-106](file://base/coroutine.hpp#L99-L106)
- [coroutine.hpp:187-193](file://base/coroutine.hpp#L187-L193)
- [coroutine.hpp:277-285](file://base/coroutine.hpp#L277-L285)

**Section sources**
- [coroutine.hpp:81-122](file://base/coroutine.hpp#L81-L122)
- [coroutine.hpp:184-198](file://base/coroutine.hpp#L184-L198)
- [coroutine.hpp:205-382](file://base/coroutine.hpp#L205-L382)
- [coroutine.hpp:388-555](file://base/coroutine.hpp#L388-L555)

### Practical Examples from Tests
The unit tests demonstrate:
- Submitting multiple tasks and collecting results via futures.
- Using ThreadPoolExecutor with coroutines by setting a default pool.
- Mixing std::future-based tasks and coroutine Tasks within the same pool.

These examples illustrate:
- Proper pool sizing and lifecycle.
- Safe interaction between futures and coroutines.
- Cleanup of default executor configuration.

**Section sources**
- [thread_pool_test.cpp:14-37](file://tests/Pool/thread_pool_test.cpp#L14-L37)
- [thread_pool_test.cpp:39-58](file://tests/Pool/thread_pool_test.cpp#L39-L58)
- [thread_pool_test.cpp:60-85](file://tests/Pool/thread_pool_test.cpp#L60-L85)

### Support Generation Parallelization
The support generation system now leverages the new parallel infrastructure through the ISupport interface:

**IsLayerIndependent()**: New virtual method that declares whether Generate can be called independently for each layer. Default returns true, enabling automatic parallelization.

**GenerateAll()**: Enhanced implementation that uses ParallelMapLayers when IsLayerIndependent() returns true, falling back to serial iteration otherwise.

This design allows different support algorithms to opt into parallelization based on their dependency characteristics.

**Section sources**
- [ISupport.hpp:46-54](file://support/ISupport.hpp#L46-L54)
- [ISupport.cpp:14-46](file://support/ISupport.cpp#L14-L46)

### Path Generation Parallelization
Path generation functions now utilize ParallelMapLayers for improved throughput:

**GenerateGCodePath**: Uses ParallelMapLayers to generate GPoint vectors for outlines, fills, and supports per layer, then merges results on the main thread.

**GenerateGCodePathV2**: Applies ParallelMapLayers to build combined PolygonsD per layer, optimizing the path building process.

Both approaches maintain thread safety by keeping GCodePath modifications on the main thread while parallelizing the computationally intensive polygon processing.

**Section sources**
- [path_generator.cpp:70-112](file://LibHsBaSlicer/Path/path_generator.cpp#L70-L112)
- [path_generator.cpp:114-149](file://LibHsBaSlicer/Path/path_generator.cpp#L114-L149)

### G-code Serialization Parallelization
The G-code serialization process has been optimized with ParallelMapLayers:

**ToGCode**: Now generates individual layer G-code strings in parallel using ParallelMapLayers, then concatenates them on the main thread. This approach significantly reduces serialization time for large models while maintaining deterministic output ordering.

**Section sources**
- [gcodepath.cpp:333-368](file://paths/gcodepath.cpp#L333-L368)

## Dependency Analysis
- ThreadPool depends on standard concurrency primitives and error types.
- ThreadPoolExecutor depends on IExecutor and ThreadPool.
- Coroutine layer depends on IExecutor implementations to schedule work.
- **New**: Pipeline parallel infrastructure depends on ThreadPool and provides shared resource management.
- Support and path generation modules depend on the new parallel infrastructure.
- Tests depend on both ThreadPool and coroutine abstractions.

```mermaid
graph LR
TP["ThreadPool"] --> STD["std::thread / std::mutex / std::condition_variable"]
TPE["ThreadPoolExecutor"] --> TP
CE["Coroutine Layer"] --> IEXE["IExecutor"]
IEXE --> TPE
PP["Pipeline Parallel"] --> TP
SUP["Support Generation"] --> PP
PATH["Path Generation"] --> PP
GCODE["G-code Serialization"] --> PP
TPT["thread_pool_test.cpp"] --> TP
TPT --> CE
```

**Diagram sources**
- [thread_pool.hpp:13-23](file://base/thread_pool.hpp#L13-L23)
- [thread_pool.hpp:170-206](file://base/thread_pool.hpp#L170-L206)
- [coroutine.hpp:47-70](file://base/coroutine.hpp#L47-L70)
- [parallel.hpp:64-68](file://base/parallel.hpp#L64-L68)
- [ISupport.cpp:10-10](file://support/ISupport.cpp#L10-L10)
- [path_generator.cpp:82-82](file://LibHsBaSlicer/Path/path_generator.cpp#L82-L82)
- [gcodepath.cpp:340-340](file://paths/gcodepath.cpp#L340-L340)
- [thread_pool_test.cpp:6-7](file://tests/Pool/thread_pool_test.cpp#L6-L7)

**Section sources**
- [thread_pool.hpp:1-214](file://base/thread_pool.hpp#L1-L214)
- [coroutine.hpp:1-1091](file://base/coroutine.hpp#L1-L1091)
- [parallel.hpp:1-263](file://base/parallel.hpp#L1-L263)
- [ISupport.cpp:1-47](file://support/ISupport.cpp#L1-L47)
- [path_generator.cpp:70-174](file://LibHsBaSlicer/Path/path_generator.cpp#L70-L174)
- [gcodepath.cpp:333-476](file://paths/gcodepath.cpp#L333-L476)
- [thread_pool_test.cpp:1-87](file://tests/Pool/thread_pool_test.cpp#L1-L87)

## Performance Considerations
Hardware concurrency detection:
- ThreadPool constructor defaults to std::thread::hardware_concurrency().
- ThreadPoolExecutor fallback default pool also uses hardware_concurrency(), clamped to at least 1.
- **New**: ResolvePipelineThreads() reads HSBA_PIPELINE_THREADS environment variable, falling back to hardware_concurrency() if unset or invalid.

Task queue management:
- ThreadPool uses a simple FIFO queue protected by a mutex.
- Workers wake on condition_variable notifications.
- WaitAll uses a separate condition_variable to block until all tasks complete.
- **New**: Atomic cursor-based scheduling eliminates fixed block boundaries and provides dynamic load balancing.

Memory barriers and atomics:
- active_tasks_ is atomic<size_t>, ensuring safe cross-thread visibility without additional locks.
- stop_ is atomic<bool>, enabling lock-free checks in worker loops.
- **New**: cursor and done counters use std::atomic<int> with memory_order_relaxed for optimal performance in the hot path.

Optimization opportunities:
- For high-throughput slicing, consider batching small tasks to reduce queue contention.
- Use dedicated pools per workload type to avoid priority inversion.
- Tune pool size based on workload characteristics (CPU-bound vs IO-bound).
- Avoid excessive logging or heavy operations inside hot paths to minimize lock hold times.
- **New**: The atomic cursor approach provides better load balancing than fixed block sizes, especially for uneven workloads.
- **New**: Process-wide ThreadPool eliminates per-call thread pool creation overhead.

Benchmarking guidelines:
- Measure throughput and latency across varying pool sizes.
- Compare std::future-based tasks versus coroutine Tasks for similar workloads.
- Profile cache locality and memory allocation patterns when slicing large meshes.
- Validate scalability on multi-core systems and compare against single-threaded baselines.
- **New**: Test HSBA_PIPELINE_THREADS=1 vs parallel execution for regression validation.
- **New**: Measure the impact of atomic cursor scheduling vs traditional fixed-block approaches.

[No sources needed since this section provides general guidance]

## Troubleshooting Guide
Common issues and remedies:
- Submitting tasks after shutdown:
  - ThreadPool throws a runtime error if submit is called after stop flag is set. Ensure application logic does not submit after destruction or explicit shutdown.
- Deadlocks or hangs:
  - Verify that WaitAll is only called when no more tasks will be submitted.
  - Ensure coroutines are awaited or their results consumed to prevent resource leaks.
  - **New**: Avoid nested parallel calls within worker callbacks, as this can cause thread pool exhaustion.
- Exception handling:
  - ThreadPool catches exceptions from tasks but still decrements active_tasks_; ensure your tasks report errors appropriately.
  - Coroutine Task propagates exceptions via Result::get_or_throw; use catching or then chains to handle them.
  - **New**: Exceptions from worker lambdas in RunAtomicIndexed are rethrown via futures.get() on the calling thread.
- Debugging concurrent code:
  - Use PendingTasks and ActiveTasks to monitor pool state.
  - Add structured logging around task boundaries to trace execution order.
  - In tests, verify final states and counters to detect races.
  - **New**: Monitor atomic cursor and done counters to understand work distribution patterns.

Relevant behaviors:
- ThreadPool destructor sets stop flag and joins threads.
- Worker loop catches std::exception and continues processing remaining tasks.
- Coroutine promise stores exceptions and rethrows on get_result or get_or_throw.
- **New**: PipelinePool() creates a static ThreadPool that persists for the entire process lifetime.
- **New**: HSBA_PIPELINE_THREADS environment variable controls parallelism level.

**Section sources**
- [thread_pool.hpp:47-61](file://base/thread_pool.hpp#L47-L61)
- [thread_pool.hpp:77-89](file://base/thread_pool.hpp#L77-L89)
- [thread_pool.hpp:140-153](file://base/thread_pool.hpp#L140-L153)
- [coroutine.hpp:277-285](file://base/coroutine.hpp#L277-L285)
- [coroutine.hpp:449-457](file://base/coroutine.hpp#L449-L457)
- [parallel.hpp:37-54](file://base/parallel.hpp#L37-L54)
- [parallel.hpp:64-68](file://base/parallel.hpp#L64-L68)
- [parallel.hpp:170-173](file://base/parallel.hpp#L170-L173)

## Conclusion
HsBaSlicer's parallel processing stack centers on a robust ThreadPool and a flexible coroutine framework built on IExecutor. The new process-wide pipeline infrastructure with atomic cursor-based load balancing provides significant performance improvements for layer-parallel operations while maintaining thread safety and deterministic output ordering. ThreadPool provides reliable task execution, synchronization, and lifecycle management. ThreadPoolExecutor integrates seamlessly with the coroutine layer, allowing ergonomic asynchronous programming while leveraging the underlying thread pool. By understanding these components, you can design efficient parallel slicing pipelines, tune performance for different workloads, and maintain thread safety and correctness.

[No sources needed since this section summarizes without analyzing specific files]

## Appendices

### API Reference Summary
- ThreadPool:
  - submit: Enqueues a task and returns a future.
  - PendingTasks: Returns number of queued tasks.
  - ActiveTasks: Returns number of currently executing tasks.
  - WaitAll: Blocks until all tasks complete.
- ThreadPoolExecutor:
  - execute: Submits a function to the underlying ThreadPool.
  - SetDefaultPool: Configures the default pool for coroutine tasks.
- IExecutor:
  - execute: Abstract interface for scheduling functions.
- **New**: Pipeline Infrastructure:
  - PipelinePool(): Returns process-wide ThreadPool singleton.
  - ResolvePipelineThreads(): Resolves thread count from environment or hardware.
  - ParallelForLayers(): Executes work for each layer index with optional progress callbacks.
  - ParallelMapLayers(): Produces one result per layer in parallel, returning ordered results.
  - PipelineThreadCount(): Returns cached worker count matching PipelinePool construction.

**Section sources**
- [thread_pool.hpp:67-111](file://base/thread_pool.hpp#L67-L111)
- [thread_pool.hpp:180-188](file://base/thread_pool.hpp#L180-L188)
- [coroutine.hpp:47-53](file://base/coroutine.hpp#L47-L53)
- [parallel.hpp:37-75](file://base/parallel.hpp#L37-L75)
- [parallel.hpp:192-219](file://base/parallel.hpp#L192-L219)