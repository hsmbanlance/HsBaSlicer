---
kind: configuration_system
name: 'Configuration System: INI/PropertyTree Server Config, AppConfig Singleton, and ParamStore Persistence'
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
    - fileoperator/param_store.hpp
    - fileoperator/sql_adapter.hpp
    - fileoperator/rw_ptree.hpp
    - utils/struct_json.hpp
---

## Overview

HsBaSlicer uses a small, layered configuration system with no external config framework:

1. **Process-level runtime settings** are loaded from an INI file (`restapi.ini`) into a typed `ServerConfig` struct via `boost::property_tree`, with built-in defaults and defensive normalization.
2. **Cross-cutting application settings** (e.g. external tool paths) are exposed through a thread-safe singleton `AppConfigSingletone` in `utils/app_config.*`.
3. **Persisted process parameters** (pipeline configs) are stored via `ParamStore` in `fileoperator/`, backed by SQLite / MySQL / PostgreSQL through `ISQLAdapter`, with optional AES-256-CBC at-rest encryption.
4. **Serialization helpers** for reflectable/aggregates types live in `utils/struct_json.hpp` (RapidJSON), `utils/struct_xml.hpp`, `utils/struct_yaml.hpp`, plus generic INI/XML/JSON property-tree I/O in `fileoperator/rw_ptree.hpp`.

There is no `.env` loader, no environment-variable override layer, and no feature-flag subsystem — configuration is file + singleton + database.

## Key Files

- `restapi/server_config.hpp` / `restapi/server_config.cpp` — `ServerConfig` struct and `LoadServerConfig(path)` that reads `restapi.ini`.
- `restapi/restapi.ini` — default INI with `[server]` and `[paramstore]` sections; documented as the authoritative source of defaults.
- `restapi/main.cpp` — CLI entry point parses `--config <file>` / `-c`, defaults to `./restapi.ini`, then calls `LoadServerConfig`.
- `utils/app_config.hpp` / `utils/app_config.cpp` — `AppConfigSingletone` singleton exposing `GetSevenZPath()`; guarded by `std::shared_mutex`.
- `fileoperator/param_store.hpp` / `fileoperator/param_store.cpp` — `ParamStore` class providing CRUD over pipeline configs with schema migration hooks and crypto key support.
- `fileoperator/sql_adapter.hpp` — `Backend` enum (`HSBA_PARAM_BACKEND_SQLITE`, `HSBA_PARAM_BACKEND_MYSQL`, `HSBA_PARAM_BACKEND_POSTGRESQL`) and `ISQLAdapter` interface.
- `fileoperator/rw_ptree.hpp` — `Config::from_ini/from_json/from_xml` and `to_ini/to_json/to_xml` wrappers around `boost::property_tree::ptree`, plus `VariantConfigMap` / `AnyConfigMap` type-safe/type-erased maps.
- `utils/struct_json.hpp` — generic `to_json`/`from_json` for `Aggregte` and `Reflectable` types using RapidJSON.
- `logger/logger.hpp` / `logger/logger.cpp` — separate logging configuration loaded from `logcfg.ini` (referenced in `restapi.ini` comments).

## Architecture and Conventions

### INI-based server configuration (`restapi.ini`)

- The REST server looks for `restapi.ini` in its working directory; the path can be overridden via `--config <file>` / `-c` on the command line (`restapi/main.cpp`).
- A missing file is **not an error**: `LoadServerConfig` returns the struct with all built-in defaults populated (see `ServerConfig` default member initializers). This lets a freshly built server start without any config file.
- If the file exists but is malformed, `boost::property_tree::file_parser_error` propagates out of `LoadServerConfig`.
- Values are read with `pt.get<T>("section.key", default_value)` so every field has a fallback.
- Defensive normalization runs after parsing: port clamped to `(0, 65535]` (defaults to 8080), `default_format` forced to `json` unless exactly `xml`, `max_concurrent_jobs` / `max_jobs` floored at 1, `result_ttl_seconds` floored at 0.
- The `[auth]` section is reserved for a future release and not read by the current implementation.

### AppConfig singleton

- `AppConfigSingletone::GetInstance()` implements a double-checked lock pattern with `std::shared_mutex` (readers use `std::shared_lock`, writer uses `std::unique_lock`).
- `DeleteInstance()` allows explicit teardown; there is no automatic destruction guarantee since the instance is heap-allocated.
- Currently only exposes `GetSevenZPath()`, intended for external tool paths.

### ParamStore persistence

- `ParamStore` wraps an `ISQLAdapter` and provides per-pipeline-config-type tables with columns derived from reflection (`EnsureSchema`, `EnsureTable`).
- Save flow is staged: Validate → Reflect → Coerce → Upsert; batch APIs run inside a transaction.
- Errors are never swallowed: `last_error_` records the message and the exception is rethrown so callers (including Lua adapters) can handle them.
- At-rest encryption: when `SetCryptoKey` receives a non-empty key, serialized column maps are encrypted with AES-256-CBC (password-derived) and stored in a dedicated `param_blob` column; `param_key`, `schema_version`, `created_at`, `updated_at` remain plaintext for lookup/listing. Existing rows are only readable with the same key.
- Migration hook (`MigrationFn`) is called per backend/table/schema version.

### Property tree and config map abstractions

- `Config::from_ini/json/xml` and `to_ini/json/xml` wrap `boost::property_tree` with consistent error semantics (`file_parser_error`).
- `VariantConfigMap<Args...>` stores values as `std::variant<Args...>` keyed by string; `AnyConfigMap` stores `std::any`. Both expose `GetOptional<T>`, `AddOrChangeValue`, `ToPtree`, `AddValueInPtree`, and conversion between each other.
- `ChangeTranslator` helpers migrate values between old/new string translators in a ptree.

### Serialization of pipeline configs

- Pipeline configuration structs are serialized/deserialized through `Utils::to_json`/`from_json` in `utils/struct_json.hpp`, which dispatches on three categories: `Reflectable` (via `StaticReflect::Reflector`), `Aggregte` (via `boost::pfr`), or custom `RapidJsonValueConvertible` types implementing `to_json`/`from_json`.
- Enums are serialized as their name strings via `EnumName` / `EnumFromName`; `std::optional` fields become JSON null when empty.
- Unsupported field types throw `RuntimeError` during serialization/deserialization.

## Conventions and Constraints

- **INI keys are dotted section.path strings** (e.g. `server.host`, `paramstore.backend`) consumed by `boost::property_tree::ptree::get`.
- **Missing config files are tolerated** for the REST server: `LoadServerConfig` treats a nonexistent path as "use built-in defaults" rather than failing.
- **Numeric bounds are normalized defensively** after parsing in `LoadServerConfig` (port range, minimum job counts, non-negative TTL); invalid `default_format` is coerced to `json`.
- **ParamStore errors propagate**: the API documents that exceptions are rethrown and `GetLastError()` captures the last failure message.
- **At-rest encryption is opt-in** via `ParamStore::SetCryptoKey`; an empty key preserves the prior wide-table plaintext behavior.
- **No environment variable overrides** are implemented anywhere in the codebase; configuration comes exclusively from files and the in-process singleton/database.
- **Feature flags / toggles** do not exist as a subsystem; the `[auth]` section in `restapi.ini` is explicitly marked "reserved for a future release" and unused.