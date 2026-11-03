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
| Result preview | the same `GET /api/jobs/{id}/result` body, rendered as text (`Preview` button), with the G-code's embedded preview images drawn below it and the images of a result package browsed by `GET /api/jobs/{id}/package` + `GET /api/jobs/{id}/package/{entry}` (`Images` button) |
| Layer geometry | `GET /api/jobs/{id}/geometry`, `GET /api/jobs/{id}/geometry/{layer}`, `GET /api/jobs/{id}/geometry/path`, drawn as SVG in the page (Layer / Stack / Side / Path views) |
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

The **Geometry** button on a job row opens the "Layer geometry" panel, which draws the sliced model
as an SVG line picture — no rasterisation on the server, no extra dependency, nothing
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
- **Four views of one snapshot.** `Layer` is the cross-section described above. `Stack` and `Side` show
  the same snapshot from the outside as two fixed orthographic projections, and `Path` draws the single
  uninterrupted line a spiral run deposited. There is no 3D library here: `projectPoint` in
  `httphelpers.js` rotates a model point about the printer z axis by yaw and tilts the camera down by
  pitch, keeping the horizontal screen axis plus the raised one — `pitch = pi/2` looks straight at the
  build plate, `pitch = 0` gives the XZ elevation, which is why `Side` is really the same math snapped to
  two yaws. The math lives in the DOM-free helpers because that is what makes it unit-testable; the page
  only proves it applied the numbers. Dragging a `Stack` changes yaw and pitch (pitch is clamped between
  level and top-down so the view never flips through the plane), dragging a `Side` snaps between the
  front and the side face once the drag passes a fixed pixel step.
- **A stack is sampled, not streamed.** `Stack` and `Side` request at most `GEOMETRY_STACK_LAYERS` (12)
  layers, always including the first and the last so the height of the part is not misrepresented, and
  split the same 3000-node budget over them with `stackBudget(layerCount)` — a twelve-layer stack costs
  what one dense layer costs. Rings of one layer stay one path each, so the short segments the panel
  draws **between** rings of a layer, and between consecutive layers, are *inferred* travel: the engine
  did go there, but only the `Path` view reports a line it actually recorded.
- **Deposition order is a colour, not an animation.** Ticking `order` puts a `stroke` presentation
  attribute from `sequenceColor()` on every drawn element, walking a hue range over the sequence. The
  attribute has lower priority than any CSS rule, so the three category colours are scoped as
  `svg:not(.geometry-ordered) …` in `app.css` — without that scoping the class would not change the
  picture. Unticking removes the attribute and the class, which restores the category colours without
  a single request.
- **`Path` is gated by the server's own flag.** The view is only reachable when the overview reports
  `has_path: 1`; a click on a run that deposited no continuous line is refused with the sentence naming
  `spiral_mode=1`, logged as an error, and the panel stays on the view it can actually draw — switching
  to another job keeps the chosen view but falls back to `Layer` when the new snapshot has no path. The
  polyline is drawn as received, without a travel/extrude distinction, and a `404 job emitted no
  continuous path` from `/geometry/path` is quoted verbatim rather than paraphrased.

**Cancel** sits next to it and is offered while a row is `queued` or `running`. Cancellation is
cooperative: the server only records the request, so the row keeps running until the pipeline reaches
a stage boundary — the log line says so instead of implying an immediate stop, and the listing is
refreshed right after.

## G-code preview images

A finished FDM row can carry its own picture of the part. Ticking `embed preview images (fdm)` before
submitting patches `gcode_thumbnails: 1` into the JSON config, and the engine then projects the
**original STL triangle mesh** — not the sliced contours, not the G-code motion lines — orthographically
onto XY (`model_top`) and XZ (`model_side`), rasterises each silhouette as a grayscale PNG (white on
black) and writes it into the file header as `; thumbnail begin/end` comment blocks. The console
parses those blocks out of the same `/result` text it already fetched for the preview and shows them
as `<img>` elements, so no second download and no new endpoint are involved.

- **The console never decodes or measures the image.** It only splits the base64 lines back apart and
  hands the result to the browser as a `data:` URI; the PNG was produced by the engine, so the picture
  says what the model looks like, not what the printer will do.
- **A skipped view is stated, not hidden.** Where the engine wrote `; thumbnail skipped name=… reason=…`
  the panel prints that sentence instead of an empty box, quoting its own reason
  (`mesh_too_large`, `empty_silhouette`, `render_failed`, `image_too_large`).
- **Nothing asked for is invented.** A text result with no blocks at all says so and names the switch
  (`no embedded previews, set gcode_thumbnails to 1`). The checkbox is disabled for every other kind:
  SLA gets real images from its package (below), and SLS/SLM/LOM/TDP/WAAM/`custom` produce neither a
  preview nor a previewable text format this panel can draw.
- **Sizes stay inside the preview budget.** The engine caps a projection at 48 KiB of base64 per view,
  so both blocks still fit the 64 KiB the panel reads; the parser additionally bounds itself to
  `GCODE_THUMBNAIL_MAX_BLOCKS` blocks and stops at the first non-comment line, which is why reading a
  few megabytes of G-code costs a few dozen lines.
- **Long edges follow `gcode_thumbnail_size`.** 0 (the default) means 256 px, anything else is clamped
  to `[32, 512]`; the panel labels each image with its own `WxH` rather than assuming the default.

## SLA layer images

A package result (`sla.zip`, and whatever an export script stored for SLS/SLM/LOM/TDP) is more than one
file, and `/result` can only hand the whole archive over as a download. The `Images` button next to
`Result` opens the archive instead: it asks `GET /api/jobs/{id}/package` for the listing, keeps the
entries whose `media_type` is an image, orders them by the first number in their name — so
`layers/layer_2.png` comes before `layers/layer_10.png` — and shows the first one. Prev/Next only move
that index; the listing is fetched once and the browser loads each entry from
`GET /api/jobs/{id}/package/{entry}` as an ordinary same-origin `<img src>`.

- **The panel is honest about "no pictures".** A package whose entries are all non-images states that
  SLA exports PNG/JPG/SVG layers; a single-file result quotes the server's own `result is not a
  package` answer. Both also stay out of the way — the stage is hidden, not left as an empty frame,
  and the sentence itself is written into `#package-meta`, which sits *outside* that stage: a
  message inside a hidden container is invisible precisely when it is the only feedback on screen.
- **Only a previewed job can be browsed.** Pressing `Images` before `Preview` asks for a preview first
  rather than guessing which job is meant, and a browse is retired as soon as the job it belongs to
  leaves the listing — a refresh that still shows the same job keeps the layer you were looking at.
  Pressing `Preview` on another job retires both strips (the package images and the embedded G-code
  previews) with it, because either one left standing would show that job's picture under this job's
  heading.
- **Names are re-encoded, not reinterpreted.** `packageEntryUrl` encodes each path segment separately,
  so a stored folder name survives the round trip and the server's own traversal check stays in charge
  of what is addressable.

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
- The picture features need no relaxation of that policy. `img-src 'self' data:` is already part of
  the CSP: an embedded G-code preview is a `data:` URI and a package image is a same-origin `/api/…`
  URL, so neither opens a new origin. Package entries are served with `X-Content-Type-Options:
  nosniff` (and `Content-Security-Policy: default-src 'none'` for SVG), and the proxy passes both
  through untouched, so an archive entry cannot be reinterpreted into something executable.
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
geometry maths (ring → path string, Y mirroring, viewBox arithmetic, the drawn-path budget, the
orthographic projection and layer sampling of the 3D views, the sequence colours) are
tested as real function calls, not as string matches — as are the two picture features: the G-code
thumbnail parser (a complete block, several blocks, a skip notice, the stop at the first command line,
the block and base64 bounds) and the package helpers (per-segment URL encoding, the image filter and
numeric ordering, the JSON-only submission flag). Keeping that maths in the DOM-free helpers is
what makes it testable at all: the page then only has to prove it applied the numbers.

The page logic itself is executed too: `test/server.test.js` runs the served `app.js` in a `node:vm`
context against a minimal DOM (elements created on demand by id, working `localStorage`, a `fetch`
bound to the console origin, no-op timers) and then presses the buttons a user would. That covers
rendering the job rows, previewing an inline text result under its fallback name, refusing to decode
a binary result, downloading it under the name from `Content-Disposition`, and clearing the panel —
plus drawing an embedded preview from a `data:` URI, quoting a skip reason, browsing a package with
exactly one listing request, and refusing to send the preview-image switch to a pipeline that has no
such field — because a checkbox that stays enabled for a kind it cannot affect is a broken control.
All of it is checked together with the log holding no error line, since `run()` reports failures there
instead of throwing. The SVG drawing is checked through that same fake: it implements `setAttribute` and
`createElementNS`, so an assertion can read the `d` string of each `<path>` the page created and
compare it with the mirrored coordinates the real layer carries — a picture built with
`createElement("path")`, or a `d` written to a plain field instead of an attribute, would not pass.
No packages are needed.
Because that fake hands out every id as its own detached node, it cannot see the page's *nesting*, and
nesting is where two real bugs hid: a heading filled with `textContent` deleted the `Images` and
`Clear` buttons built inside it, and a "no pictures" sentence written into the hidden strip was never
readable. Those are therefore asserted against the served HTML instead: every id `app.js` fills with
`textContent` must be a leaf element, and the package caption must not be a descendant of the strip.
The switch was also checked the other way round: with the buttons moved into a plain `<span>` the new
assertions pass, and putting the id back on the `<h2>` fails with
`<h2>#preview-title holds child elements the script would destroy`.

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
FDM case additionally checks what the served preview helpers decide about genuine G-code. Two cases
then go through the archive itself: the SLA run's `sla.zip` is listed and one of its layer entries is
read back and checked as a real PNG (signature, IHDR size, uncompressed length, `nosniff`), and an FDM
run asked for previews has its `; thumbnail` blocks parsed out of the served G-code and decoded back to
a PNG whose header agrees with the block's own `width`/`height`/`b64len` — while a run without the
switch still starts at `; Generated by HsBaSlicer`. Endpoint listing alone proves nothing; the files
have to exist.

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
matrix. The geometry endpoints are covered against the real server twice: a pass-through case proves a
three-segment job sub-resource reaches it intact, and `captures and serves the real spiral path` slices
the sample model with `?geometry=1` and `spiral_mode=1`, then reads the summary (`has_path`), the
polyline and its monotone `z` back from `/geometry/path`. That live-slice snapshot stays a single case,
because geometry only exists once a job is terminal and the pool keeps just the last few snapshots.

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
