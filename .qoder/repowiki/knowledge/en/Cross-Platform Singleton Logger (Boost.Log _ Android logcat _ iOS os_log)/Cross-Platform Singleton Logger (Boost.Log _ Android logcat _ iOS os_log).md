---
kind: logging_system
name: Cross-Platform Singleton Logger (Boost.Log / Android logcat / iOS os_log)
category: logging_system
scope:
    - '**'
source_files:
    - logger/logger.hpp
    - logger/logger.cpp
    - logger/CMakeLists.txt
    - logger/export.h
    - utils/logcfg.ini
---

## What system/approach is used

HsBaSlicer ships a **home-grown, cross-platform singleton logger** in the `logger/` module (`LoggerSingletone`, namespace `HsBa::Slicer::Log`). It is a thin wrapper around three platform backends:

| Platform | Backend | Sink(s) |
|---|---|---|
| Desktop (Windows/Linux/macOS) | Boost.Log (`boost/log/trivial.hpp`) | Console sink + optional rotating file sink |
| Android | NDK `__android_log_print` | Kernel logcat (`ANDROID_LOG_*`) |
| iOS | Apple `os_log` | System unified logging (`OS_LOG_TYPE_*`) |

The logger is built as either a static or shared library (`HsBaSlicerLog`) via `logger/CMakeLists.txt`; on non-Android/iOS platforms it links `Boost::log_setup` and `Boost::log`, while Android additionally links the `log` ndk library. The `base/singleton.hpp` helper is included but the implementation uses its own `std::shared_mutex` + `std::once_flag` double-checked-lock pattern.

## Key files and packages

- `logger/logger.hpp` — public API: `LoggerSingletone` class with `LogDebug/LogInfo/LogWarning/LogError` plus a fluent literal API (`"msg"_log_info()` etc.) under inline namespace `LogLiterals`.
- `logger/logger.cpp` — backend dispatch, config loading, format string, sinks setup.
- `logger/CMakeLists.txt` — target definition and per-platform link deps.
- `logger/export.h` — `HSBA_SLICER_LOG_API` export macro for Windows DLL visibility.
- `utils/logcfg.ini` — default configuration file consumed by the logger at startup.
- Consumers: `HsBaSlicer/HsBaSlicer.cpp` and every sample's `main.cpp` call `LoggerSingletone::LogInfo` to print startup messages and progress.

## Architecture and conventions

### Level model
Levels are plain integers 0–5, mapped uniformly across all three backends:

| Integer | Meaning | Boost.Log | Android | iOS |
|---|---|---|---|---|
| 0 | trace | `trace` | `VERBOSE` | `DEBUG` |
| 1 | debug | `debug` | `DEBUG` | `DEBUG` |
| 2 | info | `info` | `INFO` | `INFO` |
| 3 | warning | `warning` | `WARN` | `DEFAULT` |
| 4 | error | `error` | `ERROR` | `ERROR` |
| 5 | fatal | `fatal` | `FATAL` | `FAULT` |

Unknown levels fall through to `_DEBUG`-dependent defaults (`debug` in debug builds, `warning`/`DEFAULT` otherwise).

### Configuration
On construction the logger reads `<current_path>/logcfg.ini` via `Config::from_ini` (from `fileoperator/rw_ptree.hpp`). Keys:

- `[log]` section: `log_level` (release), `log_level_debug` (debug build), `use_log_file`, `log_file` (relative path appended to current dir), `[log_format]` section: `log_datatime_format`.
- Missing file or missing keys → fallback: `log_path = <current_dir>/log/log.txt`, `use_log_file = false`, datetime format `%Y-%m-%d %H:%M:%S`.
- Release default level is 3 (warning); debug default is 1 (debug).

### Output format
Desktop Boost.Log formatter:
```
[YYYY-MM-DD HH:MM:SS] <LEVEL> [file.cpp:line] function_name: message
```
Console and file sinks share the same formatter; file rotation is set to 50 MB and daily rollover (`rotation_at_time_point(0,0,0)`). On Android and iOS the logger prints a one-time initialization notice and then forwards each message directly to the native sink.

### Source location
Every log site captures `std::source_location` (C++20) automatically via the default parameter `location = std::source_location::current()`. A private helper `GetSourceLocation` formats it as `[filename.cpp:line] function_name: `. This prefix is prepended to every message before it reaches the backend.

### Fluent literal API
Inline namespace `LogLiterals` exposes user-defined literals:
```cpp
using namespace HsBa::Slicer::Log::LogLiterals;
"initializing pipeline"_log_info();
"something went wrong"_log_error();
```
Each literal constructs a temporary `LogState` whose destructor calls `LoggerSingletone::Log(message_, level, location)`.

### Thread safety
`GetInstance()` uses `std::call_once` over a static `instance_flag_` / `instance_` pair. Read-only accessors like `UseLogFile()` lock a static `std::shared_mutex mutex_`. Construction of the instance is lazy: any `Log*` entry point that sees a null `instance_` triggers `CreateInstance` inside `call_once`.

### Structured fields
There is **no structured-field framework**. Messages are concatenated strings; source location is embedded as a textual prefix rather than emitted as a structured attribute. No JSON/YAML log records are produced by this logger.

## Conventions and constraints

- **Single entry point**: All logging goes through `HsBa::Slicer::Log::LoggerSingletone` (or the `*_log_*` literals). Direct use of `printf`/`std::cout` for diagnostics is not part of the documented logging surface.
- **Level numbers are the canonical representation**: Callers pass integer levels (0–5) or use the typed helpers/literals; there is no enum type exposed publicly.
- **Configuration file name and location are fixed**: `<working_dir>/logcfg.ini` with the exact key names shown above; changing them requires modifying both `logger.cpp` and `utils/logcfg.ini`.
- **Backend selection is compile-time only**: `#if !defined(__ANDROID__) && !(defined(TARGET_OS_IOS) && TARGET_OS_IOS)` gates the Boost.Log path; Android uses `__android_log_print`; iOS uses `os_log`. There is no runtime switch between backends.
- **File logging is disabled by default** in release builds (`use_log_file = false` unless overridden in `logcfg.ini`). When enabled, logs go to `<current_dir>/<log_file>` with 50 MB rotation and midnight rollover.
- **Export boundary**: The logger is exported via `HSBA_SLICER_LOG_API` from the `HsBaSlicerLog` library; consumers must link against it (samples and the main `HsBaSlicer` executable do so).
- **No async sink**: All sinks are synchronous; the logger does not use an internal worker thread or queue.