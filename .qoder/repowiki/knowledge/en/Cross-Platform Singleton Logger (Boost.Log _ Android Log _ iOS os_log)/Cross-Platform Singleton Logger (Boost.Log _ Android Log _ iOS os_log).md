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

HsBaSlicer ships a single, repository-wide logging subsystem in the `logger/` directory. It is a thread-safe C++ singleton (`LoggerSingletone`) that abstracts three platform backends behind one API:

| Platform | Backend | Sink(s) |
|---|---|---|
| Desktop (Windows/macOS/Linux) | Boost.Log (`boost/log/trivial.hpp`, `severity_logger`, `text_file_backend`, `console`) | Console + optional rotating file |
| Android | NDK `__android_log_print` | Kernel log (`logcat`) |
| iOS | Apple `os_log` | System console log |

The logger is exposed via the `HSBA_SLICER_LOG_API` export macro and lives under namespace `HsBa::Slicer::Log`. All other modules call it through this facade — there is no direct use of Boost.Log, `__android_log_print`, or `os_log` outside `logger/logger.cpp`.

## Key Files

- `logger/logger.hpp` — public API: `LoggerSingletone` singleton, static levelled methods (`LogDebug`/`LogInfo`/`LogWarning`/`LogError`), fluent user-defined literals `_log_debug` / `_log_info` / `_log_warning` / `_log_error`, and `UseLogFile()`.
- `logger/logger.cpp` — backend dispatch, configuration loading, sink setup.
- `logger/export.h` — `HSBA_SLICER_LOG_API` visibility macro.
- `utils/logcfg.ini` — default runtime configuration shipped with the binary.

## Architecture & Conventions

### Singleton lifecycle
- Construction is private; instances are created via `CreateInstance()` and retrieved through `GetInstance()`, which uses `std::call_once` + `once_flag_` for one-time initialization.
- A `std::shared_mutex mutex_` guards read access to `use_log_file_` in `UseLogFile()`.
- The constructor lazily creates the instance if no prior call has done so (each `Log*` method checks `instance_` and triggers `call_once`).

### Log levels
A numeric integer scale is used internally and mapped per-backend:

| Numeric | Desktop (Boost) | Android | iOS |
|---|---|---|---|
| 0 | trace | VERBOSE | DEBUG |
| 1 | debug | DEBUG | DEBUG |
| 2 | info | INFO | INFO |
| 3 | warning | WARN | DEFAULT |
| 4 | error | ERROR | ERROR |
| 5 | fatal | FATAL | FAULT |

Unknown values fall back to `debug` in `_DEBUG` builds and `warning` / `DEFAULT` in release builds.

### Source location enrichment
Every log line is prefixed by `GetSourceLocation(location)`, which formats as `[file:line] function_name: `. The source location is captured automatically via `std::source_location::current()` on every call site (static methods and the `_log_*` literals both forward it).

### Configuration
On construction the logger reads `<cwd>/logcfg.ini` using `Config::from_ini` (from `fileoperator/rw_ptree.hpp`). Keys:

- `[log] log_level` — release threshold (default 3 = warning)
- `[log] log_level_debug` — debug threshold (default 1 = debug)
- `[log] use_log_file` — enable file sink
- `[log] log_file` — relative path appended to cwd (default `/log/log.txt`)
- `[log_format] log_datatime_format` — strftime-style timestamp format (default `%Y-%m-%d %H:%M:%S`)

If the file is missing or any key lookup throws `ptree_bad_path`, defaults are applied (no file sink, timestamp format above).

### Sinks (desktop)
When enabled, Boost.Log is configured with:
- A **console** sink.
- An optional **file** sink with rotation at 50 MB (`SIZE_50MB`) and daily rollover (`rotation_at_time_point(0,0,0)`).
- A formatter: `TimeStamp <severity> message\n`, where `TimeStamp` is driven by `log_datatime_format_`.
- A global filter `severity >= log_level`.
- A `TimeStamp` attribute populated from `local_clock()`.

### Fluent literal API
An inline namespace `LogLiterals` exposes user-defined literals:
```cpp
"some message"_log_info();   // level 2
"some message"_log_warning(); // level 3
```
These return a `LogState` object whose destructor invokes `LoggerSingletone::Log(message_, log_lv_, location)`.

### Usage pattern across the codebase
Call sites import the singleton via `using HsBa::Slicer::Log::LoggerSingletone;` and either:
- Call static helpers directly: `LoggerSingletone::LogInfo(msg);`
- Use the fluent literals after `using namespace HsBa::Slicer::Log::LogLiterals;`: `"progress"_log_info();`

Observed consumers include `HsBaSlicer/HsBaSlicer.cpp` (the main executable) and all sample entry points under `samples/*/main.cpp` (FDM, SLA, LOM, SLM, SLS, TDP, WAAM, Custom). No usage was found in the REST API module (`restapi/`) — the HTTP server does not appear to emit logs through this facade.

## Conventions & Constraints

- **Single entry point**: All logging goes through `HsBa::Slicer::Log`; external modules do not link against Boost.Log or call platform log APIs directly.
- **Integer-level API**: Public methods take an `int` log level (0–5); the mapping to backend-specific enums is centralized in `GetLogLevel` / `GetAndroidLogPriority` / `GetiOSLogType`.
- **Source-location capture**: Every call site captures `std::source_location::current()` automatically — callers never pass file/line/function manually.
- **Default thresholds**: Debug builds default to level 1 (debug); release builds default to level 3 (warning).
- **File sink disabled by default**: `use_log_file` defaults to `false`; when enabled, files rotate at 50 MB and midnight.
- **Thread safety**: Singleton creation is guarded by `std::call_once`; `UseLogFile()` acquires a `std::shared_lock(mutex_)` before reading the flag.
- **Configuration fallback**: Missing or malformed `logcfg.ini` silently falls back to built-in defaults without propagating errors.
- **No structured fields**: Log output is unstructured text (`[file:line] func: msg`); there are no JSON/YAML log records or named fields beyond the fixed timestamp/severity/message layout.