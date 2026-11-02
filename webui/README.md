# HsBaSlicer REST Console (`webui/`)

A standalone Web UI for [`HsBaSlicerRestServer`](../restapi/) that runs from the command line.

The console is **not** part of `HsBaSlicerRestServer`: it never links, loads or imports the
C++ libraries, and it is not wired into the CMake build. It is a plain HTTP client of the REST
API plus a same-origin proxy, written in dependency-free Node.js (standard library only), with
its own CI workflow (`.github/workflows/rest-webui.yml`).

```
browser ──HTTP──▶ webui/server.js ──HTTP──▶ HsBaSlicerRestServer
                  (static bundle +             (unchanged, optional
                   /api/* proxy)                HSBA_BUILD_REST_API build)
```

## Quick start

```sh
# 1. start the REST server (separate module, see ../docs/en/restapi/README.md)
HsBaSlicerRestServer --config restapi.ini

# 2. start the console and open the printed URL
cd webui
node server.js                       # http://127.0.0.1:5080/
node server.js --rest-url http://127.0.0.1:8080 --token "$HSBA_REST_AUTH_TOKEN"
node server.js --port 0              # let the OS pick a free port
node server.js --help
```

There is no install step: `package.json` declares no dependencies, so `node server.js` works on
a fresh checkout with Node.js 20 or newer.

## Command line

| Flag | Default | Meaning |
|---|---|---|
| `--rest-url <url>` | `http://127.0.0.1:8080` | Base URL of the REST server. `https` and a path prefix (`http://host:8080/hsba`) are supported. |
| `--host <addr>` | `127.0.0.1` | Bind address of the console itself. Anything but loopback logs a warning. |
| `--port <n>` | `5080` | Listen port; `0` asks the OS for an ephemeral port. |
| `--token <t>` | *(empty)* | Shared REST token; the proxy injects it as `X-API-Key`, so the browser never sees it. |
| `--help`, `-h` | | Print usage. |

Environment fallbacks, used only when the matching flag is absent:
`HSBA_REST_URL`, `HSBA_WEBUI_HOST`, `HSBA_WEBUI_PORT`, `HSBA_REST_AUTH_TOKEN`.

## What the page does

| Panel | Calls |
|---|---|
| Health / version badge | `GET /api/health`, `GET /api/version` |
| Slicing | `GET /api/pipelines`, `GET /api/config/default/{kind}?format=json`, `POST /api/jobs/{kind}` (JSON body or `multipart/form-data` with `config` + `model`; the `capture geometry` checkbox appends `?geometry=1`) |
| Jobs | `GET /api/jobs?limit=50`, `GET /api/jobs/{id}/result`, `POST /api/jobs/{id}/cancel`, `DELETE /api/jobs/{id}` |
| Result preview | the same `GET /api/jobs/{id}/result` body, rendered as text (`Preview` button) |
| Layer geometry | `GET /api/jobs/{id}/geometry`, `GET /api/jobs/{id}/geometry/{layer}`, drawn as SVG in the page |
| Uploads | `GET /api/uploads?limit=50`, `DELETE /api/uploads/{id}` |
| Log | client-side transcript of the actions above (including error payloads) |

Job listing polls every 3 s (`auto` checkbox) and the health probe every 5 s. The poll keeps one row
per job id and rewrites its cells in place: a row that is already on screen is not replaced, so the
hovered row and a click in flight survive the refresh, and only a newly appearing job inserts a row.
A cell is only touched when its text actually changed — assigning `textContent` with the identical
string still swaps out the text node, which would drop a text selection made in the path column.
Rows keep the order the API returns them in.

The **Jobs** table has an `Output` column with the server-side result path the API reports in
`output_path` (the generated toolpath/archive, e.g. `/data/hsba/jobs/<id>/result.gcode`). Every
kind reports it, `fdm` included: that pipeline writes the G-code to the config's `output_path`
and the job view echoes it back. The cell renders it in full — a long path wraps over several
lines instead of being clipped to an ellipsis — and clicking it copies the path to the clipboard.
Jobs that produced nothing show their `error_message` in the same column, and the state cell
keeps the error in its tooltip.

Each finished row also has a **Preview** button next to **Result**: it fetches the same
`/api/jobs/{id}/result` response and renders the beginning of a text body (G-code) in the
"Result preview" panel, reporting the whole file's byte and line counts and stating when only
the first 64 KiB are shown. A binary body is never decoded — the panel names it and points at
**Result**, which still downloads the complete file under the name the server asked for
(`Content-Disposition`, normalized to a bare file name; job id + `.gcode` when the inline branch
sent no header). Both behaviours come from `public/httphelpers.js`, which is deliberately DOM-free
so the unit tests can run it outside a browser.

The **Uploads** table has a `Stored path` column holding the server-side file behind each model (the
`path` field of `GET /api/uploads`), wrapped in full and copied on click like the job paths — the
file name alone does not tell a CLI user where the model landed. When the pool is switched off
(no `[upload] dir` in `restapi.ini`), that table states the server's answer instead of failing an
action.

## Layer geometry preview

The **Geometry** button on a job row opens the "Layer geometry" panel, which draws one layer of the
sliced model as an SVG line picture — no rasterisation on the server, no extra dependency, nothing
added to the C++ build. The endpoints themselves are documented in
[`../docs/en/restapi/README.md`](../docs/en/restapi/README.md); what the panel adds is a viewer.

- **Capture is opt-in per job.** Geometry only exists for jobs submitted with the `capture geometry`
  checkbox on, which appends `?geometry=1` to `POST /api/jobs/{kind}`. The button is enabled by the
  `geometry_available` flag the listing reports, so a job sliced without capture never offers an
  action that must fail.
- **Terminal state only.** Capture happens in the last stages of a pipeline, so `geometry_available`
  stays `0` while a job is `queued` or `running`; a row becomes previewable when it finishes.
- **Old snapshots disappear.** The server holds at most `HSBA_GEOMETRY_POOL_MAX` snapshots (default
  `4`), so slicing a few more parts can retire the one you were looking at. The panel does not invent
  a message for that: it quotes the server's own `404` body (`geometry snapshot is no longer
  available`, `job captured no layer geometry`) in the panel text and logs it as an error.
- **One layer at a time.** A slider with `-`/`+` buttons steps between layers, the mouse wheel zooms
  around the centre and dragging pans; **Fit** resets both. Outlines, infill and supports are three
  separately coloured categories, each with its own checkbox — clearing one only redraws, it never
  refetches. `Reload layer` re-requests the current layer.
- **Category slots depend on the process.** FDM fills outlines, fills and supports; the SLA
  raft/bottom sheet shows up only in the fills of layer 0; SLS, SLM, LOM, TDP and WAAM provide
  outlines only; `custom` and `filetransfer` capture nothing at all. An empty category simply is not
  drawn, so a blank layer means the process produced nothing there — not that the panel is broken.
- **Y is mirrored.** The slicer's coordinate frame grows towards the viewer while SVG grows down, so
  every ring is reflected about the bounding box's mid-line (`2 * mirror_y - y`). Mirroring about the
  *centre* rather than about `y = 0` is what keeps the viewBox derived from the raw bounds valid, and
  it lets a drag pan in millimetres without re-flipping the axis.
- **Strokes stay thin when zoomed.** All three categories are drawn unfilled with
  `vector-effect: non-scaling-stroke`: the viewBox is measured in millimetres, so without it a zoomed
  layer would show bands instead of lines, and filling closed contours would hide the infill inside.
- **Big layers are bounded, not silently slow.** A single layer of a dense model can carry tens of
  thousands of rings, so the panel draws at most `GEOMETRY_MAX_PATHS` (3000) `<path>` nodes, spending
  the budget on outlines and supports first, and states how many were left undrawn.

**Cancel** sits next to it and is offered while a row is `queued` or `running`. Cancellation is
cooperative: the server only records the request, so the row keeps running until the pipeline reaches
a stage boundary — the log line says so instead of implying an immediate stop, and the listing is
refreshed right after.

## Why a proxy instead of calling the API directly

- **No CORS changes on the C++ side.** The page and `/api/*` share one origin, so
  `HsBaSlicerRestServer` keeps its current header behaviour.
- **Token handling.** Without `--token`, the page stores the token in `localStorage` and sends it
  as `X-API-Key`, which the proxy forwards. With `--token`, the proxy overwrites the header, so
  the secret stays on the server host and an operator can hand out a token-free console URL.
- **Isolation.** The console can be upgraded, replaced or dropped without touching the C++
  build, and a bug in it cannot destabilise the slicing process.

## Security notes

- The console binds to `127.0.0.1` by default; it has no authentication of its own, so only
  expose it on a trusted network (a warning is printed otherwise).
- Only `GET`, `HEAD`, `POST`, `PUT` and `DELETE` under `/api/*` are proxied; every other path
  is served from a fixed asset allowlist, so no file-system path is derived from request input.
- Hop-by-hop headers are dropped, and `Cookie`, `Origin` and `Referer` are not forwarded.
- The HTML is served under `Content-Security-Policy: default-src 'self'`, and the bundle has no
  inline script/style and no external URL — it works on an air-gapped network. The one URI literal in
  the bundle is the `http://www.w3.org/2000/svg` namespace passed to `createElementNS`; it is an
  identifier the browser never dereferences, so it triggers no request (and `default-src 'self'`
  would block one anyway). A test extracts every URL literal from the scripts to keep that claim
  checkable rather than eyeballed.
- TLS to the REST server is verified by Node's default CA store; point `--rest-url` at the
  `--tls-cert`/`--tls-key` endpoint of the REST server to avoid plaintext traffic.

## Tests

```sh
cd webui
node --test                # or: npm test — fake upstream, runs anywhere
npm run test:e2e           # real HsBaSlicerRestServer, skipped when absent
```

`test/server.test.js` starts a fake REST server (a recording `node:http` server with a small
route table) plus a real console on an ephemeral port, then drives the console over HTTP. It
covers the static bundle and CSP, the proxy contract (method, query, body, multipart, headers),
credential forwarding and injection, status/`Content-Disposition` passthrough, 502 on an
unreachable upstream, the method allowlist, `/healthz`, the page contract (a jobs table whose
column count matches `JOB_COLUMNS` and whose `Output` column is fed by `output_path`/`error_message`,
the preview panel and its `Preview`/`Result` buttons, the geometry panel and its `Geometry`/`Cancel`
buttons, plus every `/api/…` literal being on the proxy
allowlist) and the CLI/env parsing rules. `public/httphelpers.js` is evaluated in a `node:vm`
sandbox exactly as the browser loads it, so the download-name rules (quoted, unquoted, path-bearing,
RFC 5987 `filename*`, broken percent-escape, missing header — including the per-kind fallback that
names a package `job-7.zip` and inline G-code `job-7.gcode`), the preview truncation budget and the
geometry maths (ring → path string, Y mirroring, viewBox arithmetic, the drawn-path budget) are
tested as real function calls, not as string matches. Keeping that maths in the DOM-free helpers is
what makes it testable at all: the page then only has to prove it applied the numbers.

The page logic itself is executed too: `test/server.test.js` runs the served `app.js` in a `node:vm`
context against a minimal DOM (elements created on demand by id, working `localStorage`, a `fetch`
bound to the console origin, no-op timers) and then presses the buttons a user would. That covers
rendering the job rows, previewing an inline text result under its fallback name, refusing to decode
a binary result, downloading it under the name from `Content-Disposition`, and clearing the panel —
plus a check that the browser log holds no error line, because `run()` reports failures there instead
of throwing. The SVG drawing is checked through that same fake: it implements `setAttribute` and
`createElementNS`, so an assertion can read the `d` string of each `<path>` the page created and
compare it with the mirrored coordinates the real layer carries — a picture built with
`createElement("path")`, or a `d` written to a plain field instead of an attribute, would not pass.
No packages are needed.

`test/e2e.test.js` is the real end-to-end case: it starts the actual `HsBaSlicerRestServer`
(loopback, generated temporary `restapi.ini`, upload pool rooted in the temp tree) and waits for
`/api/health`. Its model is the same public-domain Stanford Bunny the samples build downloads —
`samples/FDM/CMakeLists.txt` fetches that URL into that path — which the test reuses from
`$HSBA_SAMPLE_MODEL`, from that path, or by downloading it once, and it sizes the layer height from
the model's own bounding box. So a real 112k-triangle part is sliced, rather than a mesh invented
for the occasion, and one run also covers the multipart upload path byte for byte.

What it proves is the output contract of **every** pipeline that writes a file, not just FDM:
`fdm`, `sla`, `sls`, `slm`, `lom`, `tdp`, `waam` and `custom` are each submitted — with the sample
Lua scripts where a pipeline has no built-in exporter, since SLS/SLM/LOM/3DP refuse to export
without one and Custom will not run at all — and then checked for reporting the very path it was
given, for a non-empty file of that exact size behind it, for appearing in the job listing the page
renders, and for `/api/jobs/{id}/result` serving that file under a bare download name, packages
included (`PK` at the front). A pipeline that cannot export must also stay silent about a path, so
SLS without a script is asserted to fail with an empty `output_path` and a 409 result. The inline
FDM case additionally checks what the served preview helpers decide about genuine G-code. Endpoint
listing alone proves nothing; the files have to exist.

The binary is taken from `$HSBA_REST_SERVER`, else from the CMake output trees (`out/build`,
`out/install`, `build`), preferring a Release build because these cases measure a real workload. When
no binary exists the case reports as **skipped** instead of failing, so the C++ build stays optional:

```sh
# build once with the rest-api feature, then point the console test at the binary
cmake --preset x64-release -DHSBA_BUILD_REST_API=ON -DVCPKG_MANIFEST_FEATURES=rest-api
cmake --build out/build/x64-release --target HsBaSlicerRestServer
cd webui
HSBA_REST_SERVER=../out/build/x64-release/bin/Release/HsBaSlicerRestServer.exe npm run test:e2e
```

CI (`.github/workflows/rest-webui.yml`) runs the syntax checks, `node --test` on Node 20/22/24,
and a CLI smoke test that boots `server.js` and fetches the page and script. It is triggered only
by changes under `webui/`, so the C++ pipeline is untouched; the end-to-end case reports as
skipped there because that workflow does not build `restapi`. It runs for real in the Linux leg of
`cmake-multi-platform.yml`, which enables `HSBA_BUILD_REST_API` and fetches the sample model in a
dedicated step first, so an unreachable model host fails that job instead of quietly skipping the
matrix. The geometry endpoints are covered against the real server by a pass-through case that
proves a three-segment job sub-resource reaches it intact; no snapshot is read from a live slice,
because geometry only exists once a job is terminal and the pool keeps the last few snapshots.

## Layout

```
webui/
├── server.js            CLI, static bundle server, /api/* proxy
├── package.json         metadata + scripts (no dependencies)
├── public/
│   ├── index.html       dashboard markup (no inline script/style)
│   ├── app.css          dark theme
│   ├── httphelpers.js   DOM-free download-name, preview and geometry maths (unit tested)
│   ├── app.js           dashboard logic (fetch + rendering)
│   └── favicon.svg
└── test/
    ├── server.test.js   node:test integration suite (fake upstream)
    └── e2e.test.js      node:test end-to-end case (real server, sample model, every output-producing
                       pipeline; skips when no server binary is built)
```
