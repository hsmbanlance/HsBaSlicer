---
kind: configuration_system
name: 'Configuration System: Reflection-Driven Serializers, Variant Config Maps, and ParamStore'
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
    - tests/AppConfig/app_config_test.cpp
    - tests/ConfigMap/configmap_test.cpp
---

## Overview

HsBaSlicer has no single application-wide config loader (no `.env`, no `application.properties`). Instead it provides three complementary layers:

1. **A process-global singleton** for a small set of runtime paths (`AppConfigSingletone` in `utils/app_config.{hpp,cpp}`).
2. **Reflection-driven serializers** for JSON/YAML/XML that turn arbitrary C++ structs into/from persistent documents (`utils/struct_json.hpp`, `utils/struct_yaml.hpp`, `utils/struct_xml.hpp`).
3. **Typed key-value maps backed by Boost.PropertyTree** plus a SQL-backed persistence layer (`fileoperator/param_store.*`) for pipeline parameters.

There is no environment-variable parser, no feature-flag registry, and no secret manager in this codebase; secrets are handled only as an optional AES-256-CBC encryption key passed to `ParamStore::SetCryptoKey`.

## Key files and packages

- `utils/app_config.hpp`, `utils/app_config.cpp` — thread-safe singleton exposing `GetSevenZPath()`.
- `utils/struct_json.hpp` — RapidJSON serializer/deserializer built on `boost::pfr` + `base/static_reflect.hpp`.
- `utils/struct_yaml.hpp` — yaml-cpp serializer/deserializer with the same reflection pattern.
- `utils/struct_xml.hpp` — TinyXML2 serializer/deserializer with the same reflection pattern.
- `fileoperator/param_store.hpp`, `fileoperator/param_store_ops.cpp` — SQL-backed CRUD for `PipelineConfig` structs.
- `fileoperator/rw_ptree.hpp` — `VariantConfigMap` / `AnyConfigMap` over `boost::property_tree::ptree`.
- `tests/AppConfig/app_config_test.cpp` — verifies singleton identity, default-empty path, idempotent delete, and concurrent access.
- `tests/ConfigMap/configmap_test.cpp` — exercises both typed and untyped config maps and Eigen vector translation.

## Architecture and conventions

### AppConfigSingletone
A lazily constructed, double-checked-locked singleton guarded by `std::shared_mutex`. The constructor is private, `GetInstance()` returns a reference, `DeleteInstance()` resets the static pointer, and `GetSevenZPath()` reads under a shared lock. The test suite asserts that repeated calls return the same address and that eight concurrent threads converge on one instance.

### Reflection-based struct serialization
All three formats share the same design:

- Types are serialized via two mechanisms: (a) aggregate types deserialized through `boost::pfr::names_as_array<T>()` / `for_each_field`, or (b) "reflectable" types whose fields are enumerated through `StaticReflect::Reflector<T>::FieldName/I` from `base/static_reflect.hpp`.
- Each format defines a concept (`RapidJsonValueConvertible`, `YAMLValueConvertible`, `TinyXmlConvertible`) for custom conversion APIs (`to_json`/`from_json`, `to_yaml`/`from_yaml`, `to_xml`/`from_xml`).
- Enums are always serialized as their `EnumName(...)` string and deserialized via `EnumFromName<T>`.
- `std::optional`-like fields write null when empty and skip deserialization when absent/null.
- Range fields serialize as arrays and deserialize back into the same range type.
- Unsupported field types throw `RuntimeError` at runtime rather than failing at compile time.
- File I/O helpers (`read_json_from_file`, `write_yaml`, `read_xml_from_string`, etc.) open streams themselves and throw `IOError` if the file cannot be opened.

This is a pure data-binding library — it does not load configuration from any fixed path or precedence chain.

### VariantConfigMap / AnyConfigMap
In `fileoperator/rw_ptree.hpp` (referenced by `tests/ConfigMap/configmap_test.cpp`) there are two in-memory config stores:

- `VariantConfigMap<T...>` — a statically-typed map where each value's type is known at construction.
- `AnyConfigMap` — a fully dynamic map using `std::any`.

Both expose `AddOrChangeValue`, `GetOptional<T>(key)`, `ToPtree()`, and `AddValueInPtree<T>(ptree, key)` for round-tripping against `boost::property_tree::ptree`. Custom translators (e.g. `EigenVector2fTranslator`) plug in via the `ChangeTranslator` helper so Eigen vectors can be stored as ptree scalars.

### ParamStore (SQL-backed parameter persistence)
`ParamStore` is the main persistence API for `PipelineConfig` structs. It:

- Wraps a single `ISQLAdapter` backend (SQLite, etc.).
- Uses `AnyObject` + reflection to traverse fields and build column maps per `PipelineConfigTag`.
- Provides `EnsureSchema` / `EnsureTable` / `Save` / `Load` / `List` / `Update` / `Delete` / `SaveBatch`.
- Supports optional at-rest encryption: `SetCryptoKey(key)` switches Save to encrypt the reflected column map with AES-256-CBC (password-derived) into a `param_blob` column while keeping `param_key`, `schema_version`, `created_at`, `updated_at` plaintext.
- Records `last_error_` but rethrows exceptions so callers (including Lua) decide handling.
- Exposes `ProgressFn` and `MigrationFn` hooks.

## Conventions and constraints

- **No global env-var loading**: the codebase does not read `std::getenv` or parse `.env` files; the only process-global state is `AppConfigSingletone`.
- **Singleton access is explicit**: consumers call `AppConfigSingletone::GetInstance()` and must eventually call `DeleteInstance()`; the tests verify idempotent deletion.
- **Serialization is opt-in per type**: a struct becomes serializable either by being an aggregate (POD-like) or by being marked reflectable via `base/static_reflect.hpp`; custom types implement the corresponding concept's member functions.
- **Missing keys are silent for YAML/XML deserialization**: `from_yaml_impl` / `from_xml_impl` skip fields that are absent from the node/document; JSON `from_json` throws `InvalidArgumentError` only when the top-level value is not an object.
- **Errors propagate upward**: serialization failures throw `RuntimeError` or `InvalidArgumentError`; file I/O errors throw `IOError`; `ParamStore` records `last_error_` but rethrows.
- **Config maps are additive**: `AddOrChangeValue` inserts or replaces; `GetOptional<T>` returns `std::nullopt` when the key is missing or its type does not match.
- **Encryption is keyed by caller**: enabling encryption requires passing a non-empty key to `ParamStore::SetCryptoKey`; existing rows encrypted with one key are only readable with the same key.