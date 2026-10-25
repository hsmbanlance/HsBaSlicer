---
kind: configuration_system
name: Configuration System — INI-based Runtime Config, App Singleton & CMake Presets
category: configuration_system
scope:
    - '**'
source_files:
    - restapi/server_config.hpp
    - restapi/server_config.cpp
    - restapi/restapi.ini
    - restapi/main.cpp
    - utils/app_config.hpp
    - utils/app_config.cpp
    - logger/logger.cpp
    - utils/logcfg.ini
    - CMakePresets.json
---

## Overview

HsBaSlicer has no unified configuration framework. Instead it uses three independent mechanisms that all follow the same pattern: a small struct holding defaults, an `ini` file with human-readable keys, and a loader that falls back to built-in defaults when the file is missing.

| Concern | Mechanism | File(s) |
|---|---|---|
| REST server runtime config | `ServerConfig` struct + `LoadServerConfig()` | `restapi/server_config.{hpp,cpp}`, `restapi/restapi.ini`, `restapi/main.cpp` |
| Application-wide settings (e.g. external tool paths) | `AppConfigSingletone` singleton | `utils/app_config.{hpp,cpp}` |
| Logging subsystem config | `LoggerSingletone` constructor reads `logcfg.ini` | `logger/logger.cpp`, `utils/logcfg.ini` |
| Build / cross-platform presets | CMakePresets.json | `CMakePresets.json` |

There is no `.env` support, no environment-variable override layer, no feature-flag registry, and no centralized secret store in the codebase.

## REST Server Configuration (`restapi/`)

`ServerConfig` (`restapi/server_config.hpp`) is a plain struct whose members carry built-in defaults:
- `[server]` section: `host`, `port`, `default_format`, `max_concurrent_jobs`, `max_jobs`, `result_ttl_seconds`
- `[paramstore]` section: `backend` (sqlite/mysql/postgresql), `sqlite_path`, `host/user/password/database/port`, `crypto_password`
- `[auth]` section exists in `restapi.ini` but is documented as "Reserved for a future release. Not read by the current implementation."

`LoadServerConfig(path)` (`restapi/server_config.cpp`):
1. Returns a default-initialized `ServerConfig` if the path does not exist (missing file is not an error).
2. Reads via `Config::from_ini(path)` (a `boost::property_tree::ptree` wrapper in `fileoperator/rw_ptree.hpp`).
3. Normalizes values defensively: port clamped to `(0,65535]`, `default_format` forced to `"json"` unless exactly `"xml"`, `max_concurrent_jobs`/`max_jobs` floored at 1, `result_ttl_seconds` floored at 0.
4. Throws `boost::property_tree::file_parser_error` on malformed files (documented in the header).

CLI overrides in `restapi/main.cpp` take precedence over the ini file: `--config <path>` selects the ini, `--host <addr>` and `--port <n>` overwrite the parsed values before calling `RunServer(cfg)`.

The default ini shipped with the repo is `restapi/restapi.ini`; its comments state the server looks for it in its working directory and can be overridden with `--config`.

## Application Configuration Singleton (`utils/app_config.*`)

`AppConfigSingletone` (`utils/app_config.hpp/.cpp`) is a thread-safe global exposing `GetSevenZPath()`. It uses a double-checked-locking pattern guarded by `std::shared_mutex` and exposes `DeleteInstance()` for tests. The current implementation only holds a `sevenZ_path_` string member; there is no loader or ini file attached to it yet — the field is initialized empty and returned as-is.

## Logging Configuration (`logger/logger.cpp`)

`LoggerSingletone`'s constructor auto-detects `logcfg.ini` relative to `std::filesystem::current_path()`:
- In `_DEBUG` builds it reads `log.log_level_debug`; in release it reads `log.log_level`.
- Also reads `log.use_log_file`, `log.log_file`, `log_format.log_datatime_format`.
- If the file is absent or any key is missing, it falls back to: log level 3 (release) or 1 (debug), no file logging, format `%Y-%m-%d %H:%M:%S`, output path `<cwd>/log/log.txt`.
- On non-Windows/non-iOS platforms this configures Boost.Log with console + optional rotating file sink (50 MB, daily rotation). On Android/iOS it routes through `__android_log_print` / `os_log` instead.

The default `utils/logcfg.ini` ships with `log_level = 3`, `use_log_file = false`.

## Build-Time Configuration (`CMakePresets.json`)

Build-time configuration is centralized in `CMakePresets.json` using CMake's preset system:
- `configurePresets`: per-host variants (`windows-base`, `linux-debug/release`, `macos-debug/release`, `ios-release`, `android-release`) selecting generator, architecture, toolchain (`vcpkg.cmake`), and cache variables like `CMAKE_BUILD_TYPE`, `HSBA_COVERAGE`, `VCPKG_TARGET_TRIPLET`, `ANDROID_NDK`, etc.
- `buildPresets` / `testPresets`: convenience wrappers around configure presets (e.g. `linux-coverage` runs tests then the `coverage` target).
- Coverage presets pass `HSBA_COVERAGE=ON`, `HSBA_COVERAGE_THRESHOLD_CORE=45`, `HSBA_COVERAGE_THRESHOLD_OTHER=25`.

## Conventions Observed

- **INI-first, defaults-always**: every runtime config type defines sensible built-in defaults so the application starts without any config file. Missing files are treated as "use defaults", not errors.
- **Defensive normalization after parsing**: numeric/string fields are validated and clamped after loading from ini (see `LoadServerConfig` port/format/job-count checks).
- **Per-component config structs**: each subsystem owns its own config type and loader (`ServerConfig` in `restapi/`, logger config inside `LoggerSingletone`); there is no shared config base class.
- **Singleton access pattern**: both `AppConfigSingletone` and `LoggerSingletone` expose a static `GetInstance()` accessor; `LoggerSingletone` additionally uses `std::call_once` for thread-safe lazy initialization.
- **Ini schema documented inline**: `restapi.ini` and `logcfg.ini` contain comments describing every key and its effect.
- **No env-var layer**: neither `ServerConfig` nor `AppConfigSingletone` consult environment variables; CLI flags (`--config`, `--host`, `--port`) are the only override mechanism.
- **No secrets abstraction**: passwords (ParamStore `crypto_password`, MySQL/PG credentials) are stored as plain strings in the ini file with no encryption-at-rest beyond the optional ParamStore backend password.
- **Build vs runtime separation**: `CMakePresets.json` configures compilation; runtime behavior is exclusively driven by ini files and CLI args.