---
kind: logging_system
name: Cross-Platform Singleton Logger (Boost.Log / Android Log / iOS os_log)
category: logging_system
scope:
    - '**'
source_files:
    - logger/logger.hpp
    - logger/logger.cpp
    - logger/CMakeLists.txt
    - utils/logcfg.ini
    - docs/en/logger/README.md
---

## Overview

HsBaSlicer ships a self-contained logging subsystem in the `logger/` module. It is a **thread-safe singleton** (`LoggerSingletone`) that wraps three platform backends behind one C++ API:

| Platform | Backend |
|---|---|
| Desktop / Linux / macOS (non-iOS) | Boost.Log (`boost::log`, `boost::log_setup`) with console + optional rotating file sink |
| Android | Native `__android_log_print` via `<android/log.h>` |
| iOS | Apple `os_log` / `os_log_with_type` |

The public API lives in `logger/logger.hpp` and `logger/logger.cpp`; configuration is read from an INI file at runtime.

## Key Files

- `logger/logger.hpp` — Public header: `LoggerSingletone` class, static levelled entry points (`LogDebug`/`LogInfo`/`LogWarning`/`LogError`), fluent user-defined literals `_log_debug` / `_log_info` / `_log_warning` / `_log_error` returning a `LogState` object.
- `logger/logger.cpp` — Implementation: per-platform backend dispatch, Boost.Log formatter, rotation policy, config parsing, source-location formatting.
- `logger/CMakeLists.txt` — Builds `HsBaSlicerLog` as STATIC or SHARED; links `Boost::log` on desktop, `log` on Android, nothing extra on iOS.
- `utils/logcfg.ini` — Default configuration shipped with the project.
- `docs/en/logger/README.md` — Module documentation referencing `LoggerSingletone` and `LogState`.

## Architecture & Conventions

### Level model
Levels are plain integers 0–5, mapped to both Boost.Log severity and the native platform equivalents:

| Integer | Boost.Log | Android | iOS | API surface |
|---|---|---|---|---|
| 0 | `trace` | `ANDROID_LOG_VERBOSE` | `OS_LOG_TYPE_DEBUG` | `_log_debug` |
| 1 | `debug` | `ANDROID_LOG_DEBUG` | `OS_LOG_TYPE_DEBUG` | `LogDebug` |
| 2 | `info` | `ANDROID_LOG_INFO` | `OS_LOG_TYPE_INFO` | `LogInfo` |
| 3 | `warning` | `ANDROID_LOG_WARN` | `OS_LOG_TYPE_DEFAULT` | `LogWarning` |
| 4 | `error` | `ANDROID_LOG_ERROR` | `OS_LOG_TYPE_ERROR` | `LogError` |
| 5 | `fatal` | `ANDROID_LOG_FATAL` | `OS_LOG_TYPE_FAULT` | — |

Unknown levels fall through to a default: `debug` in `_DEBUG`, `warning` otherwise.

### Source location
Every log call captures `std::source_location::current()` and prefixes the message with `[file:line] function_name: `. This is done uniformly for all three backends via the internal helper `GetSourceLocation`.

### Sink strategy
- **Console**: always enabled on desktop (Boost.Log console sink).
- **File**: optional, controlled by `use_log_file`. When enabled, the file sink rotates at 50 MB (`SIZE_50MB`) and midnight (`rotation_at_time_point(0, 0, 0)`), appending (`std::ios::app`).
- **Android/iOS**: no file sink; `UseLogFile()` returns `true` unconditionally on those platforms even though output goes through the system logger.

### Configuration
On construction the logger reads `<cwd>/logcfg.ini` via `Config::from_ini` (from `fileoperator/rw_ptree.hpp`). Keys:

- `[log].log_level` — release threshold (default 3 = warning)
- `[log].log_level_debug` — debug threshold (default 1 = debug)
- `[log].use_log_file` — enable file sink
- `[log].log_file` — relative path appended to cwd (default `/log/log.txt`)
- `[log_format].log_datatime_format` — strftime-style timestamp format (default `%Y-%m-%d %H:%M:%S`)

Missing keys raise `boost::property_tree::ptree_bad_path` and are silently replaced with defaults.

### Initialization order
The singleton is lazily created via `std::call_once` on first use of any logging API. If no explicit initialization has occurred, `Log*` triggers construction. The constructor itself parses `logcfg.ini` and sets up sinks; it does not require callers to invoke an explicit `Init()` method.

### Fluent literal syntax
Callers can write either:

```cpp
LoggerSingletone::LogInfo("pipeline started");
"pipeline started"_log_info();
```

Both paths converge on `LoggerSingletone::Log(message, level, location)`.

## Usage Across the Codebase

The logger is consumed primarily by top-level sample executables and the HsBaSlicer host process (e.g. `HsBaSlicer/HsBaSlicer.cpp`, `samples/*/main.cpp`), typically to emit progress messages like `[X%] stage_name`. Internal slicing-engine modules do not appear to call the logger directly in the sampled results; they rely on the host/samples layer for diagnostics.

## Constraints & Rules Observed

- Levels are integer constants 0–5; there is no enum type for levels in the public API — callers pass raw ints or use the provided helpers/literals.
- On non-desktop platforms (Android/iOS) the Boost.Log dependency is excluded at compile time via `#if !defined(__ANDROID__) && !(defined(TARGET_OS_IOS) && TARGET_OS_IOS)` guards around every Boost.Log include.
- The logger is exported as a shared library named `HsBaSlicerLog` on Windows (with `HSBA_SLICER_LOG_EXPORTS`); on other platforms it links against `Boost::log_setup` and `Boost::log`.
- File paths are resolved relative to `std::filesystem::current_path()` at construction time; the logger does not support absolute paths from the config without prepending cwd.
- The datetime format string is passed verbatim to Boost's `format_date_time<boost::posix_time::ptime>` — only formats accepted by `strftime`/`put_time` are valid.
- There is no structured-field mechanism (no JSON payloads, no key-value fields); log records are flat strings composed of `[file:line] func: message`.
- Thread safety is provided by a `std::shared_mutex` guarding `UseLogFile()` and a `std::once_flag` for singleton creation; the actual Boost.Log sinks handle their own concurrency.