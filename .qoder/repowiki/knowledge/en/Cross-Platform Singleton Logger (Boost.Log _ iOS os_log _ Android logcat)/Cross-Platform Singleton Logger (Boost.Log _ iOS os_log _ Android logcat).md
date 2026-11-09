---
kind: logging_system
name: Cross-Platform Singleton Logger (Boost.Log / iOS os_log / Android logcat)
category: logging_system
scope:
    - '**'
source_files:
    - logger/logger.hpp
    - logger/logger.cpp
    - utils/logcfg.ini
    - docs/en/logger/README.md
    - docs/en/logger/logger_singletone.md
    - docs/en/logger/log_state.md
---

## Overview

HsBaSlicer ships a small, self-contained logging subsystem under `logger/` that wraps three platform backends behind a single C++20 API:

| Platform | Backend | Sink(s) |
|---|---|---|
| Desktop / Linux / Windows | Boost.Log (`boost/log/trivial.hpp`) | Console + optional rotating file |
| iOS | `os_log` (Foundation) | System console log |
| Android | `__android_log_print` (ndk) | Kernel logcat |

The public surface is the class `HsBa::Slicer::Log::LoggerSingletone` plus the inline namespace `LogLiterals` exposing user-defined literals `_log_debug`, `_log_info`, `_log_warning`, `_log_error`. Consumers call either the static methods `LogDebug/LogInfo/LogWarning/LogError` or the fluent literal syntax `"msg"_log_info();`.

## Key files

- `logger/logger.hpp` — public header: singleton class, level constants (int 0–5), source-location capture via `std::source_location`, fluent `LogState` type and UDLs.
- `logger/logger.cpp` — implementation: per-platform backend dispatch, config loading from `logcfg.ini`, Boost.Log formatter/filter setup, rotation policy.
- `utils/logcfg.ini` — default configuration shipped with the project.
- `docs/en/logger/README.md`, `logger_singletone.md`, `log_state.md` — Doxygen-style documentation for the logger.

## Architecture and conventions

### Level model
Levels are plain integers 0–5:

| Value | Meaning | Desktop backend | iOS type | Android priority |
|---|---|---|---|---|
| 0 | trace | `trace` | `OS_LOG_TYPE_DEBUG` | `ANDROID_LOG_VERBOSE` |
| 1 | debug | `debug` | `OS_LOG_TYPE_DEBUG` | `ANDROID_LOG_DEBUG` |
| 2 | info | `info` | `OS_LOG_TYPE_INFO` | `ANDROID_LOG_INFO` |
| 3 | warning | `warning` | `OS_LOG_TYPE_DEFAULT` | `ANDROID_LOG_WARN` |
| 4 | error | `error` | `OS_LOG_TYPE_ERROR` | `ANDROID_LOG_ERROR` |
| 5 | fatal | `fatal` | `OS_LOG_TYPE_FAULT` | `ANDROID_LOG_FATAL` |

The mapping functions `GetLogLevel`, `GetiOSLogType`, `GetAndroidLogPriority` in `logger.cpp` enforce this table. Unknown values fall through to `_DEBUG ? debug : warning` (desktop) or equivalent defaults on mobile.

### Source location enrichment
Every log line is prefixed by `GetSourceLocation(location)` which renders `[file:line] function_name: `. This is injected at every call site because all public entry points accept an implicit `std::source_location::current()` parameter; the fluent UDL path goes through `LogState::operator()` which forwards the same argument.

### Configuration
On construction the logger reads `<cwd>/logcfg.ini` via `Config::from_ini` (from `fileoperator/rw_ptree.hpp`). Expected keys:

```ini
[log]
log_level = 3              # release default
log_level_debug = 1        # _DEBUG build override
use_log_file = false
log_file = /log/log.txt
[log_format]
log_datatime_format=%Y-%m-%d %H:%M:%S
```

Defaults when the file is missing or a key is absent: `log_path_ = <cwd>/log/log.txt`, `use_log_file_ = false`, `log_datatime_format_ = "%Y-%m-%d %H:%M:%S"`.

### Sinks and rotation (desktop)
When `use_log_file` is true, Boost.Log adds a file sink with:
- Rotation size: `SIZE_50MB` (50 MiB).
- Time-based rotation at midnight (`rotation_at_time_point(0, 0, 0)`).
- Append mode (`std::ios::app`).

Console output is always enabled. The global filter is `severity >= log_level_`. A `TimeStamp` attribute is registered via `add_common_attributes()` and `local_clock()`.

### Thread safety
`LoggerSingletone` uses `std::shared_mutex mutex_` and `std::once_flag instance_flag_` with `std::call_once` to ensure one-time initialization of the singleton instance. Read accessors like `UseLogFile()` lock with `std::shared_lock`; write paths go through `CreateInstance` / constructor only.

### Fluent API
The `LogState` object holds `(log_lv_, message_)` and defers emission until its destructor or explicit `operator()(location)` is invoked. User-defined literals return a `LogState` so callers write:

```cpp
"initializing pipeline"_log_info();
"not use log file"_log_warning();
```

This is used throughout `HsBaSlicer/HsBaSlicer.cpp` and sample executables.

### Platform-specific behavior
- On iOS, `UseLogFile()` unconditionally returns `true` (the system log is considered "file-like") even though no local file is written.
- On Android, `UseLogFile()` also returns `true`.
- Both mobile platforms emit a one-time warning during construction indicating they are using the native kernel/system log.

## Conventions observed

1. **Call sites use either the static helpers or the UDLs** — the grep across the repo shows `samples/*/main.cpp` and `HsBaSlicer/HsBaSlicer.cpp` calling `LoggerSingletone::LogInfo(...)` for startup messages and `"..."_log_info()` for status lines. No other logging framework is used anywhere else in the codebase.
2. **Level 3 (warning) is the release default**, level 1 (debug) in `_DEBUG` builds — enforced by the constructor's `#if _DEBUG` branches before config overrides.
3. **All log entries carry source location** — there is no way to bypass it; even the raw `Log(message, level)` path prepends `GetSourceLocation(location)`.
4. **Log levels are numeric, not enums** — callers pass integer constants 0–5 directly or indirectly through the four helper methods.
5. **Configuration is INI-driven** — the logger never accepts runtime parameters beyond what `logcfg.ini` provides; changing severity requires editing the file or rebuilding with `_DEBUG`.
6. **No structured fields** — the format string is a fixed concatenation of timestamp, severity, source location, and message; there is no key/value metadata layer.
7. **Export boundary** — the logger is exposed through `logger/export.h` (`HSBA_SLICER_LOG_API`) so downstream modules can link against it independently of the full slicer library.