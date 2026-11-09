---
kind: configuration_system
name: 'Configuration System: INI/JSON/YAML Loaders, Reflection-Based Struct Serialization, and ParamStore Persistence'
category: configuration_system
scope:
    - '**'
source_files:
    - restapi/restapi.ini
    - restapi/server_config.hpp
    - restapi/server_config.cpp
    - fileoperator/rw_ptree.hpp
    - utils/struct_json.hpp
    - utils/struct_yaml.hpp
    - utils/struct_xml.hpp
    - utils/app_config.hpp
    - utils/app_config.cpp
    - fileoperator/param_store.hpp
    - restapi/config_bridge.hpp
---

## Overview

HsBaSlicer does not use a single monolithic configuration framework. Instead it composes several small, purpose-built subsystems:

1. **Runtime process config** — `restapi/server_config.{hpp,cpp}` loads `restapi.ini` via `boost::property_tree` into the `ServerConfig` struct.
2. **Application-level singleton config** — `utils/app_config.{hpp,cpp}` exposes a thread-safe `AppConfigSingletone` (currently only the 7-Zip path).
3. **Generic file-format serializers** — `utils/struct_json.hpp`, `utils/struct_yaml.hpp`, `utils/struct_xml.hpp` provide reflection-driven JSON/YAML/XML serialization for aggregate and reflectable C++ structs.
4. **Property-tree bridge** — `fileoperator/rw_ptree.hpp` wraps `boost::property_tree` with typed `from_ini`/`from_json`/`from_xml` helpers plus `VariantConfigMap` / `AnyConfigMap` key-value stores.
5. **Pipeline-parameter persistence** — `fileoperator/param_store.{hpp,cpp}` persists `PipelineConfig` structs to SQLite / MySQL / PostgreSQL through `SQL::ISQLAdapter`, with optional AES-256-CBC at-rest encryption.
6. **Wire-format bridge** — `restapi/config_bridge.{hpp,cpp}` converts flat C ABI pipeline configs (`HsBa*PipelineConfig_t`) to/from JSON or XML via `Utils::AnyObject` + `param_convert`.

There is no `.env` loader, no environment-variable-based feature flags, and no centralized feature-flag system in scope of these files.

## Key Files

| File | Role |
|---|---|
| `restapi/restapi.ini` | Canonical default server configuration (INI) |
| `restapi/server_config.hpp` | `ServerConfig` struct with built-in defaults per section |
| `restapi/server_config.cpp` | `LoadServerConfig(path)` — reads ini, normalizes values |
| `fileoperator/rw_ptree.hpp` | `Config::from_ini/json/xml`, `VariantConfigMap`, `AnyConfigMap` |
| `utils/struct_json.hpp` | Reflection-driven RapidJSON serializer (`to_json`/`from_json`) |
| `utils/struct_yaml.hpp` | Reflection-driven yaml-cpp serializer (`to_yaml`/`from_yaml`) |
| `utils/struct_xml.hpp` | Reflection-driven XML serializer |
| `utils/app_config.hpp/.cpp` | Thread-safe `AppConfigSingletone` singleton |
| `fileoperator/param_store.hpp` | `ParamStore` — CRUD over `PipelineConfig` types |
| `restapi/config_bridge.hpp` | `SerializeConfig` / `DeserializeConfig` for REST wire format |

## Architecture and Conventions

### Server config loading (`restapi.ini` → `ServerConfig`)

`LoadServerConfig` in `server_config.cpp` follows a strict pattern:

1. If the file does not exist, return the struct with its built-in defaults — missing config is never an error.
2. Parse via `Config::from_ini(path)` (wrapping `boost::property_tree`).
3. Read each field with `pt.get<T>("section.key", cfg.field)` so absent keys keep defaults.
4. Normalize every value defensively after parsing:
   - `port` clamped to `(0, 65535]`, else reset to 8080.
   - `default_format` forced to `json` unless exactly `xml`.
   - `max_concurrent_jobs`, `max_jobs` floored at 1.
   - `upload_evict_mode` accepted only as `ttl | count | both | manual`; unknown values fall back to `both`.
   - `upload_max_files` floored at `HSBA_MODEL_POOL_SIZE` (a compile-time constant from top-level CMake).
   - `upload_ttl_seconds` clamped to `[0, 2592000]` (30 days), `upload_sweep_interval_seconds` to `[5, 3600]`.

Boolean parsing accepts case-insensitive `1/true/yes/on` and `0/false/no/off`. The `auth.enabled` field supports an extra `auto` mode: when empty, auth is enabled iff a token is present.

The `restapi.ini` file itself documents every section (`[server]`, `[upload]`, `[paramstore]`, `[auth]`, `[rate_limit]`) and states that all keys are optional and values shown are built-in defaults.

### Generic reflection-based serialization

`struct_json.hpp` and `struct_yaml.hpp` share the same design:

- They operate on two categories of types: **aggregate** (POD-like, introspected via `boost::pfr`) and **reflectable** (registered via `base/static_reflect.hpp`'s `StaticReflect::Reflector`).
- Both support nested aggregates, ranges, `std::optional`/`OptionalLike`, enums (via `EnumName`/`EnumFromName`), strings, and arithmetic types.
- Custom types can opt in by implementing `to_json`/`from_json` (RapidJSON) or `to_yaml`/`from_yaml` (yaml-cpp).
- Missing fields during deserialization are silently ignored; the target struct retains its default-initialized values.
- Unsupported field types throw `RuntimeError`.

This is the foundation used by `restapi/config_bridge` to serialize/deserialize the flat C ABI pipeline config structs without hand-written converters.

### Property tree bridge and type-erased maps

`rw_ptree.hpp` provides:

- `Config::from_ini/json/xml(path)` returning `boost::property_tree::ptree`.
- `Config::to_ini/json/xml(path, ptree)` for writing.
- `VariantConfigMap<Args...>` — a `std::unordered_map<string, std::variant<Args...>>` with `GetOptional<T>(key)`, `AddOrChangeValue(key, value)`, and conversion to/from `ptree`.
- `AnyConfigMap` — the same interface backed by `std::any` for runtime-typed storage.
- `ChangeTranslator` helpers for migrating property-tree values between old/new string translators.

These are generic utilities; the concrete `ServerConfig` loader bypasses them and uses `ptree.get<T>` directly.

### Application singleton config

`AppConfigSingletone` is a manually managed singleton with double-checked locking using `std::shared_mutex`:

- `GetInstance()` lazily allocates the instance.
- `DeleteInstance()` frees it.
- `GetSevenZPath()` returns the stored 7-Zip executable path under a shared lock.

It currently holds only one setting (`sevenZ_path_`) and is accessed wherever external tool paths are needed.

### Pipeline parameter persistence (`ParamStore`)

`ParamStore` is the persistent counterpart of the transient `ServerConfig`:

- It wraps an `SQL::ISQLAdapter` (SQLite, MySQL, or PostgreSQL) and one wide table per `PipelineConfig` tag.
- `Save` runs a staged `Validate → Reflect → Coerce → Upsert` flow; batch APIs wrap operations in transactions.
- Errors are never swallowed: `last_error_` is recorded and exceptions rethrown so callers (including Lua) decide handling.
- Optional at-rest encryption: when `SetCryptoKey` is called with a non-empty password, the reflected column map is serialized, encrypted with AES-256-CBC (password-derived), and stored in a dedicated `param_blob` column; plaintext columns stay empty. `param_key`, `schema_version`, `created_at`, `updated_at` remain plaintext for lookup/listing.
- Migration hooks (`MigrationFn`) allow backend-specific schema evolution.

### Wire-format bridge

`config_bridge` exists because the flat C ABI pipeline config structs contain raw `const char*` members that `struct_json`/`struct_xml` cannot handle directly. The bridge walks the reflective field table registered in `fileoperator/param_reflect.hpp` via `Utils::AnyObject`, converges every field through `param_convert`'s `FieldToAny`/`AnyToField`, then serializes the uniform view with the project's `Utils` serializers.

## Conventions and Constraints

- **Missing config files are not errors.** `LoadServerConfig` returns built-in defaults when `restapi.ini` is absent; this is documented in both the function comment and the ini header.
- **Every `ServerConfig` member carries a built-in default**, so partial ini sections still yield usable configuration.
- **All numeric/string settings are normalized after parsing** — invalid ports, unrecognized evict modes, out-of-range TTLs, and undersized upload pools are corrected rather than rejected.
- **Auth is auto-enabled**: if `auth.enabled` is empty and a token is set, authorization is turned on automatically; enabling auth with an empty token is a startup error (documented in `restapi.ini`).
- **Loopback binds skip auth by design**: `127.0.0.1` / `::1` stays token-free so local workflows and container health probes work without credentials.
- **Upload pool size is constrained by `HSBA_MODEL_POOL_SIZE`**: `upload_max_files` is floored at the model-pool capacity so retained uploads can always refill the in-process pool.
- **Serialization is reflection-driven, not hand-coded**: new pipeline config structs need no additional JSON/YAML/XML code — they are picked up by `StaticReflect` / `boost::pfr`.
- **Errors in serialization/deserialization propagate as exceptions** (`RuntimeError`, `InvalidArgumentError`, `IOError`, `boost::property_tree::file_parser_error`); they are not swallowed.
- **ParamStore errors propagate too**: the class comment states "errors are never swallowed: last_error is recorded and the exception is rethrown".
- **At-rest encryption is opt-in via `SetCryptoKey`**; existing rows are only readable with the same key.
- **No environment-variable based configuration** was found in the inspected files; secrets like the auth token are intended to be injected via the `HSBA_REST_AUTH_TOKEN` environment variable (documented in `restapi.ini` comments) but the actual env-var reading logic is not in `server_config.cpp`.
- **Feature flags are not implemented as a separate subsystem**; toggles such as `auth_enabled`, `upload_enabled`, `paramstore_enabled` are plain boolean fields in `ServerConfig`.