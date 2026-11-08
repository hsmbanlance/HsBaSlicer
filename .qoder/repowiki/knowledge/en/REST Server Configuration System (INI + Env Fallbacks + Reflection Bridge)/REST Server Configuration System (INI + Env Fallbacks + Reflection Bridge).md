---
kind: configuration_system
name: REST Server Configuration System (INI + Env Fallbacks + Reflection Bridge)
category: configuration_system
scope:
    - '**'
source_files:
    - restapi/server_config.hpp
    - restapi/server_config.cpp
    - restapi/restapi.ini
    - restapi/main.cpp
    - restapi/config_bridge.hpp
    - restapi/config_bridge.cpp
    - fileoperator/rw_ptree.cpp
    - utils/app_config.hpp
    - utils/app_config.cpp
---

## Overview

The HsBaSlicer REST API server uses a layered configuration system with three sources, applied in strict precedence order:

1. **Built-in defaults** — every field of `ServerConfig` is initialized to a safe default value.
2. **INI file** (`restapi.ini`, path overridable via `--config`) — parsed through `fileoperator/rw_ptree::from_ini` into a `boost::property_tree::ptree` and mapped onto the struct.
3. **Environment variables** — `HSBA_REST_AUTH_TOKEN`, `HSBA_REST_TLS_CERT`, `HSBA_REST_TLS_KEY`; used only when the corresponding ini value is empty.
4. **Command-line overrides** — `--host`, `--port`, `--auth-token`, `--tls-cert`, `--tls-key`, `--require-tls`; these outrank both ini and environment.

A missing ini file is not an error: `LoadServerConfig` returns the defaults unchanged. A malformed ini throws `boost::property_tree::file_parser_error`, which `main.cpp` catches and exits with code 1.

## Key Files

- `restapi/server_config.hpp` / `server_config.cpp` — `ServerConfig` struct (all members carry built-in defaults) and `LoadServerConfig(path)`.
- `restapi/restapi.ini` — canonical default configuration documenting every section/field.
- `restapi/main.cpp` — CLI parsing, env fallback application, precedence enforcement, and invocation of `RunServer(cfg)`.
- `restapi/config_bridge.hpp` / `config_bridge.cpp` — reflection-driven JSON/XML serialization/deserialization for pipeline config structs (`HsBa*PipelineConfig_t`) via `param_reflect` + `param_convert` + `struct_json`/`struct_xml`.
- `fileoperator/rw_ptree.cpp` — thin wrapper around `boost::property_tree::ptree` exposing `Config::from_ini()`.
- `utils/app_config.hpp` / `app_config.cpp` — separate thread-safe singleton (`AppConfigSingletone`) holding process-wide settings such as the 7-Zip executable path; unrelated to the REST server's runtime configuration.

## Architecture and Conventions

### INI sections and keys

| Section | Purpose | Notable fields |
|---|---|---|
| `[server]` | HTTP(S) listener | `host`, `port`, `default_format`, `max_concurrent_jobs`, `max_jobs`, `result_ttl_seconds`, `tls_cert`, `tls_key`, `require_tls` |
| `[upload]` | Multipart model upload pool | `dir`, `max_files`, `ttl_seconds`, `evict_mode`, `max_file_mb`, `max_request_mb`, `max_total_mb`, `sweep_interval_seconds` |
| `[paramstore]` | Persistent parameter store backend | `backend` (`sqlite` | `mysql` | `postgresql`), `sqlite_path`, `host`, `user`, `password`, `database`, `port`, `crypto_password` |
| `[auth]` | Shared token auth | `enabled`, `token` |
| `[rate_limit]` | Reserved for future use | `requests_per_minute` (not read by current implementation) |

### Validation and normalization

`LoadServerConfig` performs defensive post-parse normalization:

- Port clamped to `(0, 65535]`, defaulting to 8080 on invalid values.
- `default_format` coerced to `json` unless explicitly set to `xml`.
- `max_concurrent_jobs` and `max_jobs` floored at 1; `result_ttl_seconds` floored at 0.
- `upload_evict_mode` accepted values are `ttl`, `count`, `both`, `manual`; any other value falls back to `both`.
- `upload_max_files` is raised to at least `HSBA_MODEL_POOL_SIZE` (defined in top-level CMakeLists.txt, 10 with CGAL/OCCT kernels, otherwise 1) so the in-process model pool can always be refilled from retained uploads.
- `upload_ttl_seconds` clamped to `0..2592000` (30 days); `upload_max_file_mb` to `1..4096`; `upload_sweep_interval_seconds` to `5..3600`.
- `upload_max_request_mb` is raised to `max_file_mb + 4` to cover multipart overhead.

### Auth enablement semantics

- If `auth.enabled` is absent or empty, auth is auto-enabled exactly when `auth.token` is non-empty.
- If `auth.enabled` is present, its boolean value wins (parsed as `1`/`true`/`yes`/`on` → true; `0`/`false`/`no`/`off` → false).
- A non-loopback bind (`host != 127.0.0.1` and not `::1`) requires either a configured token or one supplied via `HSBA_REST_AUTH_TOKEN` / `--auth-token`; loopback binds skip auth by design.
- `require_tls = true` refuses to start without a valid `tls_cert`/`tls_key` pair.

### Environment variable fallbacks

Only three variables are recognized, and only when the corresponding ini value is empty:

- `HSBA_REST_AUTH_TOKEN` — also flips `cfg.auth_enabled = true`.
- `HSBA_REST_TLS_CERT`
- `HSBA_REST_TLS_KEY`

This lets container images ship a token-free `restapi.ini` and inject secrets at runtime.

### Command-line precedence

CLI flags override everything else:

```
ini defaults ← LoadServerConfig() ← ApplyEnvFallbacks() ← CLI overrides
```

A non-empty `--auth-token` always arms authorization regardless of `auth.enabled`.

### Pipeline config serialization bridge

For the per-request slicing pipeline configs (flat C ABI structs like `HsBaFdmPipelineConfig_t`), the REST layer does not hand them directly to `struct_json`/`struct_xml`. Instead, `config_bridge` walks the reflective `TypeInfo` table registered in `fileoperator/param_reflect.hpp`, converts each field via `param_convert::FieldToAny` / `AnyToField`, and serializes the uniform `Utils::AnyObject` view. This avoids the unsupported raw `const char*` members and keeps JSON/XML output consistent across all pipeline types.

### Separate concerns

- `utils/app_config.hpp/.cpp` implements a distinct `AppConfigSingletone` (thread-safe via `std::shared_mutex`) that exposes external tool paths (e.g. 7-Zip). It is not loaded from `restapi.ini` and is unrelated to the REST server's runtime configuration.
- The logger subsystem has its own `logcfg.ini` loader (also using `Config::from_ini`), independent from the REST server config.

## Conventions and Constraints

- Every member of `ServerConfig` carries a built-in default; partial or missing ini files produce a usable configuration rather than failing fast.
- All ini string values are trimmed and lowercased before comparison (`to_lower`, `trim`).
- Boolean ini values accept `1/true/yes/on` and `0/false/no/off`; unrecognized values fall back to the struct default.
- Missing ini file → no error; malformed ini → `boost::property_tree::file_parser_error` propagated to `main.cpp`.
- Upload directory being empty disables multipart uploads while leaving all other endpoints functional.
- ParamStore backend `none` (or empty) disables `/api/params` endpoints (they return 503).
- `HSBA_MODEL_POOL_SIZE` is a compile-time constant defined by the top-level CMakeLists.txt; `LoadServerConfig` enforces `upload_max_files >= HSBA_MODEL_POOL_SIZE` at runtime.
- The `[rate_limit]` section exists in `restapi.ini` but is intentionally not read by the current implementation (documented as reserved for a future release).
- Secrets (`auth.token`, `tls_cert`, `tls_key`) are never persisted in the ini by convention; the documented injection path is environment variables.