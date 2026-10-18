---
kind: logging_system
name: Cross-Platform Logger Library (Boost.Log / Android logcat / iOS os_log)
category: logging_system
scope:
    - '**'
source_files:
    - logger/logger.hpp
    - logger/logger.cpp
    - logger/CMakeLists.txt
    - logger/export.h
    - utils/logcfg.ini
    - HsBaSlicer/HsBaSlicer.cpp
---

## What system/approach is used

HsBaSlicer ships a dedicated `logger/` library (`HsBaSlicerLog`) that provides a thread-safe singleton logger with six integer levels (0–5: trace/debug/info/warning/error/fatal). The backend is selected at compile time:

- **Desktop / Windows**: Boost.Log (`boost::log::trivial`, file + console sinks, rotation at 50 MB and midnight).
- **Android**: NDK `__android_log_print` with `ANDROID_LOG_*` priorities.
- **iOS**: Apple `os_log` / `os_log_with_type` via `<os/log.h>`.

Configuration is loaded from an INI file named `logcfg.ini` located next to the process working directory, using the in-repo `utils/logcfg.ini` as the shipped default. The logger also exposes fluent user-defined literals (`"msg"_log_debug()`, `_log_info`, `_log_warning`, `_log_error`) through an inline namespace `LogLiterals`.

## Key files and packages

- `logger/logger.hpp` — public API: `LoggerSingletone` class, static levelled methods (`LogDebug`/`LogInfo`/`LogWarning`/`LogError`), `GetInstance()`/`CreateInstance()`, `UseLogFile()`, and the `LogState` + literal helpers.
- `logger/logger.cpp` — implementation: per-platform backends, config parsing, source-location formatting, sink setup, and the central `Log(message, log_lv, location)` switch dispatching to the right backend.
- `logger/CMakeLists.txt` — builds `HsBaSlicerLog` as static or shared; links `Boost::log` on desktop, `log` (NDK) on Android, nothing extra on iOS.
- `logger/export.h` — exports the `HSBA_SLICER_LOG_API` macro for the shared build.
- `utils/logcfg.ini` — default configuration template consumed by `Config::from_ini`.
- `base/singleton.hpp` — base singleton helper consumed by the logger's own singleton pattern.
- Consumer entry points such as `HsBaSlicer/HsBaSlicer.cpp` and each sample under `samples/*/main.cpp` initialize the logger via `LoggerSingletone::GetInstance()` and use the `_log_*` literals.

## Architecture and conventions

1. **Singleton with `std::call_once`** — `instance_` is lazily created through `GetInstance()` / `CreateInstance()` guarded by `instance_flag_`; every public logging method re-checks `!instance_` and triggers construction if needed.
2. **Integer log-level enum** — Levels are plain `int` values 0–5 mapped uniformly across all three backends via local helpers `GetLogLevel` (Boost), `GetAndroidLogPriority`, and `GetiOSLogType`. Unknown values fall back to `debug` in `_DEBUG` builds and `warning` / `default` otherwise.
3. **Source location capture** — Every call site captures `std::source_location::current()` (C++20); the logger prefixes messages with `[file:line] function_name: ` via `GetSourceLocation`.
4. **Per-backend output format** — Desktop logs go through Boost.Log formatters writing `TimeStamp <severity> message\n`; Android/iOS emit the same `[file:line] func: msg` prefix directly into their native log streams.
5. **File vs. no-file mode** — `use_log_file_` toggles the Boost file sink; on Android/iOS `UseLogFile()` unconditionally returns true so callers can treat those platforms as always "logging".
6. **Fluent literal syntax** — `using namespace HsBa::Slicer::Log::LogLiterals;` enables expressions like `"initialize completed"_log_info();` which construct a `LogState` whose destructor invokes `LoggerSingletone::Log`.
7. **Config-driven defaults** — Constructor reads `logcfg.ini` via `Config::from_ini` (from `fileoperator/rw_ptree.hpp`). Keys:
   - `[log] log_level` (release default 3 = warning), `log_level_debug` (debug default 1 = debug), `use_log_file`, `log_file` (appended to current path),
   - `[log_format] log_datatime_format` (default `%Y-%m-%d %H:%M:%S`).
   Missing keys fall back to defaults without crashing.
8. **Thread safety** — `static std::shared_mutex mutex_` guards access to the instance pointer; `std::once_flag` ensures one-time initialization.

## Conventions and constraints

- Log levels are passed as integers 0–5; there is no separate enum type exposed to callers — consumers call `LogDebug`/`LogInfo`/`LogWarning`/`LogError` or the generic `Log(msg, level)`.
- All public logging APIs accept `std::string_view` plus an optional `std::source_location` defaulting to `std::source_location::current()`.
- On non-desktop platforms the Boost.Log headers are excluded via `#if !defined(__ANDROID__) && !(defined(TARGET_OS_IOS) && TARGET_OS_IOS)`, so the logger compiles without Boost on Android/iOS.
- The logger depends on `HsBaSlicerBase` (for `singleton.hpp`) and `HsBaSlicerFileOperator` (for `Config::from_ini`); it does not depend on any other slicer subsystem.
- Consumers should obtain the logger through `LoggerSingletone::GetInstance()` rather than constructing it directly (the constructor takes a private `Private` tag).
- The repository-wide usage observed in `HsBaSlicer/HsBaSlicer.cpp` and the samples follows the pattern:
  ```cpp
  using HsBa::Slicer::Log::LoggerSingletone;
  auto log = LoggerSingletone::GetInstance();
  using namespace HsBa::Slicer::Log::LogLiterals;
  "message"_log_info();
  ```
- There is no structured-field framework (no JSON/XML log records); log entries are flat strings composed of the source-location prefix and the user message.