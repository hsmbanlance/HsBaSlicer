---
kind: logging_system
name: Cross-Platform Singleton Logger (Boost.Log / Android logcat / iOS os_log)
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

## What system/approach is used

HsBaSlicer ships a small, self-contained logging library under `logger/` that exposes a thread-safe singleton (`LoggerSingletone`) with four user-facing levels: debug, info, warning, error. The implementation is platform-specific:

- **Desktop (Windows/macOS/Linux)**: uses Boost.Log (`boost/log/trivial.hpp`, `severity_logger`, `text_file_backend`, `console`).
- **Android**: delegates to the NDK `__android_log_print` API.
- **iOS**: delegates to Apple's `os_log` API via `<os/log.h>`.

There is no external configuration framework beyond a simple INI file; structured fields are not emitted — logs are plain text lines composed of timestamp + severity + source location + message.

## Key files and packages

- `logger/logger.hpp` — public API: `LoggerSingletone` class, static `Log*` helpers, and fluent user-defined literals `_log_debug` / `_log_info` / `_log_warning` / `_log_error` in the inline namespace `LogLiterals`.
- `logger/logger.cpp` — backend dispatch, Boost.Log initialization, Android/iOS backends, config parsing, rotation constants (`SIZE_1KB`, `SIZE_1MB`, `SIZE_50MB`).
- `utils/logcfg.ini` — default INI consumed by `Config::from_ini` from `fileoperator/rw_ptree.hpp`.
- `docs/en/logger/` and `docs/zh/logger/` — bilingual Doxygen documentation for the logger module.

## Architecture and conventions

### Levels and numeric mapping
Levels are integers 0–5 mapped uniformly across all three backends:

| int | desktop (Boost) | Android | iOS |
|-----|----------------|---------|-----|
| 0   | trace          | VERBOSE | DEBUG |
| 1   | debug          | DEBUG   | DEBUG |
| 2   | info           | INFO    | INFO |
| 3   | warning        | WARN    | DEFAULT |
| 4   | error          | ERROR   | ERROR |
| 5   | fatal          | FATAL   | FAULT |

Unknown values fall through to `debug` in `_DEBUG` builds and `warning` / `DEFAULT` in release builds.

### Singleton lifecycle
`LoggerSingletone` is constructed lazily via `GetInstance()` using `std::call_once` + `instance_flag_`. Construction reads `logcfg.ini` from `std::filesystem::current_path() + "/logcfg.ini"`; if the file is absent or a key is missing, defaults apply:

- `log_level` = 1 in `_DEBUG`, 3 otherwise.
- `use_log_file` = false.
- `log_path` = `<cwd>/log/log.txt`.
- `log_datatime_format_` = `%Y-%m-%d %H:%M:%S`.

On desktop, Boost.Log sinks are set up only when `use_log_file_` is true (file sink with 50 MB rotation + daily rollover at midnight) plus a console sink. A global filter `severity >= log_level` is applied.

### Source location
Every log call captures `std::source_location::current()` and prefixes the message with `[file:line] function_name: `. This is done via the internal helper `GetSourceLocation`.

### Fluent literal API
The inline namespace `LogLiterals` provides `"message"_log_debug()`, etc., which construct a `LogState` object whose destructor invokes `LoggerSingletone::Log(message_, level, location)`. Usage observed in the codebase is limited to `HsBaSlicer/HsBaSlicer.cpp`.

### Thread safety
All public methods acquire a `std::shared_lock(mutex_)` before accessing shared state; the singleton itself is guarded by `std::once_flag`.

## Conventions and constraints

- **Level convention**: callers use either the typed helpers `LoggerSingletone::LogDebug/Info/Warning/Error` or the `_log_*` literals; the raw `Log(message, level)` path is reserved for internal dispatch. Level 0 (trace) and 5 (fatal) exist in the enum but have no dedicated helper — they are reached only via the integer switch.
- **Configuration file**: `logcfg.ini` must live next to the executable; it is parsed with `Config::from_ini` and keys expected are `log.log_level`, `log.log_level_debug`, `log.use_log_file`, `log.log_file`, and `log_format.log_datatime_format`. Missing keys trigger the default branch that falls back to no-file logging.
- **Default behavior**: in release builds the effective threshold is `warning` (level 3); in debug builds it is `debug` (level 1). File logging is disabled by default.
- **Backend selection**: compilation-time macros `__ANDROID__` and `TARGET_OS_IOS` select the backend unconditionally — there is no runtime switch between them.
- **Structured fields**: none. Log output is a single formatted line; no key-value pairs or JSON payloads are produced.
- **Rotation policy** (desktop): 50 MB per file and daily rotation at 00:00 via `rotation_at_time_point(0, 0, 0)`.
- **Documentation scope**: the logger module is documented in both English and Chinese under `docs/en/logger/` and `docs/zh/logger/`, matching the project's bilingual doc convention.