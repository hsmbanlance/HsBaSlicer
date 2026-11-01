# HsBaSlicer REST API Server

`HsBaSlicerRestServer` is an **optional** executable that exposes the
`DllHsBaSlicer` C ABI over HTTP, answering in both **JSON** and **XML**. It is
built only when the CMake option `HSBA_BUILD_REST_API` is `ON`, and it is
available on desktop platforms only (Windows / Linux / macOS); it is forced `OFF`
elsewhere.

## Enabling the build

The HTTP server is backed by [cpp-httplib](https://github.com/yhirose/cpp-httplib),
which is installed through the vcpkg **`rest-api`** manifest feature (not part of
`default-features`). That feature selects `cpp-httplib[openssl]`, so HTTPS is
available in every REST-enabled build. Both the CMake option and the vcpkg feature
must be enabled together, in the same configure session:

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
`pipeline_executor`, `job_manager`, `server_config`, `auth_gate`, `upload_bridge`,
`upload_store`) have no HTTP dependency and are covered by the standalone unit tests
under `tests/RestApi/`.

Those tests share the module's gate: with `HSBA_BUILD_REST_API=OFF` (the default)
neither the server nor its tests are built, so a configuration can no longer expose
tests for a module it does not compile. When enabled, every case links the
`HsBaSlicerRestApi` implementation library (which contains `routes.cpp`), so an HTTP
layer that stops compiling also breaks the test build.

## Configuration (`restapi.ini`)

The server reads `restapi.ini` from its working directory (same convention as
`logcfg.ini`); a copy is placed next to the executable at build time. Override
the path with `--config <file>`. Command-line options override the ini values, and
`HSBA_REST_AUTH_TOKEN` / `HSBA_REST_TLS_CERT` / `HSBA_REST_TLS_KEY` fill a value
only when the ini left it empty (so an image can ship a token-free ini). A missing
file is not an error — the built-in defaults apply.

| Section | Key | Default | Meaning |
|---|---|---|---|
| `[server]` | `host` | `127.0.0.1` | Bind address (`0.0.0.0` to expose on the LAN). |
| `[server]` | `port` | `8080` | TCP port. |
| `[server]` | `default_format` | `json` | Response format when neither `?format=` nor `Accept` is given. |
| `[server]` | `max_concurrent_jobs` | `2` | Worker threads running slicing jobs concurrently. |
| `[server]` | `max_jobs` | `128` | Tracked-job cap; new submissions get HTTP 429 beyond it. |
| `[server]` | `result_ttl_seconds` | `3600` | Finished-job retention before reaping. |
| `[server]` | `tls_cert` / `tls_key` | *(empty)* | PEM pair enabling HTTPS; must be set together, empty means plain HTTP. |
| `[server]` | `require_tls` | `false` | Refuse to start unless a PEM pair is configured. |
| `[auth]` | `enabled` | *(auto)* | `true` / `false` / empty = auto: auto arms the gate exactly when a token exists. |
| `[auth]` | `token` | *(empty)* | Shared token checked against `Authorization: Bearer` / `X-API-Key`. |
| `[paramstore]` | `backend` | `sqlite` | `sqlite` / `mysql` / `postgresql`; empty disables `/api/params` (503). |
| `[paramstore]` | `sqlite_path` / `host` / `user` / `password` / `database` / `port` / `crypto_password` | | ParamStore connection (per-request). |
| `[upload]` | `dir` | *(empty)* | Root directory for uploaded models; **empty disables** multipart submission (503). |
| `[upload]` | `max_files` | `256` | Retained upload count, floored at `HSBA_MODEL_POOL_SIZE` (10 with CGAL/OCCT, otherwise 1). |
| `[upload]` | `ttl_seconds` | `604800` | Retention in seconds, **counted from the job's terminal state**; `0` disables age-based reclaim (capped at 30 days). |
| `[upload]` | `evict_mode` | `both` | Reclaim policy: `ttl` / `count` / `both` / `manual`; unknown values fall back to `both`. |
| `[upload]` | `max_file_mb` | `256` | Per-file cap; larger parts get 413. |
| `[upload]` | `max_request_mb` | `260` | Whole-body cap including multipart boundaries; raised to `max_file_mb + 4` when lower. |
| `[upload]` | `max_total_mb` | `0` | Directory-wide size cap in MB; `0` = unlimited. |
| `[upload]` | `sweep_interval_seconds` | `60` | Background reclaim period, clamped to `[5, 3600]`. |

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
| `POST` | `/api/jobs?kind={kind}` or `/api/jobs/{kind}` | Submit a slicing job (JSON/XML body, or `multipart/form-data`, see below). | `HsBaRun*Pipeline` (async) |
| `GET` | `/api/jobs?state=&offset=&limit=` | List / paginate jobs (optional state filter). | job queue |
| `GET` | `/api/jobs/{id}` | Job status (state/progress/result metadata). | job queue |
| `GET` | `/api/jobs/{id}/result` | Downloadable artifact (gcode text / output file); the file branch names only the file. | job queue |
| `DELETE` | `/api/jobs/{id}` | Delete a queued or finished job (running jobs are refused); only unlinks its upload. | job queue |
| `GET` | `/api/uploads?state=&offset=&limit=` | List pooled uploads (`pending` / `in_use` / `terminal`). | upload pool |
| `GET` | `/api/uploads/{id}` | Details of a single upload. | upload pool |
| `DELETE` | `/api/uploads/{id}` | Delete one upload (409 while a job still references it). | upload pool |
| `PUT` | `/api/params/{kind}/{key}` | Upsert a config into the ParamStore. | `HsBaSavePipelineParams` |
| `GET` | `/api/params/{kind}/{key}` | Load a config from the ParamStore. | `HsBaLoadPipelineParams` |

`{kind}` is one of: `fdm`, `sla`, `sls`, `slm`, `lom`, `tdp`, `waam`, `custom`,
`filetransfer`.

Spec-verbatim aliases are also served: `GET /api/kinds` (≡ `/api/pipelines`),
`GET /api/config/default/{kind}` (≡ `/api/pipelines/{kind}/default-config`), and
`POST /api/params/{kind}?key=` (≡ `PUT /api/params/{kind}/{key}`).

## Model file upload

Besides a JSON/XML body, `POST /api/jobs/{kind}` accepts **`multipart/form-data`**: a part
named `config` (JSON or XML, selected by that part's `Content-Type`) plus a part named
`model` (the model file).

```sh
curl -X POST http://127.0.0.1:8080/api/jobs/fdm \
     -F 'config=@fdm.json;type=application/json' \
     -F 'model=@cube.stl'
# 202 + Location: /api/jobs/job-7
```

Server-side order of operations:

1. Sanitize the file name: keep only `[A-Za-z0-9._-]`, drop any directory component and
   `..`, truncate to 128 bytes, fall back to `model.bin` when nothing survives.
2. Store it at `<upload_dir>/<job-id>/<safe-name>` and reflect that path into the config's
   `model_path` (plus `model_name` when the struct declares it). The strings are copied
   into the `StringArena`, the same ownership model as `DeserializeConfig` — **never
   malloc'ed**.
3. Register it in the upload pool and submit the job; the entry stays `in_use` until the
   worker is done with it and is never reclaimed meanwhile.

Failure paths: `400` (missing `config`/`model` part, unparsable config, or a kind without
`model_path` — e.g. `filetransfer`), `413` (model larger than `max_file_mb`), `429` (job
queue full), `503` (`[upload] dir` not configured, or the pool is full). Every failure
removes the file that was just written.

### Pool and retention policy

The pool is **fully decoupled from `result_ttl_seconds`**: finishing a job only flips the
entry from `in_use` to `terminal` and starts its clock, while reaping a job (or
`DELETE /api/jobs/{id}`) merely unlinks it — **the model file is never deleted**. Files
are reclaimed only by the pool's own policy:

- `ttl`: measured from `terminal_at` (from `created_at` while unlinked); reclaimed past
  `ttl_seconds`.
- `count`: once the entry count exceeds `max_files`, the oldest `created_at` goes first.
- `both` (default): both rules apply.
- `manual`: only `DELETE /api/uploads/{id}` deletes files.

`in_use` entries are exempt from both `ttl` and `count`; when every entry is referenced by a
live job, a new upload is refused with 503 instead of sacrificing a model being sliced. On
restart the `job-*` directories under `upload_dir` are re-adopted with the directory mtime
as `created_at`, so a restart neither loses track of retained models nor keeps expired ones
forever; directories not named `job-*` are left untouched.

Entry fields: `id` (`upl-<n>`), `job_id`, `file_name` (sanitized), `original_name`
(client-supplied), `path`, `state`, `size_bytes`, `created_at`, `expires_at` (0 = no age
expiry), `remaining_seconds`.

```sh
curl 'http://127.0.0.1:8080/api/uploads?state=terminal&limit=10'
curl http://127.0.0.1:8080/api/uploads/upl-3        # 404 when deleted or never existed
curl -X DELETE http://127.0.0.1:8080/api/uploads/upl-3   # 204; 409 while in_use
```

### Output path

`output_path` in the config is honoured by every kind: the pipeline writes its artifact there
and, once the job reaches a terminal state, `GET /api/jobs/{id}` reports that file in
`output_path` (which is what the console's Output column shows and copies). Which branch
`GET /api/jobs/{id}/result` takes depends on the kind: FDM keeps the G-code in memory and answers
with that text, Custom answers with the Lua script's return value, and every other kind serves the
file named by `export_path`.

The kinds differ in what they write but share one reporting path: `fdm` writes G-code text,
`sla`/`sls`/`slm`/`lom`/`tdp` write a zip package (reported as `export_path`), `waam` writes a robot
path as text, and `custom` writes whatever its script chooses, reporting it through
`HsBa.setOutputPath`. SLS, SLM, LOM and 3DP have no built-in exporter and therefore require
`export_lua_script`, and Custom requires `pipeline_lua_script` or `pipeline_lua_source` to run at
all: without one the job ends `failed` and `output_path` stays empty, so the server never reports a
file it did not write. A Custom script's own file is reachable only through `output_path`, because
its result endpoint is taken by the return value.

The file branch puts only the last component of `output_path` into `Content-Disposition` — never the
server-side directory — and falls back to the job id when that name contains a quote, a backslash or
a control character; the inline-text branch sends no such header. The web console applies the same
rule to pick the save name (adding the extension the kind writes — `.gcode`, `.zip`, `.txt` — when no
header arrived) and adds a **Preview** panel: a text result is rendered up to the first 64 KiB
together with the whole file's byte and line counts, while a binary result is described by name and
size only. This contract is covered by `tests/RestApi/endpoint_http_test.cpp` (the production route
table installed on a real cpp-httplib listener) and by `webui/test/e2e.test.js`, which runs every
file-writing kind against the real binary using the open model the samples download.

## Authorization and TLS

Authorization is enforced **only on non-loopback binds**. A `127.0.0.1` / `::1` /
`localhost` bind stays token-free by design, which keeps local `curl` workflows and
container `HEALTHCHECK` probes working.

```sh
# expose on the LAN over HTTPS, requiring the shared token
HsBaSlicerRestServer --host 0.0.0.0 \
    --tls-cert server.crt --tls-key server.key --require-tls \
    --auth-token "$HSBA_REST_AUTH_TOKEN"

curl -k -H "Authorization: Bearer $HSBA_REST_AUTH_TOKEN" https://host:8080/api/version
curl -k -H "X-API-Key: $HSBA_REST_AUTH_TOKEN"            https://host:8080/api/jobs
curl -k https://host:8080/api/health   # probe: always reachable without credentials
```

Details and deliberate limits:

- Accepted credentials: `Authorization: Bearer <token>` (the `bearer` scheme is
  case-insensitive, and a scheme-less value is compared verbatim) or `X-API-Key: <token>`;
  `Authorization` wins when both are sent. Comparison is constant-time over the length.
- Exempt paths: `/` and `/api/health`. Everything else — including future endpoints —
  is gated by a pre-routing hook.
- Startup fails with a message when `enabled=true` carries an empty token, when only one
  of `tls_cert` / `tls_key` is set, or when `require_tls=true` has no pair.
- Binding a non-loopback address without a token only logs a warning (plaintext is
  reported too); it does not silently pretend to be secure.
- Single shared token, no user accounts, no rotation endpoint, no rate limiting
  (`[rate_limit]` is still reserved). Put a reverse proxy in front when you need those.

## Web console (separate module)

A browser console lives in [`webui/`](../../../webui/README.md). It is deliberately **not** part of
`HsBaSlicerRestServer`: it never links the C++ libraries, is not wired into the CMake/vcpkg
build, and is released on its own schedule with its own CI workflow
(`.github/workflows/rest-webui.yml`). It is a dependency-free Node.js process that serves a
static dashboard and reverse-proxies `/api/*` to this server, so no CORS support is needed here.

```sh
cd webui
node server.js --rest-url http://127.0.0.1:8080 --token "$HSBA_REST_AUTH_TOKEN"
# http://127.0.0.1:5080/
```

Because the proxy calls the API like any other client, the server behaves exactly as documented
above — nothing here changes the endpoint, format, upload or authorization contracts.

## Slicing flow

1. `GET /api/pipelines/{kind}/default-config` to obtain a template.
2. Edit the fields and `POST /api/jobs/{kind}` with the config body → `202` and
   `Location: /api/jobs/{id}`. When the model is not already on a server-side path, send
   `multipart/form-data` instead and attach it in the same request (previous section).
3. Poll `GET /api/jobs/{id}` until `state` is `success` or `failed`.
4. On success fetch `GET /api/jobs/{id}/result`.

## Errors

Failures are uniform `{code, message, detail}` JSON / `<Error>` XML:
`400` invalid kind/body/config conversion or a missing multipart part, `401` missing or
wrong credentials (`WWW-Authenticate: Bearer realm="HsBaSlicer"`), `404` unknown
job/upload/key, `409` job not finished (result), still running (job delete), or upload still
referenced by a job (upload delete), `413` model larger than `[upload] max_file_mb`, `429`
queue full, `503` ParamStore not configured / model upload disabled / upload pool full.
`DELETE /api/jobs/{id}` and `DELETE /api/uploads/{id}` return `204` on success.

## Security note

Loopback bindings are unauthenticated by design; on any other interface a token is
required as soon as `[auth] token` (or `--auth-token` / `HSBA_REST_AUTH_TOKEN`) is set.
Serve TLS on exposed networks — either your own PEM pair or a reverse proxy that
terminates TLS — and never ship an image containing a real token.
