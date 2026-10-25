# HsBaSlicer REST API Server

`HsBaSlicerRestServer` is an **optional** executable that exposes the
`DllHsBaSlicer` C ABI over HTTP, answering in both **JSON** and **XML**. It is
built only when the CMake option `HSBA_BUILD_REST_API` is `ON`, and it is
available on desktop platforms only (Windows / Linux / macOS); it is forced `OFF`
elsewhere.

## Enabling the build

The HTTP server is backed by [cpp-httplib](https://github.com/yhirose/cpp-httplib),
which is installed through the vcpkg **`rest-api`** manifest feature (not part of
`default-features`). Both the CMake option and the vcpkg feature must be enabled
together, in the same configure session:

```sh
cmake --preset <existing-preset> \
      -DHSBA_BUILD_REST_API=ON \
      -DVCPKG_MANIFEST_FEATURES=rest-api
cmake --build --preset <existing-preset>
```

If `HSBA_BUILD_REST_API=ON` is set without the `rest-api` feature, configuration
fails with a `FATAL_ERROR` telling you to enable it (the detection mirrors the
existing `HSBA_COPL` pattern).

The HTTP transport layer (`routes.cpp`) is the only part that needs
cpp-httplib. The non-HTTP units (`content_negotiate`, `config_bridge`,
`pipeline_executor`, `job_manager`, `server_config`) have no HTTP dependency and
are covered by the standalone unit tests under `tests/RestApi/`.

## Configuration (`restapi.ini`)

The server reads `restapi.ini` from its working directory (same convention as
`logcfg.ini`); a copy is placed next to the executable at build time. Override
the path with `--config <file>`. Command-line `--host` / `--port` override the
ini values. A missing file is not an error — the built-in defaults apply.

| Section | Key | Default | Meaning |
|---|---|---|---|
| `[server]` | `host` | `127.0.0.1` | Bind address (`0.0.0.0` to expose on the LAN). |
| `[server]` | `port` | `8080` | TCP port. |
| `[server]` | `default_format` | `json` | Response format when neither `?format=` nor `Accept` is given. |
| `[server]` | `max_concurrent_jobs` | `2` | Worker threads running slicing jobs concurrently. |
| `[server]` | `max_jobs` | `128` | Tracked-job cap; new submissions get HTTP 429 beyond it. |
| `[server]` | `result_ttl_seconds` | `3600` | Finished-job retention before reaping. |
| `[paramstore]` | `backend` | `sqlite` | `sqlite` / `mysql` / `postgresql`; empty disables `/api/params` (503). |
| `[paramstore]` | `sqlite_path` / `host` / `user` / `password` / `database` / `port` / `crypto_password` | | ParamStore connection (per-request). |
| `[auth]` | — | — | Reserved; not read in this release. |

## Format negotiation

- **Response** format precedence: `?format=json|xml` → `Accept` header → `default_format`.
- **Request body** format is chosen by `Content-Type` (`application/xml` / `text/xml` →
  XML, anything else → JSON).

Every machine-readable response is produced by the project's `Utils` serializers
(`write_pretty_json` / `write_xml`); no JSON/XML is hand-assembled. Config
structs — which contain `const char*` members unsupported by those serializers —
are bridged through `param_reflect` reflection field-by-field (`config_bridge`).

## Endpoints

| Method | Path | Purpose | Underlying call |
|---|---|---|---|
| `GET` | `/api/health` | Liveness probe (status + uptime). | in-process |
| `GET` | `/api/version` | Library version. | `HsBaGetVersionJson/Xml` |
| `GET` | `/api/pipelines` | Supported pipeline kinds. | dispatch table |
| `GET` | `/api/pipelines/{kind}/default-config` | Default config for a kind. | `HsBaCreateDefault*Config` |
| `POST` | `/api/config/convert?kind={kind}` | Convert proto bytes → JSON/XML; append `&to=proto` to encode a JSON/XML body → proto bytes. | `HsBa*ConfigFromProtoBytes/ToProtoBytes` |
| `POST` | `/api/jobs?kind={kind}` or `/api/jobs/{kind}` | Submit a slicing job. | `HsBaRun*Pipeline` (async) |
| `GET` | `/api/jobs?state=&offset=&limit=` | List / paginate jobs (optional state filter). | job queue |
| `GET` | `/api/jobs/{id}` | Job status (state/progress/result metadata). | job queue |
| `GET` | `/api/jobs/{id}/result` | Downloadable artifact (gcode text / output file). | job queue |
| `DELETE` | `/api/jobs/{id}` | Delete a queued or finished job (running jobs are refused). | job queue |
| `PUT` | `/api/params/{kind}/{key}` | Upsert a config into the ParamStore. | `HsBaSavePipelineParams` |
| `GET` | `/api/params/{kind}/{key}` | Load a config from the ParamStore. | `HsBaLoadPipelineParams` |

`{kind}` is one of: `fdm`, `sla`, `sls`, `slm`, `lom`, `tdp`, `waam`, `custom`,
`filetransfer`.

Spec-verbatim aliases are also served: `GET /api/kinds` (≡ `/api/pipelines`),
`GET /api/config/default/{kind}` (≡ `/api/pipelines/{kind}/default-config`), and
`POST /api/params/{kind}?key=` (≡ `PUT /api/params/{kind}/{key}`).

## Slicing flow

1. `GET /api/pipelines/{kind}/default-config` to obtain a template.
2. Edit the fields and `POST /api/jobs/{kind}` with the config body → `202` and
   `Location: /api/jobs/{id}`.
3. Poll `GET /api/jobs/{id}` until `state` is `success` or `failed`.
4. On success fetch `GET /api/jobs/{id}/result`.

## Errors

Failures are uniform `{code, message, detail}` JSON / `<Error>` XML:
`400` invalid kind/body/config conversion, `404` unknown job or key, `409` job
not finished (result) or still running (delete), `429` queue full, `503` ParamStore
not configured. `DELETE /api/jobs/{id}` returns `204` on success.

## Security note

No authentication is implemented (trusted local/LAN model). Do **not** bind to a
public interface on an untrusted network.
