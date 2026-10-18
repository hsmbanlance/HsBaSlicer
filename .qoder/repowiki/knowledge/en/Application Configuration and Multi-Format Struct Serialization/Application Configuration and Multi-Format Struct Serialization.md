---
kind: configuration_system
name: Application Configuration and Multi-Format Struct Serialization
category: configuration_system
scope:
    - '**'
source_files:
    - utils/app_config.hpp
    - utils/app_config.cpp
    - utils/logcfg.ini
    - logger/logger.cpp
    - utils/struct_json.hpp
    - utils/struct_yaml.hpp
    - utils/struct_xml.hpp
    - tests/AppConfig/app_config_test.cpp
---

## Overview

HsBaSlicer does not use a single centralized configuration framework. Instead, runtime configuration is split across two small, independent subsystems in `utils/`:

1. **`AppConfigSingletone`** — a thread-safe singleton holding process-wide application settings (currently only the external 7-Zip executable path).
2. **Typed struct serialization helpers** (`struct_json.hpp`, `struct_yaml.hpp`, `struct_xml.hpp`) that serialize arbitrary C++ structs to/from JSON/YAML/XML using Boost.PFR reflection.
3. **Logger INI config** — the logger reads its own `[log]` / `[log_format]` section from `utils/logcfg.ini` via Boost.PropertyTree.

There is no `.env` loader, no feature-flag system, and no secrets manager in this repository.

## Key Files

- `utils/app_config.hpp` / `utils/app_config.cpp` — `AppConfigSingletone` class.
- `utils/logcfg.ini` — default logger configuration file.
- `logger/logger.cpp` — parses `logcfg.ini` at startup and applies log level/file settings.
- `utils/struct_json.hpp` — RapidJSON-based reflectable struct serializer/deserializer.
- `utils/struct_yaml.hpp` — YAML-based reflectable struct serializer/deserializer.
- `utils/struct_xml.hpp` — TinyXML-based reflectable struct serializer/deserializer.
- `tests/AppConfig/app_config_test.cpp` — tests for the singleton's identity, defaults, deletion, and concurrent access.
- `tests/ConfigMap/*_test.cpp` — tests exercising the JSON/YAML/XML struct converters.

## Architecture and Conventions

### AppConfigSingletone

The class lives in namespace `HsBa::Slicer` and exposes:

- `GetInstance()` — double-checked locking with `std::shared_mutex` over a static `instance_` pointer; returns a reference to the lazily-created singleton.
- `DeleteInstance()` — destroys the cached instance under lock; idempotent (safe to call when already deleted).
- `GetSevenZPath() const` — returns the configured 7-Zip executable path (empty string by default).

The constructor is private; there is no setter exposed yet, so the current design is read-only after construction. The test suite verifies singleton identity, empty default, safe re-creation after deletion, and that eight concurrent threads all receive the same pointer.

### Logger INI Configuration

`logger/logger.cpp` constructs the config path as `current_path.string() + "/logcfg.ini"` (relative to the working directory) and loads it with Boost.PropertyTree. It reads:

- `log.log_level` (int)
- `log.log_level_debug` (int)
- `log.use_log_file` (bool)
- `log.log_file` (string)
- `log_format.log_datatime_format`
- `log_format.log_time_format`

If the file cannot be opened or parsed, sensible defaults are applied: `log_level = 3`, `use_log_file = false`. On desktop platforms the logger then configures Boost.Log severity filtering; on Android/iOS it maps to platform log priorities.

### Typed Struct Serialization (JSON / YAML / XML)

All three serializers share the same design pattern built on top of `base/static_reflect.hpp` and `base/concepts.hpp`:

- A type is "reflectable" if it satisfies the `Reflectable<T>` concept (generated via `StaticReflect`).
- For non-reflectable aggregate types, Boost.PFR (`boost/pfr.hpp`) introspects fields by position.
- Custom per-field conversion is supported through concepts like `RapidJsonValueConvertible` (requires `to_json(Value&, Allocator&)` and static `from_json(Value)`), which lets user-defined types plug into the generic serializer without macros.
- Enums are serialized by name via `EnumName`.
- `std::string`, `std::string_view`, arithmetic types, ranges, and `OptionalLike` wrappers are handled as special cases.
- Nested reflectable/aggregates are recursively expanded.

This is a compile-time, header-only library used throughout the codebase to convert pipeline configuration structs (defined in `proto/*.proto` and mirrored in C++) to/from human-readable files.

## Conventions and Constraints

- Application-level settings go through `AppConfigSingletone`; there is no global config map or environment-variable loader in scope.
- The 7-Zip path is currently the only configurable setting exposed by `AppConfigSingletone`; new settings would follow the same getter-on-singleton pattern.
- Logger configuration is loaded once from `logcfg.ini` relative to the working directory; missing or malformed files fall back to built-in defaults rather than throwing.
- Struct serialization uses Boost.PFR reflection — adding a field to a reflectable struct automatically includes it in JSON/YAML/XML output; removing a field breaks backward compatibility unless the serializer explicitly handles missing members (the `HasMember` check in `set_reflectable_fields_impl` provides optional-member tolerance for JSON).
- There is no hierarchical configuration merging, no environment variable override layer, and no secret management in this repository.