---
kind: logging_system
name: Cross-Platform Singleton Logger (Boost.Log / os_log / android log)
category: logging_system
scope:
    - '**'
source_files:
    - logger/logger.hpp
    - logger/logger.cpp
    - utils/logcfg.ini
    - docs/en/logger/README.md
    - docs/zh/logger/README.md
---

## Overview

HsBaSlicer ships a small, self-contained logging subsystem under `logger/` that wraps three platform backends behind a single C++ API:

| Platform | Backend | Sink(s) |
|---|---|---|
| Desktop (Windows/macOS/Linux) | Boost.Log (`boost/log/trivial`) | Console + optional rotating file |
| iOS | `os_log.h` (`os_log_with_type`) | System console log |
| Android | `android/log.h` (`__android_log_print`) | Kernel log (`logcat`) |

The public entry point is the singleton class `HsBa::Slicer::Log::LoggerSingletone`, exposed via the `HSBA_SLICER_LOG_API` export macro. Consumers call either the static helpers `LogDebug` / `LogInfo` / `LogWarning` / `LogError` or the fluent user-defined literals `_log_debug` / `_log_info` / `_log_warning` / `_log_error`.

## Key files

- `logger/logger.hpp` — Public API: `LoggerSingletone` class, `LogState` fluent wrapper, and the four `operator""_log_*` literals in the inline namespace `HsBa::Slicer::Log::LogLiterals`.
- `logger/logger.cpp` — Implementation: per-platform backend selection, configuration loading from `logcfg.ini`, level mapping, and sink setup.
- `utils/logcfg.ini` — Default runtime configuration (see below).
- `docs/en/logger/` and `docs/zh/logger/` — Doxygen documentation for the logger module.

## Architecture and conventions

### Level model
Levels are plain integers with the following semantics (defined in `GetLogLevel` / `GetiOSLogType` / `GetAndroidLogPriority`):

| Integer | Meaning | Boost.Log | iOS | Android |
|---|---|---|---|---|
| 0 | trace | `trace` | `OS_LOG_TYPE_DEBUG` | `ANDROID_LOG_VERBOSE` |
| 1 | debug | `debug` | `OS_LOG_TYPE_DEBUG` | `ANDROID_LOG_DEBUG` |
| 2 | info | `info` | `OS_LOG_TYPE_INFO` | `ANDROID_LOG_INFO` |
| 3 | warning | `warning` | `OS_LOG_TYPE_DEFAULT` | `ANDROID_LOG_WARN` |
| 4 | error | `error` | `OS_LOG_TYPE_ERROR` | `ANDROID_LOG_ERROR` |
| 5 | fatal | `fatal` | `OS_LOG_TYPE_FAULT` | `ANDROID_LOG_FATAL` |

Unknown levels fall back to `debug` in `_DEBUG` builds and `warning` otherwise.

### Initialization and defaults
`LoggerSingletone` constructor reads `<current_path>/logcfg.ini` via `Config::from_ini`. If the file is missing or a key is absent, it falls back to:

- `log_level = 3` (warning), `log_level_debug = 1` (debug)
- `use_log_file = false`
- `log_file = /log/log.txt` (relative to current path)
- `log_datatime_format = "%Y-%m-%d %H:%M:%S"`

On desktop platforms, when `use_log_file` is true, Boost.Log writes to the configured file with 50 MB rotation size and midnight time-based rotation; both console and file sinks share one formatter:

```
[TimeStamp] <severity> [file:line] function_name: message\n
```

Timestamps come from `boost::posix_time::ptime` and are also registered as a global attribute named `TimeStamp`.

### Source location capture
Every logging method takes an optional `std::source_location` parameter defaulting to `std::source_location::current()`. The helper `GetSourceLocation` formats it as `[filename:line] function_name: `. This is prepended to every emitted message across all three backends.

### Thread safety
The singleton uses `std::shared_mutex mutex_` plus `std::once_flag instance_flag_` and `std::call_once` to ensure one-time construction. `UseLogFile()` acquires a shared lock before reading state.

### Fluent literal syntax
The inline namespace `LogLiterals` exposes:

```cpp
"some message"_log_info();
"some message"_log_debug();
"some message"_log_warning();
"some message"_log_error();
```

Each literal returns a `LogState` whose destructor invokes `LoggerSingletone::Log(message, level, location)`.

### Usage pattern in the codebase
All sample executables (`samples/*/main.cpp`, `HsBaSlicer/HsBaSlicer.cpp`) use the static helpers directly:

```cpp
LoggerSingletone::LogInfo(msg);
LoggerSingletone::LogInfo(std::format("[{}%] {}", percent, stage));
```

No other source files in the repository were found using the logger; the samples demonstrate the intended usage at the application boundary.

## Conventions and constraints

- **Levels are integers 0–5.** There is no enum type; callers pass numeric constants and the implementation maps them to the platform-specific severity. New levels must be added in all three switch statements (`GetLogLevel`, `GetiOSLogType`, `GetAndroidLogPriority`) and in the main `switch` inside `LoggerSingletone::Log`.
- **Configuration is read once at first construction** from `logcfg.ini` under the process's current working directory. Keys are `log.log_level`, `log.log_level_debug`, `log.use_log_file`, `log.log_file`, and `log_format.log_datatime_format`.
- **File output is off by default** (`use_log_file = false`). When enabled, logs rotate at 50 MB and midnight.
- **Desktop logging goes through Boost.Log**; iOS uses `os_log`; Android uses `__android_log_print`. The three branches are mutually exclusive via `#ifdef` guards around `__ANDROID__` and `TARGET_OS_IOS`.
- **The singleton is lazily initialized**: `GetInstance()` and each `Log*` helper check whether `instance_` is null and construct it on demand via `std::call_once`.
- **Exported symbols** are marked with `HSBA_SLICER_LOG_API` so the logger can be consumed from other modules built as separate DLLs/shared libraries.
- **Structured fields are not used.** Log entries are flat strings composed of timestamp, severity, source location, and the user-supplied message. No JSON or key-value pairs are emitted.