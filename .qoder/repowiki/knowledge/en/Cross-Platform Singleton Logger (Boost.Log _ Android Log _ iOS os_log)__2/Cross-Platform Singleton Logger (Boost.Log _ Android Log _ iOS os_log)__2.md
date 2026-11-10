---
kind: logging_system
name: Cross-Platform Singleton Logger (Boost.Log / Android Log / iOS os_log)
category: logging_system
scope:
    - '**'
source_files:
    - logger/logger.hpp
    - logger/logger.cpp
    - logger/export.h
    - utils/logcfg.ini
---

## Overview

HsBaSlicer ships a small, self-contained logging subsystem under `logger/` that wraps three platform backends behind a single C++ API:

- **Desktop / Linux**: Boost.Log (`boost/log/trivial.hpp`, file + console sinks).
- **Android**: `__android_log_print` with the `ANDROID_LOG_*` priorities.
- **iOS**: `os_log` / `os_log_with_type` via `<os/log.h>`.

The public surface is the class `HsBa::Slicer::Log::LoggerSingletone` plus an inline namespace `LogLiterals` exposing user-defined literals `_log_debug`, `_log_info`, `_log_warning`, `_log_error` for fluent syntax like `"msg"_log_info()`. The singleton is thread-safe through `std::shared_mutex` and `std::call_once`.

## Key Files

| File | Role |
|---|---|
| `logger/logger.hpp` | Public API: `LoggerSingletone` class, level helpers, `LogState` RAII wrapper, UDL operators. |
| `logger/logger.cpp` | Backend dispatch, config loading, sink setup, per-level switch logic. |
| `logger/export.h` | `HSBA_SLICER_LOG_API` visibility macro. |
| `utils/logcfg.ini` | Default runtime configuration template shipped with the project. |
| `docs/en/logger/...` and `docs/zh/logger/...` | Doxygen documentation pages for the logger module. |

## Architecture & Conventions

### Level model
Levels are plain integers 0–5:

| Value | Meaning | Desktop backend | Android priority | iOS type |
|---|---|---|---|---|
| 0 | trace | `trace` | `ANDROID_LOG_VERBOSE` | `OS_LOG_TYPE_DEBUG` |
| 1 | debug | `debug` | `ANDROID_LOG_DEBUG` | `OS_LOG_TYPE_DEBUG` |
| 2 | info | `info` | `ANDROID_LOG_INFO` | `OS_LOG_TYPE_INFO` |
| 3 | warning | `warning` | `ANDROID_LOG_WARN` | `OS_LOG_TYPE_DEFAULT` |
| 4 | error | `error` | `ANDROID_LOG_ERROR` | `OS_LOG_TYPE_ERROR` |
| 5 | fatal | `fatal` | `ANDROID_LOG_FATAL` | `OS_LOG_TYPE_FAULT` |

Unknown values fall back to `debug` in `_DEBUG` builds and `warning` otherwise.

### Sink selection
- On desktop (non-iOS, non-Android) both a **console** sink and an optional **file** sink are attached to `boost::log::core::get()`.
- File rotation is configured at construction: 50 MB max (`SIZE_50MB`) plus midnight time-based rotation.
- On Android and iOS the file/console sinks are not set up; `UseLogFile()` returns `true` on those platforms so callers can still write their own files if desired.

### Source location enrichment
Every log line is prefixed by `GetSourceLocation(location)`, which renders `[file:line] function_name: `. This is injected automatically because every public logging method takes `const std::source_location& location = std::source_location::current()`.

### Configuration
At construction the logger reads `<cwd>/logcfg.ini` via `Config::from_ini` (from `fileoperator/rw_ptree.hpp`). Keys consumed:

- `[log] log_level` — release threshold (default 3 = warning)
- `[log] log_level_debug` — debug build threshold (default 1 = debug)
- `[log] use_log_file` — enable file sink
- `[log] log_file` — appended to current working directory
- `[log_format] log_datatime_format` — strftime format for timestamps (default `%Y-%m-%d %H:%M:%S`)

If the file is missing or any key lookup throws `ptree_bad_path`, defaults are used (no file sink, default timestamp format). The shipped template lives at `utils/logcfg.ini`.

### Initialization order
Callers must ensure the singleton exists before emitting logs. Every public static method (`Log`, `LogDebug`, `LogInfo`, `LogWarning`, `LogError`) performs lazy initialization via `std::call_once(instance_flag_, ...)` when `instance_` is null. Samples initialize it early in `main()` using `LoggerSingletone::GetInstance()` or `CreateInstance()`.

### Fluent literal API
The `LogState` class captures `(log_lv, message)` and invokes `LoggerSingletone::Log` in its destructor, enabling expressions like:

```cpp
"Processing started"_log_info();
"Failed to open " << path << ""_log_error();
```

## Usage Pattern Across the Codebase

All sample executables (`samples/*/main.cpp`) follow the same pattern:

1. Create the logger singleton at process start.
2. Emit startup messages via `LoggerSingletone::LogInfo(...)`.
3. Report pipeline progress as `[percent%] stage` strings.

No other modules in this repository were found to call into the logger directly beyond the samples and the top-level `HsBaSlicer/HsBaSlicer.cpp`; the rest of the codebase appears to rely on the REST API or Lua layer for user-visible output rather than direct log calls.

## Constraints Observed

- Levels are integer constants 0–5 defined inside the implementation; there is no separate enum header. Adding a new level requires updating every `switch` in `logger.cpp` (desktop, Android, iOS branches) and the corresponding helper methods.
- The logger is intentionally a **singleton**; constructing `LoggerSingletone` directly is blocked by a private nested `Private` tag struct.
- The Boost.Log dependency is excluded on Android and iOS via preprocessor guards; on those platforms only the native loggers are used.
- Log file paths are resolved relative to `std::filesystem::current_path()` at construction time — changing cwd after initialization does not re-read the config.
- The logger itself does not perform thread-synchronization around the underlying Boost.Log stream; thread safety comes from Boost.Log's internal sinks and the singleton access guard.