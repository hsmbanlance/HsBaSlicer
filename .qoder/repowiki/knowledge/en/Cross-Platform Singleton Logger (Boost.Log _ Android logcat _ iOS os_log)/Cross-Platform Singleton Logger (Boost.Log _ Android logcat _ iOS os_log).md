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
    - docs/en/logger/logger_singletone.md
    - docs/en/logger/log_state.md
---

## What system/approach is used

HsBaSlicer ships a small, self-contained logging library under `logger/` that provides a thread-safe singleton with leveled output and three platform-specific backends:

- **Desktop (Windows/macOS/Linux)**: Boost.Log (`boost/log/trivial.hpp`, `text_file_backend`, `console`) configured at construction time.
- **Android**: NDK `__android_log_print` with priorities `VERBOSE/DEBUG/INFO/WARN/ERROR/FATAL`.
- **iOS**: Apple `os_log` / `os_log_with_type` with types `DEBUG/INFO/DEFAULT/ERROR/FAULT`.

The public API lives in namespace `HsBa::Slicer::Log`. Consumers call either static convenience methods on `LoggerSingletone` or the fluent user-defined-literal helpers in the inline sub-namespace `LogLiterals`.

## Key files and packages

- `logger/logger.hpp` — Public header: `LoggerSingletone` class, `LogState` fluent helper, `_log_debug/_log_info/_log_warning/_log_error` literals.
- `logger/logger.cpp` — Implementation: backend selection, config loading, sinks, level mapping.
- `logger/CMakeLists.txt` + `logger/export.h` — Library target and `HSBA_SLICER_LOG_API` export macro.
- `utils/logcfg.ini` — Default INI configuration consumed by `fileoperator/rw_ptree.hpp`.
- `docs/en/logger/README.md`, `logger_singletone.md`, `log_state.md` — Doxygen documentation for the logger.

Consumers are scattered across the codebase; representative usages include `HsBaSlicer/HsBaSlicer.cpp`, `samples/FDM/main.cpp`, `samples/Custom/main.cpp`, and `samples/LOM/main.cpp`.

## Architecture and conventions

### Levels
Levels are plain integers 0–5:

| int | desktop (Boost) | Android | iOS |
|-----|-----------------|---------|-----|
| 0   | trace           | VERBOSE | DEBUG |
| 1   | debug           | DEBUG   | DEBUG |
| 2   | info            | INFO    | INFO |
| 3   | warning         | WARN    | DEFAULT |
| 4   | error           | ERROR   | ERROR |
| 5   | fatal           | FATAL   | FAULT |

Unknown levels fall through to a default: `debug` in `_DEBUG` builds, `warning` otherwise.

### Backend dispatch
`LoggerSingletone::Log(message, log_lv, location)` switches on `log_lv` and routes each case to the active backend. The same switch appears once in `GetLogLevel`, `GetAndroidLogPriority`, and `GetiOSLogType`, plus again inside the per-level branches of `Log`.

### Source location
Every public logging method takes an optional `std::source_location` (defaulted via `std::source_location::current()`). A private helper `GetSourceLocation` formats it as `[file:line] function_name: ` and prepends it to every emitted message. This is the only structured field the logger emits beyond timestamp and severity.

### Singleton lifecycle
- Static members `instance_`, `mutex_`, `instance_flag_` live in `LoggerSingletone`.
- `GetInstance()` uses `std::call_once` over `CreateInstance()`, which constructs via the private-tag constructor `LoggerSingletone(Private)`.
- Each convenience method (`LogDebug`/`LogInfo`/`LogWarning`/`LogError`) also lazily initializes the instance if it is still null before delegating to `Log`.
- `UseLogFile()` wraps its read in a `std::shared_lock(mutex_)`.

### Configuration
On construction the logger looks for `<cwd>/logcfg.ini` via `Config::from_ini` (from `fileoperator/rw_ptree.hpp`). Keys read:

- `[log] log_level` — release threshold (int)
- `[log] log_level_debug` — debug build threshold (int)
- `[log] use_log_file` — enable file sink (bool)
- `[log] log_file` — relative path appended to cwd
- `[log_format] log_datatime_format` — strftime-style format string

If the file is missing or any key lookup throws `ptree_bad_path`, defaults are applied:
- `log_path_ = <cwd>/log/log.txt`
- `use_log_file_ = false`
- `log_datatime_format_ = "%Y-%m-%d %H:%M:%S"`
- Release `log_level_ = 3`; debug `log_level_ = 1`.

On desktop, when `use_log_file_` is true, Boost.Log adds a `text_file_backend` with rotation size `SIZE_50MB` (50 MiB) and daily rotation (`rotation_at_time_point(0,0,0)`), opened in append mode. A console sink is always added. Both sinks share one formatter:

```
[TimeStamp] <severity> [file:line] function_name: message\n
```

where `TimeStamp` is formatted with the configured `log_datatime_format`.

### Fluent literal API
The inline namespace `LogLiterals` exposes:

```cpp
using namespace HsBa::Slicer::Log::LogLiterals;
"some message"_log_info();
"some message"_log_debug();
"some message"_log_warning();
"some message"_log_error();
```

Each literal returns a `LogState` whose destructor invokes `LoggerSingletone::Log(message_, log_lv_, location)` — enabling RAII-style delayed emission.

### Platform-specific notes
- On Android, `UseLogFile()` unconditionally returns `true` even though no file sink is installed; the constructor prints a warning via `__android_log_print` that kernel log is being used.
- On iOS, `UseLogFile()` also returns `true`; logs go through `os_log_with_type` using a handle created via `os_log_create("HsBaSlicer", "Log")`.
- Boost.Log headers are conditionally included only when neither `__ANDROID__` nor `TARGET_OS_IOS` is defined.

## Conventions and constraints

- **Level encoding**: Callers pass integer levels 0–5; there is no enum. The mapping is enforced by the three `Get*LogType/Priority` functions and the `switch` in `LoggerSingletone::Log`.
- **Thread safety**: The singleton is initialized exactly once via `std::call_once` + `once_flag`; reads of `use_log_file_` are protected by `std::shared_mutex`.
- **Configuration source of truth**: `utils/logcfg.ini` is the documented default; consumers must not hard-code thresholds — the constructor loads from disk and falls back to built-in defaults.
- **Structured fields**: Only two fields are consistently emitted — a `TimeStamp` attribute (set via `add_global_attribute("TimeStamp", local_clock())`) and the source-location prefix produced by `GetSourceLocation`. No additional structured fields (e.g. module name, thread id) are attached.
- **Backends are mutually exclusive per build**: A single binary targets exactly one backend (desktop Boost.Log, Android logcat, or iOS os_log); there is no runtime toggle between them.
- **API surface**: All public symbols are marked with `HSBA_SLICER_LOG_API` (from `logger/export.h`), making the logger part of the exported C ABI surface.
- **Documentation**: The logger's usage is documented in `docs/en/logger/` (`logger_singletone.md`, `log_state.md`), mirroring the Chinese docs under `docs/zh/logger/`.