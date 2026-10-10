---
kind: configuration_system
name: 'Configuration System: AppConfig Singleton, Reflection-Backed Struct Serialization, and ParamStore Persistence'
category: configuration_system
scope:
    - '**'
source_files:
    - utils/app_config.hpp
    - utils/app_config.cpp
    - utils/struct_json.hpp
    - utils/struct_yaml.hpp
    - utils/struct_xml.hpp
    - fileoperator/param_store.hpp
    - fileoperator/param_store.cpp
    - logger/logger.cpp
    - tests/AppConfig/app_config_test.cpp
---

# Configuration System in HsBaSlicer

## Overview

HsBaSlicer does not use a single external configuration framework (no .env parser, no Boost.Program_options, no dedicated config file loader). Instead it composes three layers:

1. **Runtime application settings** — a thread-safe singleton (`AppConfigSingletone`) exposing mutable process-wide values such as the 7-Zip executable path.
2. **Struct serialization primitives** — reflection-driven JSON/YAML/XML serializers/deserializers (`utils/struct_json.hpp`, `struct_yaml.hpp`, `struct_xml.hpp`) built on top of RapidJSON, yaml-cpp, TinyXML2 and Boost.PFR / `base/static_reflect.hpp`.
3. **Process-parameter persistence** — `ParamStore` (`fileoperator/param_store.{hpp,cpp}`), which persists per-pipeline configuration structs into a SQL backend via an `ISQLAdapter` abstraction, with schema management, staged validation/reflection/coercion/upsert, batch transactions, progress callbacks, and migration hooks.

There is no repository-wide `.env`, `.ini`, or TOML-based application bootstrap; the only hard-coded config file reference found is `logger/logger.cpp` loading `logcfg.ini` from the current directory for the logger subsystem.

## Key Files and Packages

| Layer | File(s) | Role |
|---|---|---|
| Runtime settings | `utils/app_config.hpp`, `utils/app_config.cpp` | Thread-safe singleton for shared app-level settings (currently only `sevenZ_path_`). |
| JSON serializer | `utils/struct_json.hpp` | Reflection-backed `to_json`/`from_json` over RapidJSON. |
| YAML serializer | `utils/struct_yaml.hpp` | Reflection-backed `to_yaml`/`from_yaml` over yaml-cpp. |
| XML serializer | `utils/struct_xml.hpp` | Reflection-backed `to_xml`/`from_xml` over TinyXML2. |
| Parameter persistence | `fileoperator/param_store.hpp`, `fileoperator/param_store.cpp` | CRUD + batch transaction layer over `ISQLAdapter`. |
| Logger config | `logger/logger.cpp` | Reads `logcfg.ini` from `current_path()`. |
| Tests | `tests/AppConfig/app_config_test.cpp` | Validates singleton identity, default value, idempotent delete, and concurrent access. |

## Architecture and Conventions

### AppConfigSingletone — process-wide runtime settings

- Implemented as a classic double-checked locking singleton using `std::shared_mutex` (`GetInstance` takes a `unique_lock` on first construction, `GetSevenZPath` uses a `shared_lock`).
- Exposes `GetInstance()` and `DeleteInstance()` so callers can reset the singleton between tests.
- Currently holds one field: `std::string sevenZ_path_`, accessed through `GetSevenZPath()`.
- The test suite verifies that repeated `GetInstance` calls return the same address, that the default path is empty, that `DeleteInstance` is idempotent, and that eight concurrent threads all observe the same instance.

No file/env/TOML loading is performed by this class — it is a pure in-process state holder.

### Reflection-driven struct serialization (JSON / YAML / XML)

All three serializers share the same design pattern:

- They operate on two categories of types:
  - **Aggregate types** (`Aggregte<T>`) serialized via Boost.PFR's `names_as_array<T>()` and `for_each_field`.
  - **Reflectable types** (`Reflectable<T>`) serialized via `StaticReflect::Reflector<T>` from `base/static_reflect.hpp`.
- Custom conversion is supported through type traits/concepts: `RapidJsonValueConvertible`, `YAMLValueConvertible`, `TinyXmlConvertible`, each requiring a pair of member functions (`to_*` and static `from_*`).
- Supported field types include enums (serialized as their name via `EnumName`/`EnumFromName`), strings, arithmetic types, ranges, and `OptionalLike` wrappers.
- Public API surface is uniform across formats: `to_*`/`from_*`, `write_*`/`read_*`, and `*_from_file`/`*_from_string` helpers that throw `IOError` on I/O failure and `RuntimeError` on parse errors.
- Missing fields during deserialization are silently skipped (the target struct retains its default value), making the format forward/backward compatible.

These headers are the foundation used by `ParamStore` to reflect pipeline config structs into column maps and back.

### ParamStore — typed parameter persistence

`ParamStore` is the central configuration persistence component. It is bound to a single `SQL::ISQLAdapter&` and supports multiple backends detected at construction time via `ParamSchema::DetectBackend`.

Core flow of `Save` (documented in the header comment):

1. **Validate** — checks that the `AnyObject` wraps a registered `PipelineConfig` type.
2. **Reflect** — walks fields via `AnyObject::ForeachField` and converts them to `std::any` columns through `FieldToAny`.
3. **Coerce** — adds metadata columns (`param_key`, `schema_version`, `created_at`, `updated_at`); for `FileTransfer` configs, `file_paths` is additionally serialized as a JSON TEXT column via RapidJSON.
4. **Upsert** — selects by `param_key`; if found, updates; otherwise inserts and probes for the generated `param_id`.

Additional capabilities:

- `EnsureSchema()` / `EnsureTable(tag)` — creates tables per `PipelineConfigTag` using `ParamSchema::CreateStatement`.
- `SaveBatch` — wraps all saves in a single `BEGIN ... COMMIT` transaction (dialect-aware: PostgreSQL vs SQLite-style `BEGIN TRANSACTION`).
- `List(whereJson)` — parses a JSON object into column filters via RapidJSON.
- `Update(table, key, partial, changedFields)` — partial update restricted to explicitly listed fields.
- `SetProgressCallback(ProgressFn)` — emits `(percent, stage)` callbacks (e.g. "validate", "reflect & coerce", "upsert", "inserted").
- `SetMigrationHook(MigrationFn)` — hook invoked per backend/table/schema version.
- `Guard(Fn)` — wraps every operation, clearing `last_error_` before execution and rethrowing `RuntimeError` after recording the message.

Table naming convention: if the caller supplies a non-empty table name it is used verbatim; otherwise `DefaultTableName(tag)` is tried; finally it falls back to `"hsba_param_" + TypeShortName(full_type_name)`, where `TypeShortName` strips the `HsBa` prefix and the trailing `PipelineConfig` suffix and lowercases the remainder.

The schema version is hardcoded as `constexpr int64_t kSchemaVersion = 1`.

### Logger configuration

`logger/logger.cpp` constructs a log config path by appending `/logcfg.ini` to `current_path().string()` and loads it there. This is the only place in the codebase that reads a named configuration file directly.

## Conventions and Constraints

- **Singletons are explicit**: runtime settings go through `AppConfigSingletone::GetInstance()` / `DeleteInstance()` rather than global variables; concurrency is guarded with `std::shared_mutex`.
- **Serialization is reflection-first**: new config structs do not need hand-written parsers — they participate automatically if they are aggregate types or marked `Reflectable` via `base/static_reflect.hpp`. Custom types opt in by implementing the corresponding `to_*`/`from_*` pair required by the relevant concept.
- **Errors are never swallowed**: `ParamStore::Guard` records `last_error_` but rethrows the original exception; JSON/YAML/XML read/write helpers throw `RuntimeError` on parse failures and `IOError` on file I/O failures.
- **Persistence is typed and staged**: every `Save` goes through Validate → Reflect → Coerce → Upsert, with progress callbacks reporting each stage.
- **Batch operations are transactional**: `SaveBatch` always begins a transaction and rolls back on any failure, preserving the original exception.
- **Schema evolution is versioned**: `kSchemaVersion = 1` plus a `MigrationFn` hook provide the mechanism for future schema migrations.
- **No env-var / TOML / INI application bootstrap exists** beyond the logger's `logcfg.ini` load; application configuration is loaded by callers of `ParamStore` and `AppConfigSingletone`, not by a centralized config loader.

## Applicable Scope

This category applies to the repository. The configuration concern is present but lightweight: a small in-process singleton plus a robust reflection-backed serialization toolkit and a typed SQL-backed parameter store, rather than a full-featured config-file framework.