---
kind: configuration_system
name: Application Configuration System — INI/Property-Tree Logger Config and AppConfig Singleton
category: configuration_system
scope:
    - '**'
source_files:
    - utils/app_config.hpp
    - utils/app_config.cpp
    - logger/logger.cpp
    - utils/logcfg.ini
    - fileoperator/rw_ptree.hpp
    - tests/AppConfig/app_config_test.cpp
---

## Overview

HsBaSlicer has no centralized application-wide configuration framework (no env-var loader, no feature-flag service, no secrets manager). Runtime configuration is split across two small, independent subsystems:

1. **Logger configuration** — loaded from an INI file at process startup.
2. **AppConfig singleton** — a thread-safe in-memory store for app-level settings (currently only the path to the external 7-Zip executable).

There is no unified config API that other modules consume; each subsystem owns its own configuration loading.

## Key Files

- `utils/app_config.hpp`, `utils/app_config.cpp` — `AppConfigSingletone` class.
- `logger/logger.cpp` — `LoggerSingletone` constructor reads `logcfg.ini` via Boost.PropertyTree.
- `utils/logcfg.ini` — default logger configuration shipped with the repo.
- `fileoperator/rw_ptree.hpp` / `fileoperator/rw_ptree.cpp` — `Config::from_ini()` helper used by the logger.
- `tests/AppConfig/app_config_test.cpp` — unit tests for the AppConfig singleton.

## Architecture and Conventions

### Logger configuration (`logcfg.ini`)

The logger loads its configuration from `<current working directory>/logcfg.ini` using `Config::from_ini(cfg_path)` (Boost.PropertyTree-based INI reader in `fileoperator/rw_ptree.hpp`).

Relevant keys (defined in `utils/logcfg.ini`):

| Section | Key | Type | Meaning |
|---|---|---:|---|
| `log` | `log_level` | int (0–5) | Production log level (trace=0 … fatal=5) |
| `log` | `log_level_debug` | int | Debug-build override of `log_level` |
| `log` | `use_log_file` | bool | Whether to enable file logging |
| `log` | `log_file` | string | Relative path appended to CWD |
| `log_format` | `log_datatime_format` | string | Boost.Date_Time format string |
| `log_format` | `log_time_format` | string | Time-only format string |

Behavior in `LoggerSingletone` constructor (`logger/logger.cpp` lines 147–183):

- If `logcfg.ini` does not exist or any key lookup throws `ptree_bad_path`, defaults are applied: `log_path = "<cwd>/log/log.txt"`, `use_log_file_ = false`, `log_datatime_format_ = "%Y-%m-%d %H:%M:%S"`.
- In `_DEBUG` builds, `log_level_` starts at `1` (debug); otherwise it starts at `3` (warning).
- The selected backend depends on the target platform:
  - Desktop (non-Android, non-iOS): Boost.Log with console + optional rotating file sink (50 MB rotation, midnight reset).
  - Android: `__android_log_print` with priority mapped from the same 0–5 scale.
  - iOS: `os_log` with type mapped from the same 0–5 scale.

### AppConfig singleton (`AppConfigSingletone`)

A minimal, lazily-initialized, mutex-guarded singleton in namespace `HsBa::Slicer`:

```cpp
class AppConfigSingletone {
public:
    static AppConfigSingletone& GetInstance();
    static void DeleteInstance();
    std::string GetSevenZPath() const;
private:
    static std::shared_mutex mutex_;
    static AppConfigSingletone* instance_;
    std::string sevenZ_path_;
};
```

- Construction is double-checked locking guarded by `std::shared_mutex` (`GetInstance` takes `unique_lock` for write, `GetSevenZPath` takes `shared_lock`).
- `DeleteInstance` resets the raw pointer so the next `GetInstance` re-allocates — tested explicitly in `tests/AppConfig/app_config_test.cpp`.
- Default value of `sevenZ_path_` is empty string (asserted by the test).
- No persistence layer is implemented yet; values are set programmatically by callers (none found in the current codebase beyond the accessor).

### Serialization helpers used by configuration

Configuration files are parsed through `fileoperator/rw_ptree.hpp` which exposes `Config::from_ini(path)` returning a `boost::property_tree::ptree`. This is the only config-file parser used in the repository; there is no YAML/TOML/env-loader for application settings.

## Conventions and Constraints

- **No environment-variable based configuration**: no `.env` files, no `getenv` usage for application settings was observed.
- **No feature flags or runtime toggles**: the only boolean runtime switch is `use_log_file_` inside the logger.
- **No secrets management**: no encrypted config files, no secret vault integration; paths and log levels are plain text.
- **INI is the only persistent config format** used at runtime (for logger settings).
- **Default fallbacks are built into constructors**: both `LoggerSingletone` and `AppConfigSingletone` provide safe defaults when their respective configuration sources are missing or malformed.
- **Thread safety model**: both singletons use `std::shared_mutex` (readers get shared lock, writers get exclusive lock) plus a separate `std::once_flag` in the logger's `GetInstance`.
- **Platform-specific behavior is gated by preprocessor conditionals** (`__ANDROID__`, `TARGET_OS_IOS`, `_WIN32`) rather than by configuration files.
- **Tests assert singleton identity and idempotent deletion**, but there is no test coverage for actual config-file parsing (the INI file exists as a default artifact under `utils/logcfg.ini`).

## What Is Not Present

- No `.env` loader, no `dotenv`-style mechanism.
- No JSON/YAML/TOML application configuration schema (the `struct_json.hpp` / `struct_yaml.hpp` / `struct_xml.hpp` utilities in `utils/` are used for data serialization, not for application config).
- No protobuf-based runtime configuration (protobuf schemas under `proto/` describe pipeline messages, not app settings).
- No command-line argument parsing for configuration overrides.