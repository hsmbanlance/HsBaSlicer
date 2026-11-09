---
kind: configuration_system
name: 'Configuration System: INI-driven Server Config + Process-wide AppConfig Singleton'
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
    - tests/AppConfig/app_config_test.cpp
    - tests/RestApi/auth_gate_test.cpp
    - tests/RestApi/upload_store_test.cpp
    - fileoperator/rw_ptree.hpp
---

## Overview

HsBaSlicer has two distinct runtime-configuration mechanisms, each scoped to a different subsystem:

1. **REST server config** — `restapi/server_config.{hpp,cpp}` loads an INI file (`restapi.ini`) into a plain `ServerConfig` struct with built-in defaults and defensive normalization.
2. **Process-wide app settings** — `utils/app_config.{hpp,cpp}` exposes a thread-safe singleton (`AppConfigSingletone`) for shared process-level settings (currently only the external 7-Zip path).

There is no unified configuration framework, no environment-variable loader, no feature-flag registry, and no YAML/TOML/JSON application config in the core library. Pipeline definitions themselves are encoded as Protocol Buffers under `proto/` and converted via `convert/Msg2PipelineConfig.cpp` / `convert/PipelineConfig2Msg.cpp`; those are data schemas, not runtime configuration files.

## Key Files

- `restapi/server_config.hpp` — declares `ServerConfig` struct and `LoadServerConfig(path)`.
- `restapi/server_config.cpp` — parses `restapi.ini` via `fileoperator/rw_ptree::from_ini`, normalizes values, returns defaults when the file is missing.
- `restapi/restapi.ini` — canonical example/default INI covering `[server]`, `[upload]`, `[paramstore]`, `[auth]`, `[rate_limit]` sections.
- `restapi/main.cpp` — CLI entry point; default config path is `restapi.ini`, overridable via `--config <file>`.
- `utils/app_config.hpp` / `utils/app_config.cpp` — `AppConfigSingletone` singleton exposing `GetSevenZPath()`.
- `tests/AppConfig/app_config_test.cpp` — verifies singleton identity, empty default, idempotent delete, and concurrent access.
- `tests/RestApi/auth_gate_test.cpp`, `tests/RestApi/upload_store_test.cpp` — drive `LoadServerConfig` against temporary INI fixtures.
- `fileoperator/rw_ptree.{hpp,cpp}` — thin wrapper around Boost.PropertyTree used by `LoadServerConfig`.

## Architecture & Conventions

### REST server config (`ServerConfig`)

- A single POD-like `struct ServerConfig` holds every setting, grouped by comment blocks matching INI sections (`/* [server] */`, `/* [auth] */`, `/* [upload] */`, `/* [paramstore] */`).
- Every member carries a C++ default value so a partially filled or entirely absent INI still yields a usable configuration — this is documented in the header comment and enforced by `LoadServerConfig` returning the default-initialized struct when the file does not exist.
- Parsing uses `boost::property_tree::ptree` through `Config::from_ini(path)` from `fileoperator/rw_ptree.hpp`. Missing keys fall back to the struct's default via `pt.get<T>("section.key", cfg.field)`.
- Boolean parsing accepts case-insensitive `1/true/yes/on` and `0/false/no/off`; unrecognized strings fall through to the default.
- After loading, values are defensively normalized:
  - `port` clamped to `(0, 65535]`, else reset to `8080`.
  - `default_format` forced to `json` unless exactly `xml`.
  - `max_concurrent_jobs`, `max_jobs` floored at `1`; `result_ttl_seconds` floored at `0`.
  - `upload_evict_mode` accepted values are `ttl | count | both | manual`; anything else falls back to `both`.
  - `upload_max_files` is floored at `HSBA_MODEL_POOL_SIZE` (defined in top-level `CMakeLists.txt`, defaulting to `1` if undefined) so the upload pool can always refill the in-process model pool.
  - `upload_ttl_seconds` clamped to `0..2592000` (30 days), `upload_max_file_mb` to `1..4096`, `upload_sweep_interval_seconds` to `5..3600`, `upload_max_request_mb` raised to at least `upload_max_file_mb + 4`.
- The `[auth]` section has special logic: if `enabled` is omitted or empty, auth is auto-enabled whenever a non-empty `token` is present; otherwise `ParseBool` decides.
- The `[paramstore]` section maps `backend` string to `HsBaParamStoreBackend` enum (`mysql`, `postgresql`/`postgres`, default `sqlite`); `paramstore_enabled` is true when backend is non-empty and not `none`.

### Process-wide app settings (`AppConfigSingletone`)

- Implements a classic double-checked locking singleton guarded by `std::shared_mutex`.
- Provides `GetInstance()`, `DeleteInstance()`, and `GetSevenZPath()`.
- Default `sevenZ_path_` is empty; callers must supply the path before use.
- Thread-safety is exercised by `tests/AppConfig/app_config_test.cpp`, which spawns 8 threads calling `GetInstance` concurrently and asserts they all receive the same pointer.

### Configuration sources

- **INI file** (`restapi.ini`) — primary source for REST server settings. Path is configurable via `--config <file>` on the CLI; default is `./restapi.ini`.
- **Environment variable** — `restapi.ini` documents that `HSBA_REST_AUTH_TOKEN` may be used to inject the auth token at runtime (read when no token is configured in the INI). This is the only env-var integration observed.
- **No other env vars, `.env` files, YAML, TOML, or JSON application configs** were found in the codebase.

### Relationship to ParamStore

`ServerConfig.paramstore_*` fields configure the `ParamStore` subsystem (`fileoperator/param_store.hpp`), which persists pipeline parameters to SQLite/MySQL/PostgreSQL via `SQL::ISQLAdapter`. At-rest encryption is controlled separately via `ParamStore::SetCryptoKey` (AES-256-CBC on the column map blob), independent of the INI `crypto_password` field.

## Conventions & Constraints

- **Missing INI is not an error**: `LoadServerConfig` returns the default-initialized `ServerConfig` when the file does not exist, allowing the server to start without any explicit config file.
- **All INI keys are optional**; defaults live in C++, not in the INI file itself. The INI serves as an override layer.
- **Numeric and enum settings are validated post-parse**, not rejected with exceptions — invalid values are coerced to safe defaults (e.g., unknown `evict_mode` → `both`, out-of-range port → `8080`).
- **Auth is opt-in but self-enforcing**: setting a token implicitly enables authorization when `enabled` is unset, preventing accidental exposure.
- **Upload pool size cannot go below `HSBA_MODEL_POOL_SIZE`**: enforced at parse time to guarantee the upload directory can refills the in-process model pool.
- **The singleton is intentionally mutable and manually managed**: `DeleteInstance` resets the static pointer, and tests assert that re-creation after deletion works and that repeated deletion is a no-op.
- **No centralized config registry**: `AppConfigSingletone` and `ServerConfig` are independent; there is no base class or factory unifying them.