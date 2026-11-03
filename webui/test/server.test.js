/**
 * @file server.test.js
 * @brief Integration tests for the standalone REST console (webui/server.js).
 *
 * Each case starts a fake HsBaSlicerRestServer (a plain node:http server with a small route
 * table) plus a real console instance on an ephemeral port, then talks to the console over
 * HTTP. That keeps the test dependency-free (`node --test`, no packages) while still covering
 * the contract the browser relies on: the static bundle, the /api/* proxy, credential
 * forwarding/injection, status passthrough and the CLI surface.
 *
 * @author HsBa
 */
import assert from "node:assert/strict";
import http from "node:http";
import { describe, it } from "node:test";
import vm from "node:vm";

import {
  DEFAULTS,
  UsageError,
  consoleUrl,
  createConsoleServer,
  parseCliArgs,
  startConsole,
} from "../server.js";

// ---------------------------- fixtures ----------------------------

function listen(server) {
  return new Promise((resolve, reject) => {
    server.once("error", reject);
    server.listen(0, "127.0.0.1", () => {
      server.removeListener("error", reject);
      resolve(server);
    });
  });
}

async function closeServer(server) {
  if (!server) {
    return;
  }
  server.closeAllConnections?.();
  await new Promise((resolve) => server.close(() => resolve()));
}

function originOf(server) {
  return `http://127.0.0.1:${server.address().port}`;
}

function sendJson(res, status, payload) {
  const body = JSON.stringify(payload);
  res.writeHead(status, {
    "Content-Type": "application/json",
    "Content-Length": Buffer.byteLength(body),
  });
  res.end(body);
}

/** The job listing the console boots with: one binary artifact and one inline text result. */
const DEFAULT_JOBS = [
  {
    id: "job-1", kind: "fdm", state: "success", success: 1, progress_percent: 100,
    progress_stage: "done", progress_stage_id: "done", total_layers: 5, elapsed_seconds: 1.25,
    output_path: "/data/hsba/jobs/job-1/part.gcode", error_message: "",
    geometry_layers: 2, geometry_available: 1,
  },
  {
    id: "job-2", kind: "fdm", state: "success", success: 1, progress_percent: 100,
    progress_stage: "done", progress_stage_id: "done", total_layers: 3, elapsed_seconds: 0.5,
    output_path: "/data/hsba/jobs/job-2/inline.gcode", error_message: "",
    geometry_layers: 0, geometry_available: 0,
  },
];

/** The geometry of job-1, shaped exactly like GeometrySummaryView/LayerGeometryView. */
const GEOMETRY_SUMMARY = { id: "job-1", geometry_id: 7, layer_count: 2, bounds: [0, 0, 20, 10], has_path: 1 };
// The same panel for a run that only emitted per-layer rings: has_path is what gates the Path button.
const GEOMETRY_NO_PATH = { id: "job-2", geometry_id: 8, layer_count: 2, bounds: [0, 0, 20, 10], has_path: 0 };
const GEOMETRY_LAYERS = [
  {
    id: "job-1", layer_index: 0, z_height: 0.3, bounds: [0, 0, 20, 10],
    outlines: [{ point_count: 4, closed: 1, xy: [0, 0, 20, 0, 20, 10, 0, 10] }],
    fills: [{ point_count: 2, closed: 0, xy: [2, 2, 18, 2] }],
    supports: [],
  },
  {
    id: "job-1", layer_index: 1, z_height: 0.6, bounds: [2, 2, 18, 8],
    outlines: [{ point_count: 4, closed: 1, xy: [2, 2, 18, 2, 18, 8, 2, 8] }],
    fills: [{ point_count: 4, closed: 1, xy: [4, 4, 16, 4, 16, 6, 4, 6] }],
    supports: [{ point_count: 2, closed: 0, xy: [10, 2, 10, 8] }],
  },
];

/**
 * GeometryPathView of a spiral run: one uninterrupted polyline whose z rises as it winds around.
 * xyz is flat, so the body is read three numbers at a time exactly like routes.cpp writes it.
 */
const GEOMETRY_PATH_BODY = {
  id: "job-1",
  geometry_id: 7,
  point_count: 6,
  xyz: [0, 0, 0.3, 20, 0, 0.7, 20, 10, 1.1, 0, 10, 1.5, 0, 0, 1.9, 10, 5, 2.4],
  bounds: [0, 0, 20, 10],
  z_range: [0.3, 2.4],
};

/**
 * G-code of an FDM run that was asked to embed its previews.
 *
 * The blocks are exactly what paths/gcodepath.cpp writes: a begin line of metadata, the Base64
 * chunks, an end line, and one skip notice for the view the engine could not render. They sit at the
 * head of the file, which is what lets a truncated preview still show the pictures.
 */
const THUMBNAIL_BLOCKS = [
  "; thumbnail begin name=model_top width=8 height=8 format=png b64len=8",
  "; aGVsbG8=",
  "; thumbnail end name=model_top",
  "; thumbnail skipped name=model_side reason=mesh_too_large",
].join("\n");
const INLINE_GCODE = THUMBNAIL_BLOCKS + "\nG21\nG90\nG1 X1.0 Y2.0 E3.0 F1200\n";

/**
 * Result package of job-1, listed the way restapi/package_reader.cpp reports an archive: two layer
 * images and a config, sizes present, media types derived from the entry names.
 */
const PACKAGE_LISTING = {
  id: "job-1",
  name: "result.zip",
  entry_count: 3,
  entries: [
    { path: "config.json", size: 20, uncompressed_size: 26, media_type: "application/json" },
    { path: "layers/layer_0.png", size: 13, uncompressed_size: 13, media_type: "image/png" },
    { path: "layers/layer_1.png", size: 13, uncompressed_size: 13, media_type: "image/png" },
  ],
};

/** @brief Bytes of a package image; the console never decodes one, it only hands them to an <img>. */
const PACKAGE_PNG = Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x6c, 0x61, 0x79, 0x65, 0x72]);

/** @brief Minimal stand-in for HsBaSlicerRestServer that also records every request. */
function createFakeRestApi() {
  const server = http.createServer((req, res) => {
    const chunks = [];
    req.on("data", (chunk) => chunks.push(chunk));
    req.on("end", () => {
      const body = Buffer.concat(chunks).toString("utf8");
      server.calls.push({ method: req.method, url: req.url, headers: req.headers, body });
      respond(req, res, body, server.state);
    });
  });
  server.calls = [];
  // Listings a page test can rewrite before triggering a refresh; `uploads: null` keeps the
  // upload pool switched off, which is how a server without [upload] dir answers. The inline body is
  // writable too, so a test can show the page a run that embedded nothing.
  server.state = { jobs: DEFAULT_JOBS, uploads: null, inlineResult: INLINE_GCODE };
  return server;
}

function respond(req, res, body, state) {
  const path = (req.url || "").split("?")[0];
  switch (path) {
    case "/api/health":
      sendJson(res, 200, { status: "ok", uptime_seconds: 12 });
      return;
    case "/api/pipelines":
      sendJson(res, 200, { kinds: [{ name: "fdm" }, { name: "sla" }] });
      return;
    case "/api/config/default/fdm":
      sendJson(res, 200, { layer_height: 0.2, output_path: "" });
      return;
    case "/api/version":
      sendJson(res, 200, { librariesName: "HsBaSlicer", version: "9.9.9", buildType: "Release" });
      return;
    case "/api/uploads":
      if (!state.uploads) {
        // Mirrors the real 503 when [upload] dir is not configured.
        sendJson(res, 503, {
          code: 503,
          message: "model upload disabled",
          detail: "set [upload] dir in restapi.ini",
        });
        return;
      }
      sendJson(res, 200, { count: state.uploads.length, uploads: state.uploads });
      return;
    case "/api/echo":
      sendJson(res, 200, {
        method: req.method,
        url: req.url,
        body,
        apiKey: req.headers["x-api-key"] || "",
        contentType: req.headers["content-type"] || "",
        cookie: req.headers.cookie || "",
      });
      return;
    case "/api/jobs/job-1":
      if (req.method === "DELETE") {
        res.writeHead(204);
        res.end();
        return;
      }
      break;
    case "/api/jobs":
      sendJson(res, 200, { count: state.jobs.length, jobs: state.jobs });
      return;
    case "/api/jobs/job-1/result":
      res.writeHead(200, {
        "Content-Type": "application/octet-stream",
        "Content-Disposition": 'attachment; filename="part.gcode"',
      });
      res.end("G1 X0 Y0\n");
      return;
    case "/api/jobs/job-2/result":
      // The FDM inline branch: text without a download header, with its previews embedded.
      res.writeHead(200, { "Content-Type": "text/plain; charset=utf-8" });
      res.end(state.inlineResult);
      return;
    case "/api/jobs/fdm":
      // What routes.cpp answers for an accepted submission; the body of the call is what the page
      // tests read back out of `upstream.calls`.
      sendJson(res, 202, { id: "job-9", state: "queued", detail: "accepted" });
      return;
    case "/api/jobs/job-1/package":
      sendJson(res, 200, PACKAGE_LISTING);
      return;
    case "/api/jobs/job-1/package/config.json":
      res.writeHead(200, { "Content-Type": "application/json", "X-Content-Type-Options": "nosniff" });
      res.end('{"kind":"fdm","layers":3}');
      return;
    case "/api/jobs/job-1/package/layers/layer_0.png":
    case "/api/jobs/job-1/package/layers/layer_1.png":
      res.writeHead(200, { "Content-Type": "image/png", "X-Content-Type-Options": "nosniff" });
      res.end(PACKAGE_PNG);
      return;
    case "/api/jobs/job-2/package":
      // A G-code answer is one file, not an archive, and the server says which of the two it is.
      sendJson(res, 404, { code: 404, message: "result is not a package", detail: "inline result" });
      return;
    case "/api/jobs/job-1/geometry":
      sendJson(res, 200, GEOMETRY_SUMMARY);
      return;
    case "/api/jobs/job-1/geometry/path":
      sendJson(res, 200, GEOMETRY_PATH_BODY);
      return;
    case "/api/jobs/job-2/geometry":
      sendJson(res, 200, GEOMETRY_NO_PATH);
      return;
    case "/api/jobs/job-2/geometry/0":
      sendJson(res, 200, GEOMETRY_LAYERS[0]);
      return;
    case "/api/jobs/job-2/geometry/path":
      // The answer a non-spiral run gets: the endpoint exists, the snapshot holds nothing to send.
      sendJson(res, 404, { code: 404, message: "job emitted no continuous path", detail: "spiral_mode=1" });
      return;
    case "/api/jobs/job-1/geometry/0":
      sendJson(res, 200, GEOMETRY_LAYERS[0]);
      return;
    case "/api/jobs/job-1/geometry/1":
      sendJson(res, 200, GEOMETRY_LAYERS[1]);
      return;
    case "/api/jobs/job-1/geometry/2":
      // What routes.cpp answers for an index the snapshot does not hold.
      sendJson(res, 404, { code: 404, message: "layer index out of range", detail: "layer_count=2" });
      return;
    case "/api/jobs/job-3/geometry":
      // A job the operator never asked to capture: the server's own sentence is the whole story.
      sendJson(res, 404, {
        code: 404,
        message: "job captured no layer geometry",
        detail: "submit with ?geometry=1",
      });
      return;
    case "/api/jobs/job-1/cancel":
    case "/api/jobs/job-3/cancel": {
      const id = path.split("/")[3];
      sendJson(res, 202, { id: id, state: "cancelled", detail: "cancellation requested" });
      return;
    }
    case "/api/jobs/job-4/cancel":
      // A finished job cannot be cancelled, and the server says so with 409.
      sendJson(res, 409, { code: 409, message: "job already finished", detail: "state=success" });
      return;
    default:
      break;
  }
  sendJson(res, 404, { code: 404, message: "not found", detail: path });
}

/** @brief Run a case against a live console paired with a recording fake REST server. */
async function withConsole(options, run) {
  const upstream = await listen(createFakeRestApi());
  const consoleServer = await startConsole({
    restUrl: originOf(upstream),
    host: "127.0.0.1",
    port: 0,
    token: "",
    ...options,
  });
  try {
    await run({ upstream, consoleServer, base: originOf(consoleServer) });
  } finally {
    await closeServer(consoleServer);
    await closeServer(upstream);
  }
}

// ---------------------------- static bundle ----------------------------

describe("static bundle", () => {
  it("serves the console page with a strict content security policy", async () => {
    await withConsole({}, async ({ base }) => {
      const response = await fetch(`${base}/`);
      assert.equal(response.status, 200);
      assert.match(response.headers.get("content-type"), /^text\/html/);
      assert.match(response.headers.get("content-security-policy"), /default-src 'self'/);
      assert.equal(response.headers.get("x-content-type-options"), "nosniff");

      const html = await response.text();
      assert.match(html, /<title>HsBaSlicer REST Console<\/title>/);
      assert.match(html, /href="\/app\.css"/);
      assert.match(html, /src="\/app\.js"/);
      assert.match(html, /src="\/httphelpers\.js"/);
      // The bundle must stay self-contained: no inline script/style, no external URL.
      assert.doesNotMatch(html, /https?:\/\//);
      assert.doesNotMatch(html, /<script(?![^>]*src=)/);
    });
  });

  it("loads the http helpers before the code that calls them", async () => {
    await withConsole({}, async ({ base }) => {
      const html = await (await fetch(`${base}/`)).text();
      // Deferred scripts run in document order, so app.js may only reach HsBaHttp once the
      // helpers are already evaluated.
      const helpers = html.indexOf('src="/httphelpers.js"');
      const app = html.indexOf('src="/app.js"');
      assert.ok(helpers > 0, "httphelpers.js is not referenced");
      assert.ok(app > helpers, "app.js is loaded before the helpers");
    });
  });

  it("serves every asset with its own media type", async () => {
    await withConsole({}, async ({ base }) => {
      const expectations = [
        ["/index.html", /^text\/html/],
        ["/app.css", /^text\/css/],
        ["/httphelpers.js", /^text\/javascript/],
        ["/app.js", /^text\/javascript/],
        ["/favicon.svg", /^image\/svg\+xml/],
      ];
      for (const [path, expected] of expectations) {
        const response = await fetch(base + path);
        assert.equal(response.status, 200, path);
        assert.match(response.headers.get("content-type"), expected, path);
      }
      const script = await (await fetch(`${base}/app.js`)).text();
      assert.match(script, /\/api\/pipelines/);
      assert.match(script, /X-API-Key/);
      // The bundle must still fetch nothing off-origin. The one URI literal it does carry is the SVG
      // namespace, which is an identifier passed to createElementNS and never dereferenced, so it
      // neither breaks the `default-src 'self'` policy nor the air-gapped claim.
      const urls = [...script.matchAll(/"(https?:\/\/[^"]*)"/g)].map((match) => match[1]);
      assert.deepEqual(urls.filter((url) => url !== "http://www.w3.org/2000/svg"), []);

      // The download-name and preview logic lives in the helpers, which stay DOM-free so the
      // cases below can run them outside a browser.
      const helpers = await (await fetch(`${base}/httphelpers.js`)).text();
      assert.doesNotMatch(helpers, /\b(document|window|localStorage)\b/);
      assert.match(helpers, /globalThis\.HsBaHttp/);
    });
  });

  it("answers HEAD without a body", async () => {
    await withConsole({}, async ({ base }) => {
      const response = await fetch(`${base}/`, { method: "HEAD" });
      assert.equal(response.status, 200);
      assert.ok(Number(response.headers.get("content-length")) > 0);
      assert.equal(await response.text(), "");
    });
  });

  it("rejects unknown and traversal-looking paths instead of touching the file system", async () => {
    await withConsole({}, async ({ base }) => {
      for (const path of ["/secrets", "/%2e%2e/server.js", "/../package.json", "/app.js.bak"]) {
        const response = await fetch(base + path);
        assert.equal(response.status, 404, path);
        const body = await response.text();
        assert.equal(body, "not found\n", path);
      }
    });
  });

  it("refuses non-GET methods outside the proxy", async () => {
    await withConsole({}, async ({ base }) => {
      const response = await fetch(`${base}/app.js`, { method: "POST" });
      assert.equal(response.status, 405);
      const payload = await response.json();
      assert.equal(payload.code, 405);
    });
  });
});

// ---------------------------- api proxy ----------------------------

describe("api proxy", () => {
  it("forwards GET requests and returns the JSON body verbatim", async () => {
    await withConsole({}, async ({ base, upstream }) => {
      const response = await fetch(`${base}/api/health`);
      assert.equal(response.status, 200);
      const payload = await response.json();
      assert.deepEqual(payload, { status: "ok", uptime_seconds: 12 });
      assert.equal(upstream.calls.length, 1);
      assert.equal(upstream.calls[0].method, "GET");
      assert.equal(upstream.calls[0].url, "/api/health");
    });
  });

  it("forwards method, query string, body and content type", async () => {
    await withConsole({}, async ({ base, upstream }) => {
      const config = '{"layer_height":0.2}';
      const response = await fetch(`${base}/api/echo?kind=fdm&format=json`, {
        method: "POST",
        headers: { "Content-Type": "application/json", Accept: "application/json" },
        body: config,
      });
      assert.equal(response.status, 200);
      const payload = await response.json();
      assert.equal(payload.method, "POST");
      assert.equal(payload.url, "/api/echo?kind=fdm&format=json");
      assert.equal(payload.body, config);
      assert.match(payload.contentType, /^application\/json/);
      assert.equal(upstream.calls[0].body, config);
    });
  });

  it("forwards multipart uploads byte for byte", async () => {
    await withConsole({}, async ({ base, upstream }) => {
      const form = new FormData();
      form.append("config", new Blob(['{"kind":"fdm"}'], { type: "application/json" }), "config.json");
      form.append("model", new Blob(["solid cube\n"], { type: "model/stl" }), "cube.stl");

      const response = await fetch(`${base}/api/jobs/sla`, { method: "POST", body: form });
      assert.equal(response.status, 404); // the fake API has no /api/jobs/sla route
      const forwarded = upstream.calls[0];
      assert.match(forwarded.headers["content-type"], /^multipart\/form-data; boundary=/);
      assert.match(forwarded.body, /name="config"/);
      assert.match(forwarded.body, /name="model"; filename="cube\.stl"/);
      assert.match(forwarded.body, /solid cube/);
    });
  });

  it("forwards the browser token and drops cross-context headers", async () => {
    await withConsole({}, async ({ base }) => {
      const response = await fetch(`${base}/api/echo`, {
        headers: { "X-API-Key": "browser-token", Cookie: "session=leak", Referer: "http://evil" },
      });
      const payload = await response.json();
      assert.equal(payload.apiKey, "browser-token");
      assert.equal(payload.cookie, "");
    });
  });

  it("injects the launch token and overrides whatever the browser sent", async () => {
    await withConsole({ token: "server-side-token" }, async ({ base, upstream }) => {
      const response = await fetch(`${base}/api/echo`, { headers: { "X-API-Key": "browser-token" } });
      const payload = await response.json();
      assert.equal(payload.apiKey, "server-side-token");
      assert.equal(upstream.calls[0].headers["x-api-key"], "server-side-token");
    });
  });

  it("keeps a path prefix configured in --rest-url", async () => {
    const upstream = await listen(createFakeRestApi());
    const consoleServer = await startConsole({
      restUrl: `${originOf(upstream)}/hsba/`,
      host: "127.0.0.1",
      port: 0,
    });
    try {
      await fetch(`${originOf(consoleServer)}/api/health?probe=1`);
      assert.equal(upstream.calls[0].url, "/hsba/api/health?probe=1");
    } finally {
      await closeServer(consoleServer);
      await closeServer(upstream);
    }
  });
});

// ---------------------------- passthrough and failures ----------------------------

describe("status and header passthrough", () => {
  it("passes error statuses and error payloads through unchanged", async () => {
    await withConsole({}, async ({ base }) => {
      const response = await fetch(`${base}/api/uploads`);
      assert.equal(response.status, 503);
      const payload = await response.json();
      assert.equal(payload.code, 503);
      assert.equal(payload.message, "model upload disabled");
      assert.match(payload.detail, /\[upload\] dir/);
    });
  });

  it("passes downloads through with Content-Disposition", async () => {
    await withConsole({}, async ({ base }) => {
      const response = await fetch(`${base}/api/jobs/job-1/result`);
      assert.equal(response.status, 200);
      assert.equal(response.headers.get("content-disposition"), 'attachment; filename="part.gcode"');
      assert.match(response.headers.get("content-type"), /^application\/octet-stream/);
      assert.equal(await response.text(), "G1 X0 Y0\n");
    });
  });

  it("passes 204 deletions through without a body", async () => {
    await withConsole({}, async ({ base }) => {
      const response = await fetch(`${base}/api/jobs/job-1`, { method: "DELETE" });
      assert.equal(response.status, 204);
      assert.equal(await response.text(), "");
    });
  });

  it("reports an unreachable REST server as 502", async () => {
    const deadUpstream = await listen(http.createServer());
    const deadOrigin = originOf(deadUpstream);
    await closeServer(deadUpstream);

    const consoleServer = await startConsole({ restUrl: deadOrigin, host: "127.0.0.1", port: 0 });
    try {
      const response = await fetch(`${originOf(consoleServer)}/api/health`);
      assert.equal(response.status, 502);
      const payload = await response.json();
      assert.equal(payload.code, 502);
      assert.equal(payload.message, "REST server unreachable");
    } finally {
      await closeServer(consoleServer);
    }
  });

  it("rejects methods outside the proxy allowlist", async () => {
    await withConsole({}, async ({ base, upstream }) => {
      const response = await fetch(`${base}/api/echo`, { method: "PATCH", body: "x" });
      assert.equal(response.status, 405);
      assert.equal(upstream.calls.length, 0);
    });
  });

  it("exposes a health probe for CI and containers", async () => {
    await withConsole({}, async ({ base }) => {
      const response = await fetch(`${base}/healthz`);
      assert.equal(response.status, 200);
      assert.equal(await response.text(), "ok\n");
    });
  });
});

// ---------------------------- CLI surface ----------------------------

describe("command line", () => {
  it("uses the documented defaults", () => {
    const options = parseCliArgs([], {});
    assert.equal(options.restUrl, DEFAULTS.restUrl);
    assert.equal(options.host, "127.0.0.1");
    assert.equal(options.port, 5080);
    assert.equal(options.token, "");
    assert.equal(options.help, false);
  });

  it("lets flags win over environment fallbacks", () => {
    const options = parseCliArgs(
      ["--rest-url", "http://10.0.0.5:9000/hsba", "--host", "0.0.0.0", "--port", "0", "--token", "s3cret"],
      { HSBA_REST_URL: "http://ignored:1", HSBA_WEBUI_HOST: "::1", HSBA_WEBUI_PORT: "1", HSBA_REST_AUTH_TOKEN: "env" },
    );
    assert.equal(options.restUrl, "http://10.0.0.5:9000/hsba");
    assert.equal(options.host, "0.0.0.0");
    assert.equal(options.port, 0);
    assert.equal(options.token, "s3cret");
  });

  it("falls back to the environment", () => {
    const options = parseCliArgs([], {
      HSBA_REST_URL: "https://slicer.internal:8443",
      HSBA_WEBUI_PORT: "8090",
      HSBA_REST_AUTH_TOKEN: "env-token",
    });
    assert.equal(options.restUrl, "https://slicer.internal:8443");
    assert.equal(options.port, 8090);
    assert.equal(options.token, "env-token");
  });

  it("supports --help and rejects invalid input", () => {
    assert.equal(parseCliArgs(["--help"], {}).help, true);
    assert.equal(parseCliArgs(["-h"], {}).help, true);
    assert.throws(() => parseCliArgs(["--bogus"], {}), UsageError);
    assert.throws(() => parseCliArgs(["--port"], {}), UsageError);
    assert.throws(() => parseCliArgs(["--port", "70000"], {}), UsageError);
    assert.throws(() => parseCliArgs(["--port", "abc"], {}), UsageError);
    assert.throws(() => parseCliArgs(["--rest-url", "ftp://host"], {}), UsageError);
    assert.throws(() => parseCliArgs(["--rest-url", "not a url"], {}), UsageError);
  });

  it("reports the browsable URL of a listening console", async () => {
    const server = createConsoleServer({ restUrl: "http://127.0.0.1:1" });
    await listen(server);
    try {
      const url = consoleUrl(server);
      assert.match(url, /^http:\/\/127\.0\.0\.1:\d+\/$/);
    } finally {
      await closeServer(server);
    }
  });
});

// ---------------------------- http helpers ----------------------------

/**
 * Evaluate the *served* helpers in a bare sandbox and hand back their global object.
 *
 * Fetching instead of importing keeps the browser bundle as the single source of truth, and the
 * plain-script evaluation mirrors what the page does. TextEncoder is not part of ECMAScript, so a
 * fresh context does not have it and it has to be injected.
 */
async function loadHelpers(base) {
  const source = await (await fetch(`${base}/httphelpers.js`)).text();
  const sandbox = { TextEncoder };
  vm.createContext(sandbox);
  vm.runInContext(source, sandbox, { filename: "httphelpers.js" });
  assert.ok(sandbox.HsBaHttp, "httphelpers.js did not publish globalThis.HsBaHttp");
  return sandbox.HsBaHttp;
}

/**
 * @brief Bring a value the helpers built inside their own realm back here, ready for a strict compare.
 *
 * `node:assert/strict` deep-equality also checks the prototype, and an object made in a `node:vm`
 * context carries that context's `Object` or `Array`, so it is never reference-equal to a literal
 * written in this file even when every field matches. Serializing once is how the two are compared.
 */
function local(value) {
  return JSON.parse(JSON.stringify(value));
}

describe("result download names", () => {
  it("reads the file name out of every Content-Disposition shape", async () => {
    await withConsole({}, async ({ base }) => {
      const http_ = await loadHelpers(base);
      const cases = [
        ['attachment; filename="part.gcode"', "part.gcode"],
        ["attachment; filename=part.gcode", "part.gcode"],
        // A server that leaked its directory must still yield a bare name, on either platform.
        ['attachment; filename="/var/tmp/out/part.gcode"', "part.gcode"],
        ['attachment; filename="C:\\out\\part.gcode"', "part.gcode"],
        // RFC 5987 carries a UTF-8 name and wins over the ASCII fallback beside it.
        ['attachment; filename="fallback.gcode"; filename*=UTF-8\'\'%E6%89%93%E5%8D%B0.gcode', "打印.gcode"],
        ["", ""],
        ['attachment; filename=""', ""],
        // A broken percent-escape must not throw, only lose the extended name.
        ["attachment; filename*=UTF-8''%ZZ.gcode", ""],
      ];
      for (const [header, expected] of cases) {
        assert.equal(http_.filenameFromDisposition(header, ""), expected, header || "(empty header)");
      }
    });
  });

  it("names a result after the job id plus the extension that pipeline really writes", async () => {
    await withConsole({}, async ({ base }) => {
      const http_ = await loadHelpers(base);
      // A result can reach the browser without a Content-Disposition (inline answers, or a server
      // that refused to name an unusable path), so every output-producing kind needs its own
      // suffix: saving a package as "job-7" leaves the user with a file nothing opens.
      const suffixes = {
        fdm: "gcode",
        sla: "zip",
        sls: "zip",
        slm: "zip",
        lom: "zip",
        tdp: "zip",
        waam: "txt",
        custom: "txt",
      };
      for (const [kind, extension] of Object.entries(suffixes)) {
        assert.equal(http_.resultFilename("", "job-7", kind), `job-7.${extension}`, kind);
      }
      // filetransfer only moves other people's files, so nothing is invented for it.
      assert.equal(http_.resultFilename("", "job-7", "filetransfer"), "job-7");
      assert.equal(http_.resultFilename("", "job-7", undefined), "job-7");
      // A name from the server always wins over the fallback.
      assert.equal(http_.resultFilename('attachment; filename="part.gcode"', "job-7", "fdm"), "part.gcode");
    });
  });

  it("keeps the proxied download name intact end to end", async () => {
    await withConsole({}, async ({ base }) => {
      const http_ = await loadHelpers(base);
      // The fake REST server answers like routes.cpp: an octet-stream body plus a bare filename.
      const response = await fetch(`${base}/api/jobs/job-1/result`);
      assert.equal(response.status, 200);
      const name = http_.resultFilename(response.headers.get("Content-Disposition"), "job-1", "fdm");
      assert.equal(name, "part.gcode");
      assert.equal(name.split(/[\\/]/).length, 1);
    });
  });
});

describe("result preview", () => {
  it("previews text and json but never a binary download", async () => {
    await withConsole({}, async ({ base }) => {
      const http_ = await loadHelpers(base);
      const previewable = [
        "text/plain; charset=utf-8",
        "text/plain",
        "application/json",
        "application/xml",
        "application/geo+json",
        "TEXT/HTML",
        "application/svg+xml",
      ];
      for (const type of previewable) {
        assert.ok(http_.isPreviewable(type), type);
      }
      for (const type of ["", undefined, "application/octet-stream", "application/x-protobuf", "image/png"]) {
        assert.equal(http_.isPreviewable(type), false, String(type));
      }
    });
  });

  it("reports the whole file when it fits the panel", async () => {
    await withConsole({}, async ({ base }) => {
      const http_ = await loadHelpers(base);
      const body = "G21\nG90\nG1 X1.0 Y2.0 E3.0 F1200\n";
      const view = http_.previewText(body);
      assert.equal(view.text, body);
      assert.equal(view.truncated, false);
      assert.equal(view.bytes, Buffer.byteLength(body, "utf8"));
      assert.equal(view.shownBytes, view.bytes);
      assert.equal(view.lines, 4);
      assert.equal(http_.previewText("").lines, 0);
    });
  });

  it("cuts a large result at the byte budget without splitting a character", async () => {
    await withConsole({}, async ({ base }) => {
      const http_ = await loadHelpers(base);
      const gcode = "G1 X1.0 Y2.0\n".repeat(500);
      const view = http_.previewText(gcode, 256);
      assert.equal(view.truncated, true);
      assert.equal(view.bytes, Buffer.byteLength(gcode, "utf8"));
      assert.ok(view.shownBytes <= 256, `shown ${view.shownBytes} over budget`);
      assert.ok(gcode.startsWith(view.text));
      assert.ok(view.lines >= 2, "a line-oriented file must still show several lines");

      // Multi-byte text reaching the cut point must not leave a lone high surrogate behind.
      const emoji = "a" + "🙂".repeat(10);
      const tiny = http_.previewText(emoji, 4);
      assert.equal(tiny.text, "a");
      assert.equal(tiny.shownBytes, 1);
      assert.equal(tiny.bytes, 41);
      assert.equal(tiny.truncated, true);
    });
  });

  it("applies the documented default budget", async () => {
    await withConsole({}, async ({ base }) => {
      const http_ = await loadHelpers(base);
      const big = "x".repeat(http_.PREVIEW_LIMIT_BYTES + 1);
      assert.equal(http_.previewText(big).shownBytes, http_.PREVIEW_LIMIT_BYTES);
      assert.equal(http_.previewText(big).truncated, true);
      assert.equal(http_.previewText("x".repeat(http_.PREVIEW_LIMIT_BYTES)).truncated, false);
    });
  });
});

// ---------------------------- layer geometry ----------------------------

describe("layer geometry helpers", () => {
  it("mirrors ring coordinates about the mid-height the viewBox keeps", async () => {
    await withConsole({}, async ({ base }) => {
      const http_ = await loadHelpers(base);
      assert.equal(http_.mirrorLine([0, 0, 20, 10]), 5);
      assert.equal(http_.mirrorLine([0, 2, 20, 8]), 5);
      assert.equal(http_.mirrorLine([]), 0);
      assert.equal(http_.mirrorLine(undefined), 0);

      // A square that fills its own bounds maps onto itself in reverse winding; the open ring below
      // is the one that shows the flip, because its y values are not symmetric.
      assert.equal(
        http_.ringPath({ point_count: 4, closed: 1, xy: [0, 0, 20, 0, 20, 10, 0, 10] }, 5),
        "M 0 10 L 20 10 L 20 0 L 0 0 Z",
      );
      assert.equal(http_.ringPath({ point_count: 2, closed: 0, xy: [1, 1, 3, 5] }, 0), "M 1 -1 L 3 -5");
      // The documented format: one space after each instruction, an open ring keeps no closing one.
      assert.doesNotMatch(http_.ringPath({ point_count: 2, closed: 0, xy: [1, 1, 3, 5] }, 0), /Z$/);
    });
  });

  it("drops degenerate and inconsistent rings instead of drawing half a vertex", async () => {
    await withConsole({}, async ({ base }) => {
      const http_ = await loadHelpers(base);
      assert.equal(http_.ringPath({ point_count: 0, closed: 1, xy: [0, 0, 1, 1] }, 0), "");
      assert.equal(http_.ringPath({ point_count: 2, closed: 1, xy: [0, 0] }, 0), "");
      assert.equal(http_.ringPath(undefined, 0), "");
      // point_count may claim more than the flat array holds: read only what is really there.
      assert.equal(http_.ringPath({ point_count: 3, closed: 1, xy: [0, 0, 1, 1, 2] }, 0), "M 0 0 L 1 -1 Z");
      // Sub-micron noise must not leak a "-0" into the path data.
      assert.equal(http_.ringPath({ point_count: 2, closed: 0, xy: [0.123456, -0.0004, 1, 1] }, 0), "M 0.123 0 L 1 -1");
      assert.ok(!http_.ringPath({ point_count: 2, closed: 0, xy: [0, -0.0001, 1, 1] }, 0).includes("-0 "));
      assert.equal(http_.roundCoord("nonsense"), 0);
      assert.ok(Object.is(http_.roundCoord(-0.0004), 0), "-0 survived the rounding");
    });
  });

  it("fits a layer and then zooms and pans it in millimetres", async () => {
    await withConsole({}, async ({ base }) => {
      const http_ = await loadHelpers(base);
      const view = http_.geometryView([0, 0, 20, 10]);
      assert.equal(view.cx, 10);
      assert.equal(view.cy, 5);
      // 2% of the longer side, on every edge.
      assert.equal(view.w, 20.8);
      assert.equal(view.h, 10.8);
      assert.equal(view.x, -0.4);
      assert.equal(view.y, -0.4);

      // A layer that is one point would give a zero-size box, which nothing can render.
      const point = http_.geometryView([5, 5, 5, 5]);
      assert.equal(point.w, 1);
      assert.equal(point.h, 1);
      assert.equal(point.cx, 5);
      assert.equal(http_.geometryView(undefined).w, 1);
      assert.equal(http_.currentViewBox(undefined, 1, { x: 0, y: 0 }), http_.GEOMETRY_EMPTY_VIEW);

      assert.equal(http_.currentViewBox(view, 1, { x: 0, y: 0 }), "-0.4 -0.4 20.8 10.8");
      assert.equal(http_.currentViewBox(view, 2, { x: 1, y: 0 }), "5.8 2.3 10.4 5.4");
      assert.equal(http_.currentViewBox(view, 1, undefined), "-0.4 -0.4 20.8 10.8");

      assert.equal(http_.clampZoom(1000), http_.GEOMETRY_MAX_ZOOM);
      assert.equal(http_.clampZoom(0), http_.GEOMETRY_MIN_ZOOM);
      assert.equal(http_.clampZoom(Number.NaN), 1);
      assert.equal(http_.clampZoom(2), 2);
    });
  });

  it("spends the path budget on contours first and says how much it left out", async () => {
    await withConsole({}, async ({ base }) => {
      const http_ = await loadHelpers(base);
      const ring = { point_count: 2, closed: 0, xy: [0, 0, 1, 1] };
      const crowded = [];
      for (let at = 0; at < http_.GEOMETRY_MAX_PATHS + 5; at += 1) {
        crowded.push(ring);
      }
      const drawn = http_.layerPaths({ outlines: [ring], fills: crowded, supports: [ring] }, 0);
      assert.equal(drawn.outlines.length, 1, "a contour was squeezed out by the infill");
      assert.equal(drawn.supports.length, 1);
      assert.equal(drawn.fills.length, http_.GEOMETRY_MAX_PATHS - 2);
      // counts describe what the layer really holds, so the panel can name what is missing.
      assert.equal(drawn.counts.fills, http_.GEOMETRY_MAX_PATHS + 5);
      assert.equal(drawn.counts.dropped, 7);

      const empty = http_.layerPaths({ outlines: [{ point_count: 1, closed: 1, xy: [0, 0] }] }, 0);
      assert.equal(empty.counts.outlines, 0);
      assert.equal(empty.fills.length, 0);
    });
  });

  it("projects a model point the way the two documented camera poses say it should land", async () => {
    await withConsole({}, async ({ base }) => {
      const http_ = await loadHelpers(base);
      const readPoint = (point) => [point.u, point.v];
      // pitch pi/2 looks straight down: the plane is the build plate and model y runs downwards.
      assert.deepEqual(readPoint(http_.projectPoint(3, 5, 7, 0, Math.PI / 2)), [3, -5]);
      // pitch 0 looks along y: the plane is the XZ elevation, so height becomes the vertical axis.
      assert.deepEqual(readPoint(http_.projectPoint(3, 5, 7, 0, 0)), [3, 7]);
      // yaw pi/2 swaps the model axes before either of those happens.
      assert.deepEqual(readPoint(http_.projectPoint(3, 5, 7, Math.PI / 2, Math.PI / 2)), [5, 3]);
      assert.deepEqual(readPoint(http_.projectPoint(3, 5, 7, Math.PI / 2, 0)), [5, 7]);
      // Junk in, zero out: a half-parsed body must not paint NaN coordinates.
      assert.deepEqual(readPoint(http_.projectPoint("a", "b", undefined, Number.NaN, Number.NaN)), [0, 0]);
    });
  });

  it("frames projected points in the same padded box the layer view fits", async () => {
    await withConsole({}, async ({ base }) => {
      const http_ = await loadHelpers(base);
      const box = http_.projectedViewBox([
        { u: 0, v: 0 },
        { u: 20, v: 10 },
        { u: 10, v: 5 },
      ]);
      // SVG grows downwards, so a v of 0..10 is framed as the y range -10..0.
      assert.equal(box.d, "-0.4 -10.4 20.8 10.8");
      assert.equal(box.cx, 10);
      assert.equal(box.cy, -5);

      // A single point has no extent, and a zero-size viewBox renders nowhere at all.
      const point = http_.projectedViewBox([{ u: 5, v: 7 }]);
      assert.equal(point.w, 1);
      assert.equal(point.h, 1);
      assert.equal(point.cx, 5);
      assert.equal(point.cy, -7);
      assert.equal(http_.projectedViewBox([]).d, http_.GEOMETRY_EMPTY_VIEW);
      assert.equal(http_.projectedViewBox(undefined).d, http_.GEOMETRY_EMPTY_VIEW);
      // A vertex the server sent as junk is skipped rather than dragging the box to NaN.
      assert.equal(http_.projectedViewBox([{ u: 0, v: 0 }, { u: Number.NaN, v: 1 }]).d, "-0.5 -0.5 1 1");
    });
  });

  it("projects every ring of a layer and reports what the budget left out", async () => {
    await withConsole({}, async ({ base }) => {
      const http_ = await loadHelpers(base);
      const square = { point_count: 4, closed: 1, xy: [0, 0, 20, 0, 20, 10, 0, 10] };
      const angles = http_.GEOMETRY_STACK_ANGLES;
      const drawn = http_.projectedRingPaths([square], 0.3, angles, 100);
      assert.equal(drawn.paths.length, 1);
      assert.equal(drawn.used, 1);
      assert.equal(drawn.dropped, 0);
      assert.equal(drawn.points[0].length, 4);
      // The documented format: one M, explicit L for every vertex, and Z only for a closed ring.
      assert.equal((drawn.paths[0].match(/M/g) || []).length, 1);
      assert.doesNotMatch(drawn.paths[0], /[HVCSQTA]/);
      assert.match(drawn.paths[0], / Z$/);

      // Height is what separates the layers of a stack: the same ring at another z must move.
      const higher = http_.projectedRingPaths([square], 0.6, angles, 100);
      assert.notEqual(higher.paths[0], drawn.paths[0]);

      const open = { point_count: 2, closed: 0, xy: [2, 2, 18, 2] };
      const mixed = http_.projectedRingPaths([square, open], 0.3, angles, 100);
      assert.equal(mixed.paths.length, 2);
      assert.doesNotMatch(mixed.paths[1], /Z$/);

      // Over budget the extras are counted and not drawn, so the panel can name what is missing.
      const tight = http_.projectedRingPaths([square, open], 0.3, angles, 1);
      assert.equal(tight.paths.length, 1);
      assert.equal(tight.used, 2);
      assert.equal(tight.dropped, 1);
      // Two vertices is the smallest thing that can be drawn; anything less is not a ring.
      const nothing = http_.projectedRingPaths([{ point_count: 1, closed: 1, xy: [0, 0] }], 0, angles, 10);
      assert.equal(nothing.paths.length, 0);
      assert.equal(nothing.points.length, 0);
      assert.equal(nothing.used, 0);
      assert.equal(nothing.dropped, 0);
    });
  });

  it("draws a continuous toolpath as one polyline and drops an incomplete tail", async () => {
    await withConsole({}, async ({ base }) => {
      const http_ = await loadHelpers(base);
      const angles = http_.GEOMETRY_STACK_ANGLES;
      const drawn = http_.projectedPathPolyline(GEOMETRY_PATH_BODY.xyz, angles);
      assert.equal(drawn.points.length, GEOMETRY_PATH_BODY.point_count);
      assert.equal((drawn.d.match(/M/g) || []).length, 1, "a toolpath must never be split into subpaths");
      assert.doesNotMatch(drawn.d, /Z$/, "an uninterrupted line has no closing segment to invent");
      assert.ok(!drawn.d.includes("NaN") && !drawn.d.includes("-0 "), drawn.d);

      // One or two trailing numbers cannot form a vertex: read them as noise, not as half a point.
      assert.equal(http_.projectedPathPolyline([0, 0, 0.3, 1, 1, 0.6, 2], angles).points.length, 2);
      assert.equal(http_.projectedPathPolyline([], angles).d, "");
      assert.equal(http_.projectedPathPolyline(undefined, angles).d, "");
    });
  });

  it("shares the node budget across the layers a stack actually samples", async () => {
    await withConsole({}, async ({ base }) => {
      const http_ = await loadHelpers(base);
      assert.equal(http_.stackBudget(12), 250);
      // An empty sample still gets the whole budget rather than dividing it by zero.
      assert.equal(http_.stackBudget(0), http_.GEOMETRY_MAX_PATHS);
      assert.equal(http_.stackBudget(1), http_.GEOMETRY_MAX_PATHS);
      // A taller stack than the budget could ever serve still gets at least one path per layer.
      assert.equal(http_.stackBudget(http_.GEOMETRY_MAX_PATHS * 2), 1);
    });
  });

  it("samples the first and the last layer whatever the job is tall", async () => {
    await withConsole({}, async ({ base }) => {
      const http_ = await loadHelpers(base);
      const short = http_.sampleLayers(5, http_.GEOMETRY_STACK_LAYERS);
      assert.deepEqual(Array.from(short.indices), [0, 1, 2, 3, 4]);
      assert.equal(short.focus, 2);
      assert.equal(http_.sampleLayers(0, 12).indices.length, 0);

      const tall = http_.sampleLayers(214, http_.GEOMETRY_STACK_LAYERS);
      assert.equal(tall.indices.length, http_.GEOMETRY_STACK_LAYERS);
      assert.equal(tall.indices[0], 0, "a stack that hides the bottom layer misrepresents the part");
      assert.equal(tall.indices[tall.indices.length - 1], 213, "the top layer was hidden");
      for (let at = 1; at < tall.indices.length; at += 1) {
        assert.ok(tall.indices[at] > tall.indices[at - 1], JSON.stringify(tall.indices));
      }
      assert.equal(tall.focus, tall.indices[Math.floor((tall.indices.length - 1) / 2)]);
    });
  });

  it("walks the colour ramp from cool to warm without leaving six hex digits", async () => {
    await withConsole({}, async ({ base }) => {
      const http_ = await loadHelpers(base);
      for (const at of [0, 1, 5, 6]) {
        assert.match(http_.sequenceColor(at, 6), /^#[0-9a-f]{6}$/);
      }
      // The ramp is the only thing that tells the first wall from the last one, so it has to differ.
      assert.notEqual(http_.sequenceColor(0, 6), http_.sequenceColor(5, 6));
      const colours = [];
      for (let at = 0; at < 6; at += 1) {
        colours.push(http_.sequenceColor(at, 6));
      }
      assert.equal(new Set(colours).size, 6, colours.join(","));
      // A single element has no position in a ramp to be coloured by: it gets the middle of it.
      assert.equal(http_.sequenceColor(0, 1), http_.sequenceColor(0, 1));
      assert.equal(http_.sequenceColor(0, 0), http_.sequenceColor(0, 1));
    });
  });

  it("marks the heading of a polyline on its longest segment", async () => {
    await withConsole({}, async ({ base }) => {
      const http_ = await loadHelpers(base);
      const view = { w: 20, h: 10 };
      const straight = [
        { u: 0, v: 0 },
        { u: 10, v: 0 },
        { u: 10, v: 1 },
      ];
      const marks = http_.arrowMark(straight, view);
      assert.equal(marks.length, 1);
      // A closed three-vertex triangle, centred on the longest segment of the polyline.
      assert.match(marks[0], /^M .+ L .+ L .+ Z$/);
      assert.match(marks[0], /^M 5\.12 /, "the mark belongs to the long segment, not the short one");
      assert.equal(http_.arrowMark([], view).length, 0);
      assert.equal(http_.arrowMark([{ u: 1, v: 1 }], view).length, 0);
      // Coincident vertices have no direction to report.
      assert.equal(http_.arrowMark([{ u: 1, v: 1 }, { u: 1, v: 1 }], view).length, 0);
      // Without a known box there is no sensible size for the mark either.
      assert.equal(http_.arrowMark(straight, { w: 0, h: 0 }).length, 0);
      assert.equal(http_.arrowMark(straight, undefined).length, 0);
    });
  });
});

// ---------------------------- package entries and preview images ----------------------------

describe("package entries and embedded previews", () => {
  it("reads the preview blocks a G-code header carries", async () => {
    await withConsole({}, async ({ base }) => {
      const http_ = await loadHelpers(base);
      const blocks = http_.parseGcodeThumbnails(INLINE_GCODE);
      assert.equal(blocks.length, 2);
      assert.deepEqual(local(blocks[0]), {
        name: "model_top",
        width: 8,
        height: 8,
        format: "png",
        data: "aGVsbG8=",
        skipped: false,
        reason: "",
      });
      // A skip notice is a statement about a missing picture, so it has to survive as one.
      assert.equal(blocks[1].skipped, true);
      assert.equal(blocks[1].name, "model_side");
      assert.equal(blocks[1].reason, "mesh_too_large");
      assert.equal(blocks[1].data, "");

      // A chunked image is rejoined without the comment prefixes or the line breaks.
      const wrapped = [
        "; thumbnail begin name=long width=2 height=2 format=png b64len=12",
        "; aGVsbG8",
        "; wK3Rtd",
        "; thumbnail end name=long",
      ].join("\n");
      assert.equal(http_.parseGcodeThumbnails(wrapped)[0].data, "aGVsbG8wK3Rtd");

      // Nothing below the first command line belongs to the header, so the scan stops there: a run
      // whose blocks are cut off by the preview budget must not be guessed at.
      const below = ["G21", "; thumbnail begin name=late width=2 height=2 format=png b64len=2", "; aG"].join("\n");
      assert.deepEqual(local(http_.parseGcodeThumbnails(below)), []);
      const unclosed = ["; thumbnail begin name=cut width=2 height=2 format=png b64len=2", "; aG", "G21"].join("\n");
      assert.deepEqual(local(http_.parseGcodeThumbnails(unclosed)), []);

      // A file the engine never wrote previews for stays an empty list rather than an error.
      for (const empty of ["", undefined, null, "G21\nG90\n"]) {
        assert.deepEqual(local(http_.parseGcodeThumbnails(empty)), []);
      }

      // However many blocks a header carries, the reader stops at the documented bound.
      const many = [];
      for (let at = 0; at < http_.GCODE_THUMBNAIL_MAX_BLOCKS + 3; at += 1) {
        many.push(
          `; thumbnail begin name=m${at} width=1 height=1 format=png b64len=2`,
          "; aG",
          `; thumbnail end name=m${at}`,
        );
      }
      assert.equal(http_.parseGcodeThumbnails(many.join("\n")).length, http_.GCODE_THUMBNAIL_MAX_BLOCKS);

      // And one oversized block is bounded too, so a hostile file cannot grow the page without end.
      const chunk = "; " + "A".repeat(72);
      const lines = ["; thumbnail begin name=big width=1 height=1 format=png b64len=999999999"];
      for (let at = 0; at < 60000; at += 1) {
        lines.push(chunk);
      }
      lines.push("; thumbnail end name=big");
      const huge = http_.parseGcodeThumbnails(lines.join("\n"));
      assert.equal(huge.length, 1);
      assert.ok(
        huge[0].data.length <= http_.GCODE_THUMBNAIL_MAX_B64 + 72,
        `collected ${huge[0].data.length} characters over a ${http_.GCODE_THUMBNAIL_MAX_B64} budget`,
      );
    });
  });

  it("hands a preview to the browser as a data URI, or as nothing at all", async () => {
    await withConsole({}, async ({ base }) => {
      const http_ = await loadHelpers(base);
      const block = { name: "model_top", width: 8, height: 8, format: "png", data: "aGVsbG8=", skipped: false, reason: "" };
      assert.equal(http_.thumbnailDataUrl(block), "data:image/png;base64,aGVsbG8=");
      assert.equal(http_.thumbnailDataUrl({ ...block, format: "jpeg" }), "data:image/jpeg;base64,aGVsbG8=");
      assert.equal(http_.thumbnailDataUrl({ ...block, format: "svg" }), "data:image/svg+xml;base64,aGVsbG8=");
      // An unknown format is not a picture, and a skip notice has no bytes to show.
      assert.equal(http_.thumbnailDataUrl({ ...block, format: "tiff" }), "");
      assert.equal(http_.thumbnailDataUrl({ ...block, format: "" }), "");
      assert.equal(http_.thumbnailDataUrl({ ...block, skipped: true }), "");
      assert.equal(http_.thumbnailDataUrl({ ...block, data: "" }), "");
      assert.equal(http_.thumbnailDataUrl(undefined), "");
    });
  });

  it("addresses one entry of a result package without walking the route", async () => {
    await withConsole({}, async ({ base }) => {
      const http_ = await loadHelpers(base);
      // A folder inside an archive is the reason the route captures a greedy tail; each component still
      // has to be encoded on its own, or a name with a slash would address a different entry.
      assert.equal(http_.packageEntryUrl("job-1", "layers/layer_0.png"), "/api/jobs/job-1/package/layers/layer_0.png");
      assert.equal(http_.packageEntryUrl("a/b", "c d.png"), "/api/jobs/a%2Fb/package/c%20d.png");
      assert.equal(http_.packageEntryUrl("job-1", "..\\config.json"), "/api/jobs/job-1/package/..%5Cconfig.json");
      assert.equal(http_.packageEntryUrl("job-1", ""), "/api/jobs/job-1/package/");
    });
  });

  it("proxies a package image with the type and protection the server chose", async () => {
    await withConsole({}, async ({ base }) => {
      const http_ = await loadHelpers(base);
      // The URL the page writes into <img src> is the one a browser then requests, so the console has
      // to answer it byte for byte - and keep the header that stops a mislabeled entry being sniffed.
      const response = await fetch(base + http_.packageEntryUrl("job-1", "layers/layer_0.png"));
      assert.equal(response.status, 200);
      assert.equal(response.headers.get("Content-Type"), "image/png");
      assert.equal(response.headers.get("X-Content-Type-Options"), "nosniff");
      assert.deepEqual(Buffer.from(await response.arrayBuffer()), PACKAGE_PNG);

      // A single-file result keeps the server's own sentence, which is what the strip quotes back.
      const missing = await fetch(`${base}/api/jobs/job-2/package`);
      assert.equal(missing.status, 404);
      assert.match((await missing.json()).message, /result is not a package/);
    });
  });

  it("orders the images of a package by the layer number in their names", async () => {
    await withConsole({}, async ({ base }) => {
      const http_ = await loadHelpers(base);
      const entries = [
        { path: "layers/layer_10.png", media_type: "image/png" },
        { path: "config.json", media_type: "application/json" },
        { path: "layers/layer_2.png", media_type: "image/png" },
        { path: "preview.svg", media_type: "image/svg+xml" },
        { path: "supports/layer_1.jpg", media_type: "image/jpeg" },
        { path: "model.gcode", media_type: "text/plain" },
        { path: "floor.bin" },
      ];
      assert.deepEqual(http_.packageImages(entries).map((entry) => entry.path), [
        "supports/layer_1.jpg",
        "layers/layer_2.png",
        "layers/layer_10.png",
        "preview.svg",
      ]);
      // An entry the listing never typed is not an image either.
      assert.deepEqual(local(http_.packageImages([{ path: "a.png" }])), []);
      for (const nothing of [[], undefined, null]) {
        assert.deepEqual(local(http_.packageImages(nothing)), []);
      }
      // The media types the console will draw are the ones the server will hand out.
      assert.deepEqual(Object.keys(http_.PACKAGE_IMAGE_MEDIA_TYPES).sort(), ["image/jpeg", "image/png", "image/svg+xml"]);
    });
  });

  it("patches submission flags into a JSON config and leaves any other format alone", async () => {
    await withConsole({}, async ({ base }) => {
      const http_ = await loadHelpers(base);
      const patched = http_.applyConfigFlags('{"layer_height":0.2,"output_path":""}', { gcode_thumbnails: 1 });
      assert.equal(JSON.parse(patched).gcode_thumbnails, 1);
      assert.equal(JSON.parse(patched).layer_height, 0.2);
      assert.match(patched, /\n {2}"gcode_thumbnails": 1/, "the patch lost the editor's formatting");

      // A config the server answered as TOML or INI is still a config the console must submit verbatim.
      for (const text of ["layer_height = 0.2\n", "[fdm]\nlayer_height=0.2\n", "", "  ", "[1,2]", "5", "null"]) {
        assert.equal(http_.applyConfigFlags(text, { gcode_thumbnails: 1 }), text, text);
      }
      assert.equal(http_.applyConfigFlags(undefined, { gcode_thumbnails: 1 }), "");
      // An empty patch still normalizes JSON, which is what makes the caller's "unchanged" test mean
      // "this is not a JSON config" rather than "nothing happened".
      assert.equal(http_.applyConfigFlags('{"a":1}', {}), JSON.stringify({ a: 1 }, null, 2));
    });
  });
});

// ---------------------------- page contract ----------------------------

/** Fetch the served bundle once so several assertions can share one round trip. */
async function pageBundle(base) {
  const html = await (await fetch(`${base}/`)).text();
  const script = await (await fetch(`${base}/app.js`)).text();
  return { html, script };
}

/** Count the <th> cells of the <thead> that belongs to the given tbody id. */
function headerCount(html, bodyId) {
  const bodyAt = html.indexOf(`id="${bodyId}"`);
  assert.ok(bodyAt > 0, `missing tbody ${bodyId}`);
  const head = html.slice(0, bodyAt);
  const thead = head.slice(head.lastIndexOf("<thead>"));
  return (thead.match(/<th>/g) || []).length;
}

/** Read a `const NAME = <n>;` column constant out of app.js. */
function columnConstant(script, name) {
  const match = new RegExp(`const ${name} = (\\d+);`).exec(script);
  assert.ok(match, `missing ${name}`);
  return Number(match[1]);
}

describe("page contract", () => {
  it("shows the job result path in its own Output column", async () => {
    await withConsole({}, async ({ base }) => {
      const { html, script } = await pageBundle(base);
      assert.match(html, /<th>Output<\/th>/);
      assert.equal(headerCount(html, "jobs-body"), columnConstant(script, "JOB_COLUMNS"));
      assert.equal(columnConstant(script, "JOB_COLUMNS"), 8);
      // output_path / error_message are the JobStatusView fields the REST API reports.
      assert.match(script, /job\.output_path/);
      assert.match(script, /job\.error_message/);
      assert.match(script, /navigator\.clipboard\.writeText/);
    });
  });

  it("keeps the uploads table aligned with its column constant", async () => {
    await withConsole({}, async ({ base }) => {
      const { html, script } = await pageBundle(base);
      assert.equal(headerCount(html, "uploads-body"), columnConstant(script, "UPLOAD_COLUMNS"));
      // The stored path is a column of its own, not tooltip-only text on the file name.
      assert.match(html, /<th>Stored path<\/th>/);
      assert.match(script, /pathCell\(row, 3, upload\.path/);
    });
  });

  it("previews a finished result in a panel beside the table", async () => {
    await withConsole({}, async ({ base }) => {
      const { html, script } = await pageBundle(base);
      const helpers = await (await fetch(`${base}/httphelpers.js`)).text();

      // The panel and its controls exist, and the result row offers preview next to download.
      assert.match(html, /<pre id="preview" class="preview" hidden>/);
      assert.match(html, /id="preview-meta" class="muted"/);
      assert.match(html, /<button id="preview-clear"/);
      assert.match(script, /label: "Preview"[\s\S]{0,120}previewResult\(row\.job\.id, row\.job\.kind\)/);
      assert.match(script, /label: "Result"[\s\S]{0,120}downloadResult\(row\.job\.id, row\.job\.kind\)/);
      // The handlers go through the row's current job, because a row survives a refresh.
      assert.match(script, /row\.job = job;/);

      // Both actions share one fetch, whose name and type come from the tested helpers.
      assert.match(script, /HsBaHttp\.resultFilename\(/);
      assert.match(script, /HsBaHttp\.isPreviewable\(/);
      assert.match(script, /HsBaHttp\.previewText\(/);
      assert.match(helpers, /RESULT_EXTENSIONS: RESULT_EXTENSIONS/);

      // A preview is a button, not a column: the table shape must not have drifted.
      assert.equal(headerCount(html, "jobs-body"), columnConstant(script, "JOB_COLUMNS"));
      assert.equal(columnConstant(script, "JOB_COLUMNS"), 8);
      const css = await (await fetch(`${base}/app.css`)).text();
      assert.match(css, /\.preview \{/);
      // The reported path must be shown in full: a clipped cell would hide it behind an
      // ellipsis, which is exactly what the "path visible in the console" requirement rules out.
      const pathRule = css.match(/td\.path \{[^}]*\}/);
      assert.ok(pathRule, "the stylesheet no longer styles the path column");
      assert.match(pathRule[0], /white-space: normal/);
      assert.match(pathRule[0], /overflow-wrap: anywhere/);
      assert.doesNotMatch(pathRule[0], /text-overflow: ellipsis|overflow: hidden/);
    });
  });

  it("offers a layer geometry panel fed by the geometry endpoints", async () => {
    await withConsole({}, async ({ base }) => {
      const { html, script } = await pageBundle(base);
      const css = await (await fetch(`${base}/app.css`)).text();

      // The panel exists, with one slider, one label per category and an SVG node to draw into.
      assert.match(html, /<svg id="geometry-svg"/);
      assert.match(html, /id="geometry-layer" type="range"/);
      assert.match(html, /id="geometry-capture" type="checkbox" checked/);
      for (const toggle of ["geometry-outline", "geometry-fill", "geometry-support"]) {
        assert.match(html, new RegExp(`id="${toggle}" type="checkbox" checked`), toggle);
      }
      assert.match(html, /id="geometry-meta" class="muted"/);

      // Both new job actions go through the row's current job and the server's own availability flag.
      assert.match(script, /label: "Geometry"[\s\S]{0,160}openGeometry\(row\.job\.id/);
      assert.match(script, /label: "Cancel"[\s\S]{0,160}cancelJob\(row\.job\.id/);
      assert.match(script, /job\.geometry_available === 1/);
      // Preview and Result keep their place, so a row that survived a poll still has all five.
      assert.match(script, /label: "Preview"[\s\S]{0,120}previewResult\(row\.job\.id/);

      // Contours are drawn in millimetres, so the stroke has to opt out of the viewBox scale.
      const contourRule = css.match(/\.geometry-outline path[^{]*\{[^}]*\}/);
      assert.ok(contourRule, "the stylesheet no longer styles the geometry contours");
      assert.match(contourRule[0], /vector-effect: non-scaling-stroke/);
      assert.match(contourRule[0], /fill: none/);

      // A preview is still a button, not a column: the table shape must not have drifted.
      assert.equal(headerCount(html, "jobs-body"), columnConstant(script, "JOB_COLUMNS"));
      assert.equal(columnConstant(script, "JOB_COLUMNS"), 8);
    });
  });

  it("offers more than one way to look at the same geometry", async () => {
    await withConsole({}, async ({ base }) => {
      const { html, script } = await pageBundle(base);
      const css = await (await fetch(`${base}/app.css`)).text();

      // Four buttons, one checkbox: the views are driven by the same panel, not by a second card.
      for (const view of ["layer", "stack", "side", "path"]) {
        assert.match(html, new RegExp(`id="geometry-view-${view}" type="button"`), view);
      }
      assert.match(html, /id="geometry-order" type="checkbox"/);

      // Every view is dispatched by name, and the two projected ones share one sampled stack.
      assert.match(script, /case "stack":/);
      assert.match(script, /loadGeometryStack\(/);
      assert.match(script, /loadGeometryPath\(/);
      assert.match(script, /geometryPath\(geometry\.id, "\/geometry\/path"\)/);
      assert.match(script, /HsBaHttp\.GEOMETRY_VIEWS/);
      // The Path button is gated on the server's own flag, not on a guess about the pipeline kind.
      assert.match(script, /has_path === 1/);

      // The category colours are scoped to the unordered drawing, so the sequence can override them.
      assert.match(css, /svg:not\(\.geometry-ordered\) \.geometry-outline path \{/);
      const arrowRule = css.match(/\.geometry-arrow path \{[^}]*\}/);
      assert.ok(arrowRule, "the stylesheet no longer styles the direction marks");
      assert.match(arrowRule[0], /fill: var\(--accent\)/);
      assert.match(css, /\.geometry-layer \{ opacity: 0\.55; \}/);
      assert.match(css, /\.geometry-focus \{ opacity: 1; \}/);

      // Four more views added no column and no table: the shape of the console has not drifted.
      assert.equal(headerCount(html, "jobs-body"), columnConstant(script, "JOB_COLUMNS"));
      assert.equal(columnConstant(script, "JOB_COLUMNS"), 8);
    });
  });

  it("shows the pictures a result carries, from its header or from its package", async () => {
    await withConsole({}, async ({ base }) => {
      const { html, script } = await pageBundle(base);
      const css = await (await fetch(`${base}/app.css`)).text();

      // The submission option exists and is off until the operator asks for it, and it is the only
      // kind that can honour it: nothing but FDM writes the G-code a preview can live inside.
      assert.match(html, /<input id="fdm-thumbnails" type="checkbox">/);
      assert.match(script, /byId\("fdm-thumbnails"\)\.disabled = byId\("kind"\)\.value !== "fdm"/);
      assert.match(script, /async function loadKinds\(\)[\s\S]{0,900}syncThumbnailOption\(\);/);
      // The flag is patched into the config the editor holds, and only for the kind that reads it.
      assert.match(script, /if \(kind === "fdm"\) \{\s*body = HsBaHttp\.applyConfigFlags\(config, \{ gcode_thumbnails:/);

      // Both strips live under the text panel, and each starts out hidden.
      assert.match(html, /<div id="gcode-thumbnails" class="thumbnail-stage" hidden><\/div>/);
      assert.match(html, /<div id="package-stage" class="package-stage" hidden>/);
      assert.match(html, /<button id="package-browse" type="button">Images<\/button>/);
      assert.match(html, /<img id="package-image" alt=/);
      for (const control of ["package-prev", "package-next", "package-meta"]) {
        assert.match(html, new RegExp(`id="${control}"`), control);
      }
      // The caption is the only feedback a package without pictures gives, so it must not sit inside
      // the strip that stays hidden in that case - the message would be written and never seen.
      const stageBlock = html.match(/<div id="package-stage"[\s\S]*?<\/div>\s*<\/div>/);
      assert.ok(stageBlock, "the package strip no longer closes before the caption");
      assert.doesNotMatch(stageBlock[0], /package-meta/, "the caption is hidden together with the strip");

      // Assigning textContent replaces every child node, so an id the script fills in can never be an
      // element the page also builds controls into. No fake element can catch this: the harness hands
      // out each id as its own detached node, so a nested button simply is not there to lose. That is
      // how Images and Clear vanished from the preview heading while every page test stayed green.
      for (const filled of new Set(
        [...script.matchAll(/byId\("([^"]+)"\)\.textContent =/g)].map((match) => match[1]),
      )) {
        const at = html.indexOf(`id="${filled}"`);
        assert.ok(at >= 0, `${filled} is filled in by the script but is not in the page`);
        const open = html.lastIndexOf("<", at);
        const tag = html.slice(open + 1, at).trim().split(/[\s>]/)[0];
        const contentStart = html.indexOf(">", at);
        assert.match(
          html.slice(contentStart + 1),
          new RegExp(`^[^<]*</${tag}>`),
          `<${tag}>#${filled} holds child elements the script would destroy`,
        );
      }
      assert.match(html, /<span id="preview-title">Result preview<\/span>/);

      // One fetch feeds the text and the images, so the header is never downloaded twice.
      assert.match(script, /const body = await blob\.text\(\);[\s\S]{0,400}renderGcodeThumbnails\(body\)/);
      assert.match(script, /HsBaHttp\.parseGcodeThumbnails\(/);
      assert.match(script, /HsBaHttp\.thumbnailDataUrl\(/);
      assert.match(script, /HsBaHttp\.packageImages\(/);
      assert.match(script, /HsBaHttp\.packageEntryUrl\(/);
      // Images reads the package of the job on screen, and nothing else.
      assert.match(script, /callJson\("\/api\/jobs\/" \+ encodeURIComponent\(id\) \+ "\/package"\)/);
      assert.match(script, /browsePackage\(previewedJob\.id\)/);

      // A displayed block is an <img>, a skipped one is a sentence about why it is missing.
      assert.match(script, /image\.setAttribute\("src", url\)/);
      assert.match(script, /note\.className = "thumbnail-note"/);

      // Both strips are laid out, so each needs its own hidden rule: an author display value beats
      // the browser's default for [hidden] and would leave an empty box on screen.
      const stageRule = css.match(/\.thumbnail-stage \{[^}]*\}/);
      assert.ok(stageRule, "the stylesheet no longer lays out the preview strip");
      assert.match(stageRule[0], /display: flex/);
      assert.match(css, /\.thumbnail-stage\[hidden\] \{ display: none; \}/);
      assert.match(css, /#package-stage\[hidden\] \{ display: none; \}/);
      // A raster preview is upscaled, so it has to keep hard pixels, and sit on the black plate a
      // letterboxed image needs.
      const imageRule = css.match(/\.thumbnail-stage img, #package-image \{[^}]*\}/);
      assert.ok(imageRule, "the stylesheet no longer styles the preview images");
      assert.match(imageRule[0], /image-rendering: pixelated/);
      assert.match(imageRule[0], /object-fit: contain/);

      // An embedded image is a data URI, which the page's own policy has to keep allowing.
      const csp = (await fetch(`${base}/`)).headers.get("Content-Security-Policy");
      assert.match(csp, /img-src 'self' data:/);

      // Two more panels, still no column: the shape of the console has not drifted.
      assert.equal(headerCount(html, "jobs-body"), columnConstant(script, "JOB_COLUMNS"));
      assert.equal(columnConstant(script, "JOB_COLUMNS"), 8);
    });
  });

  it("asks the server to capture geometry only when the checkbox is on", async () => {
    await withConsole({}, async ({ base }) => {
      const { script } = await pageBundle(base);
      assert.match(script, /byId\("geometry-capture"\)\.checked\s*\?\s*"\?geometry=1"/);
      assert.match(script, /encodeURIComponent\(kind\) \+ capture/);
    });
  });

  it("has a colour for every job state the API can report", async () => {
    await withConsole({}, async ({ base }) => {
      const css = await (await fetch(`${base}/app.css`)).text();
      // buildJobRow turns the state string straight into a class name, so a state without a rule is
      // rendered in the default text colour - "cancelled" was exactly that gap.
      for (const state of ["queued", "running", "success", "failed", "cancelled"]) {
        assert.match(css, new RegExp(`\\.state-${state}\\s*\\{`), `no colour for ${state}`);
      }
    });
  });

  it("only calls endpoints that the proxy forwards", async () => {
    await withConsole({}, async ({ base }) => {
      const { script } = await pageBundle(base);
      // Anything outside /api/* would miss the proxy and hit the static allowlist instead.
      const prefixes = [
        "/api/health",
        "/api/version",
        "/api/pipelines",
        "/api/config/default/",
        "/api/jobs",
        "/api/uploads",
      ];
      const literals = [...script.matchAll(/"(\/api\/[^"]*)"/g)].map((match) => match[1]);
      assert.ok(literals.length >= 6, literals.join(","));
      for (const literal of literals) {
        assert.ok(prefixes.some((prefix) => literal.startsWith(prefix)), literal);
      }
    });
  });

  it("keeps the page's DOM use inside the browser's API surface", async () => {
    await withConsole({}, async ({ base }) => {
      const { script } = await pageBundle(base);
      // The DOM hands out `children` as a live HTMLCollection: indexed access and a length, but no
      // Array method. Asking it for one throws on the real page and passes against an array-shaped
      // stub, which is exactly how a broken table render once stayed green here.
      assert.doesNotMatch(
        script,
        /\.children\.(map|filter|find|some|every|indexOf|includes|reduce|forEach|flat|slice|join)\(/,
        "app.js calls an Array method on a live collection",
      );
      // The same trap for SVG: getAttribute, not a field, is what a browser exposes.
      assert.match(script, /document\.createElementNS\(SVG_NS, "g"\)/);
    });
  });
});

// ---------------------------- page behaviour ----------------------------

/**
 * One fake element: only the surface app.js actually touches.
 *
 * `children` behaves like the live HTMLCollection a browser hands out: indexed access and a length,
 * and no Array method. Backing it with a plain array let `children.indexOf(...)` pass every page
 * test and throw on the real page, which is how a broken table render once stayed green here.
 */
function makeElement(tag) {
  const kids = [];
  // A text node, the way the DOM has one: writing textContent replaces it even when the string is
  // identical, and a selection lives inside it. Keeping the text in a plain field here would make
  // that rewrite invisible, which is how the poll-clearing-selections bug passed this file.
  let textNode = null;
  const detach = (child) => {
    const at = kids.indexOf(child);
    if (at >= 0) {
      kids.splice(at, 1);
    }
  };
  const collection = new Proxy(kids, {
    get: (target, key) => {
      if (typeof key === "string" && /^\d+$/.test(key)) {
        return target[Number(key)];
      }
      return key === "length" ? target.length : undefined;
    },
    has: (target, key) => typeof key === "string" && /^\d+$/.test(key) && Number(key) < target.length,
  });

  return {
    tagName: String(tag || "div").toUpperCase(),
    className: "",
    value: "",
    title: "",
    hidden: false,
    disabled: false,
    checked: false,
    colSpan: 0,
    scrollTop: 0,
    scrollHeight: 0,
    // A real element keeps its attributes in one map that both getAttribute and the rendered markup
    // read, which is what lets the SVG assertions below look at the same values a browser would.
    attributes: new Map(),
    clientWidth: 640,
    files: [],
    children: collection,
    listeners: new Map(),
    clicked: 0,
    removed: false,
    get textContent() {
      if (textNode) {
        return textNode.nodeValue;
      }
      return kids.map((kid) => kid.textContent).join("");
    },
    set textContent(value) {
      kids.length = 0;
      const text = value === null || value === undefined ? "" : String(value);
      textNode = text === "" ? null : { nodeValue: text };
    },
    get firstChild() {
      return textNode || kids[0] || null;
    },
    get childElementCount() {
      return kids.length;
    },
    get firstElementChild() {
      return kids[0] || null;
    },
    appendChild(child) {
      // Moving a node that is already here must not duplicate it, as in the real DOM.
      detach(child);
      kids.push(child);
      // A <select> with no explicit choice reports its first option as the value, which is what
      // the page reads right after filling the pipeline list.
      if (this.value === "" && child.value) {
        this.value = child.value;
      }
      return child;
    },
    insertBefore(child, reference) {
      detach(child);
      const at = reference ? kids.indexOf(reference) : -1;
      if (at >= 0) {
        kids.splice(at, 0, child);
      } else {
        kids.push(child);
      }
      return child;
    },
    removeChild(child) {
      detach(child);
      return child;
    },
    replaceChildren(...replacements) {
      kids.length = 0;
      textNode = null;
      kids.push(...replacements);
    },
    setAttribute(name, value) {
      this.attributes.set(name, String(value));
    },
    getAttribute(name) {
      return this.attributes.has(name) ? this.attributes.get(name) : null;
    },
    removeAttribute(name) {
      this.attributes.delete(name);
    },
    addEventListener(type, handler) {
      if (!this.listeners.has(type)) {
        this.listeners.set(type, []);
      }
      this.listeners.get(type).push(handler);
    },
    remove() {
      this.removed = true;
    },
    // A press runs the page's own handler, so the assertions cover behaviour rather than markup.
    click() {
      this.clicked += 1;
      for (const handler of this.listeners.get("click") || []) {
        handler({});
      }
    },
  };
}

/**
 * Boot the served app.js against a minimal DOM, the way the browser does.
 *
 * Elements are created on demand by id, so a handler that looks up a renamed node gets nothing where
 * it wrote and the assertions below fail instead of silently passing. Poll intervals are replaced by
 * no-ops, which keeps the test process free of timers, and every awaited fetch chain is drained by
 * settle().
 */
async function bootPage(base) {
  const elements = new Map();
  const body = makeElement("body");
  const store = new Map();
  const domReady = [];
  // index.html ships these checkboxes as checked, and an on-demand stub knows nothing about the
  // markup: left at the ECMAScript default the page would draw no category at all and the geometry
  // assertions below would be measuring a state no browser can ever reach.
  const checkedByDefault = new Set([
    "jobs-auto",
    "geometry-capture",
    "geometry-outline",
    "geometry-fill",
    "geometry-support",
  ]);
  const documentStub = {
    body,
    createElement: (tag) => makeElement(tag),
    createElementNS: (namespace, tag) => Object.assign(makeElement(tag), { namespaceURI: namespace }),
    getElementById: (id) => {
      if (!elements.has(id)) {
        const element = makeElement("div");
        if (checkedByDefault.has(id)) {
          element.checked = true;
        }
        elements.set(id, element);
      }
      return elements.get(id);
    },
    addEventListener: (type, handler) => {
      if (type === "DOMContentLoaded") {
        domReady.push(handler);
      }
    },
  };
  const sandbox = {
    document: documentStub,
    navigator: {},
    location: { origin: base },
    localStorage: {
      getItem: (key) => (store.has(key) ? store.get(key) : null),
      setItem: (key, value) => store.set(key, String(value)),
      removeItem: (key) => store.delete(key),
    },
    Headers,
    FormData,
    Blob,
    TextEncoder,
    // The page only needs an object URL to hand to the anchor, not a working blob registry.
    URL: { createObjectURL: () => "blob:harness", revokeObjectURL: () => {} },
    fetch: (input, init) => fetch(new URL(String(input), base).href, init),
    setTimeout: () => 0,
    setInterval: () => 0,
    console,
  };
  sandbox.window = sandbox;

  const context = vm.createContext(sandbox);
  for (const file of ["httphelpers.js", "app.js"]) {
    const source = await (await fetch(`${base}/${file}`)).text();
    vm.runInContext(source, context, { filename: file });
  }
  for (const handler of domReady) {
    handler();
  }

  const harness = {
    body,
    byId: (id) => documentStub.getElementById(id),
    /** Let the fetch chain a click started finish; bounded, so a missing update fails loudly. */
    async settle(predicate) {
      for (let attempt = 0; attempt < 200; attempt += 1) {
        if (predicate()) {
          return true;
        }
        await new Promise((resolve) => setImmediate(resolve));
      }
      return predicate();
    },
    // run() reports failures in the log panel, so an uninspected log would hide every broken action.
    logErrors: () =>
      Array.from(harness.byId("log").children)
        .filter((line) => line.className.includes("log-error"))
        .map((line) => line.textContent),
  };
  const booted = await harness.settle(() => harness.byId("jobs-body").children.length >= 2);
  if (!booted) {
    // The page reports a broken action in its own log, so a failure here is unreadable without it.
    assert.fail(`the jobs table stayed empty; page log: ${harness.logErrors().join(" | ") || "empty"}`);
  }
  return harness;
}

/** @brief The child elements as an array: `children` is a live collection, as in the DOM. */
function cells(node) {
  return Array.from(node.children);
}

/** @brief The button labelled @p label in the Actions cell of one job row. */
function jobButton(harness, index, label) {
  const row = harness.byId("jobs-body").children[index];
  assert.ok(row, `job row ${index} was not rendered`);
  const actions = row.children[row.children.length - 1];
  const button = cells(actions).find((child) => child.textContent === label);
  assert.ok(button, `no "${label}" button (got ${cells(actions).map((child) => child.textContent).join(", ")})`);
  return button;
}

/**
 * @brief The <g> groups the geometry SVG currently holds, as {class, d[]} pairs.
 *
 * Read through getAttribute rather than a field: the page writes `d` the way the DOM does, so an
 * attribute typo that would render an empty drawing in a browser fails here too.
 */
function svgGroups(harness) {
  return cells(harness.byId("geometry-svg")).map((group) => ({
    class: group.getAttribute("class"),
    d: cells(group).map((path) => path.getAttribute("d")),
  }));
}

/**
 * @brief Every descendant node of one element, depth first.
 *
 * A stack drawing nests three levels (<g> per layer, <g> per category, <path>), so an assertion on
 * the direct children alone cannot see a direction mark at all. `children` stays a live collection.
 */
function descendants(node) {
  const found = [];
  for (const child of cells(node)) {
    found.push(child);
    found.push.apply(found, descendants(child));
  }
  return found;
}

/** @brief The single-layer geometry requests the fake server has seen, in order. */
function layerCalls(upstream) {
  return upstream.calls.filter((call) => /\/geometry\/\d+$/.test(call.url)).map((call) => call.url);
}

/** Flip a checkbox the way a real click would, then let the page's own handler run. */
function toggleCheckbox(harness, id) {
  const box = harness.byId(id);
  box.checked = !box.checked;
  box.click();
  return box;
}

/**
 * @brief Run the handlers the page registered for an event `click()` does not cover.
 *
 * A fake element only presses its click listeners, so a `change` handler (the pipeline selector is
 * the one the preview-image option hangs on) would never run without this.
 */
function fire(node, type) {
  for (const handler of node.listeners.get(type) || []) {
    handler({});
  }
}

/** @brief Every line the page's own log panel holds, oldest first. */
function logLines(harness) {
  return cells(harness.byId("log")).map((line) => line.textContent);
}

describe("page behaviour", () => {
  it("renders the served jobs without a single failed action", async () => {
    await withConsole({}, async ({ base }) => {
      const harness = await bootPage(base);
      assert.deepEqual(harness.logErrors(), []);
      const rows = harness.byId("jobs-body").children;
      assert.equal(rows.length, 2);
      assert.ok(
        cells(rows[0]).some((cell) => cell.textContent === "/data/hsba/jobs/job-1/part.gcode"),
        "the Output column is not fed by output_path",
      );
      assert.equal(jobButton(harness, 0, "Preview").disabled, false);
      // The uploads table answers 503 upstream and must degrade to a message, not a failed action.
      assert.ok(
        await harness.settle(() => harness.byId("uploads-body").children.length >= 1),
        "the uploads table stayed empty",
      );
      const uploadRow = harness.byId("uploads-body").children[0];
      assert.match(cells(uploadRow).map((cell) => cell.textContent).join(" "), /model upload disabled/);
    });
  });

  it("updates a refreshed row in place instead of rebuilding the table", async () => {
    await withConsole({}, async ({ upstream, base }) => {
      const harness = await bootPage(base);
      const body = harness.byId("jobs-body");
      assert.ok(await harness.settle(() => body.children.length === 2), "the jobs table did not fill up");
      const reused = body.children[0];
      const untouched = body.children[1];
      const pathCell = reused.children[6];
      const preview = jobButton(harness, 0, "Preview");

      // The same job is still listed, only further along, and a queued one joins in front of it.
      upstream.state.jobs = [
        {
          id: "job-0", kind: "sla", state: "queued", success: 0, progress_percent: 0,
          progress_stage: "", total_layers: 0, elapsed_seconds: 0, output_path: "", error_message: "",
        },
        {
          id: "job-1", kind: "fdm", state: "running", success: 0, progress_percent: 40,
          progress_stage: "slicing", total_layers: 5, elapsed_seconds: 2.5, output_path: "", error_message: "",
        },
        DEFAULT_JOBS[1],
      ];
      harness.byId("jobs-refresh").click();
      assert.ok(await harness.settle(() => body.children.length === 3), "the new listing never reached the table");

      // Rebuilding every row is what cleared a selection, dropped the hovered highlight and made a
      // click land on a detached node, so the surviving rows have to be the very same objects.
      assert.equal(body.children[1], reused, "a refresh rebuilt a row that was already on screen");
      assert.equal(body.children[2], untouched, "a row that did not move was still relocated");
      assert.equal(reused.children[6], pathCell, "the path cell of the reused row was rebuilt");
      assert.equal(reused.children[2].textContent, "running", "the reused row still shows its old state");
      assert.equal(reused.children[6].textContent, "-", "a job with no result kept its old path");
      assert.equal(reused.children.length, 8, "the job row lost a column");

      const previewAfter = jobButton(harness, 1, "Preview");
      assert.equal(previewAfter, preview, "the reused row's Preview button was replaced");
      assert.equal(previewAfter.disabled, true, "a running job is still offered a Preview");
      assert.equal(jobButton(harness, 1, "Delete").disabled, true, "a running job may be deleted");
      assert.equal(body.children[0].children[0].textContent, "job-0", "the queued job was not inserted");
      assert.deepEqual(harness.logErrors(), []);
    });
  });

  it("leaves an unchanged cell alone so a text selection can outlive a poll", async () => {
    await withConsole({}, async ({ upstream, base }) => {
      const harness = await bootPage(base);
      const body = harness.byId("jobs-body");
      assert.ok(await harness.settle(() => body.children.length === 2), "the jobs table did not fill up");
      const row = body.children[0];
      const cell = row.children[6];
      const written = cell.firstChild;
      assert.equal(
        written && written.nodeValue,
        "/data/hsba/jobs/job-1/part.gcode",
        "the path cell has no text node holding the reported path",
      );

      // Keeping the row is not enough: assigning textContent, even with the identical string, swaps
      // the text node out, and a selection made in the path column dies with the node it anchored to.
      // The listing moves one other column forward, so that cell is what proves the render ran -
      // waiting on the request count would let the assertions read a row nobody has updated yet.
      upstream.state.jobs = [
        Object.assign({}, DEFAULT_JOBS[0], { progress_percent: 55, progress_stage: "slicing" }),
        DEFAULT_JOBS[1],
      ];
      harness.byId("jobs-refresh").click();
      assert.ok(
        await harness.settle(() => row.children[3].textContent.includes("55%")),
        "the refreshed listing never reached the row",
      );
      assert.equal(cell.textContent, "/data/hsba/jobs/job-1/part.gcode", "an unchanged cell lost its text");
      assert.equal(cell.firstChild, written, "a poll rewrote a cell whose text did not change");

      // A value that really changed still has to land, in a fresh node inside the same cell.
      upstream.state.jobs = [
        Object.assign({}, DEFAULT_JOBS[0], { output_path: "/data/hsba/jobs/job-1/rebuilt.gcode" }),
        DEFAULT_JOBS[1],
      ];
      harness.byId("jobs-refresh").click();
      assert.ok(
        await harness.settle(() => cell.textContent === "/data/hsba/jobs/job-1/rebuilt.gcode"),
        "a changed path never reached the cell",
      );
      assert.equal(body.children[0].children[6], cell, "the cell of an unchanged row was rebuilt");
      assert.notEqual(cell.firstChild, written, "the rewritten cell kept its old text node");
      assert.deepEqual(harness.logErrors(), []);
    });
  });

  it("shows the stored path of an upload as its own column", async () => {
    await withConsole({}, async ({ upstream, base }) => {
      const harness = await bootPage(base);
      upstream.state.uploads = [
        {
          id: "upl-1", job_id: "job-1", original_name: "bunny.stl", file_name: "bunny.stl",
          path: "/data/hsba/uploads/upl-1/bunny.stl", size_bytes: 2048, state: "available",
          remaining_seconds: 600,
        },
      ];
      harness.byId("uploads-refresh").click();
      const body = harness.byId("uploads-body");
      // The boot listing has no upload pool, so the table sits on a single placeholder row: waiting
      // for "one row" would pass on that row instead of the upload.
      const filled = await harness.settle(() =>
        body.children.length === 1 && body.children[0].children.length === 8 &&
        body.children[0].children[0].textContent === "upl-1");
      assert.ok(filled, `the uploads table stayed on its placeholder (${cells(body.children[0]).map((c) => c.textContent).join("/")})`);

      const row = body.children[0];
      assert.equal(row.children[2].textContent, "bunny.stl");
      // The stored path used to live only in the tooltip: reading the screen has to give it up.
      assert.equal(row.children[3].textContent, "/data/hsba/uploads/upl-1/bunny.stl");
      assert.match(row.children[3].className, /\bpath\b/, "the stored path is not a wrapping path cell");
      assert.equal(row.children[4].textContent, "2.0 KiB");
      assert.equal(row.children[7].children[0].textContent, "Delete");
      assert.equal(row.children[7].children[0].disabled, false);
      assert.deepEqual(harness.logErrors(), []);
    });
  });

  it("previews an inline text result and names it after the job", async () => {
    await withConsole({}, async ({ base }) => {
      const harness = await bootPage(base);

      jobButton(harness, 1, "Preview").click();
      assert.ok(
        await harness.settle(() => harness.byId("preview").hidden === false),
        `the text result stayed hidden (${harness.byId("preview-meta").textContent})`,
      );
      assert.equal(harness.byId("preview").textContent, INLINE_GCODE);
      // No Content-Disposition on the inline branch, so the panel titles it job id + .gcode.
      assert.match(harness.byId("preview-title").textContent, /job-2\.gcode/);
      // The comment header is part of the file, so it counts toward the reported size and lines.
      assert.match(
        harness.byId("preview-meta").textContent,
        new RegExp(`${Buffer.byteLength(INLINE_GCODE, "utf8")} B, ${INLINE_GCODE.split("\n").length} lines`),
      );
      assert.doesNotMatch(harness.byId("preview-meta").textContent, /first/);
      assert.doesNotMatch(harness.byId("preview-meta").textContent, /no embedded previews/);
      assert.deepEqual(harness.logErrors(), []);

      harness.byId("preview-clear").click();
      assert.ok(await harness.settle(() => harness.byId("preview").hidden === true));
      assert.equal(harness.byId("preview-title").textContent, "Result preview");
      assert.equal(harness.byId("preview-meta").textContent, "no result previewed yet");
      // Clearing is a clean slate for everything the panel showed, pictures included.
      assert.equal(harness.byId("gcode-thumbnails").hidden, true);
      assert.deepEqual(cells(harness.byId("gcode-thumbnails")), []);
      assert.equal(harness.byId("package-stage").hidden, true);
      assert.equal(harness.byId("package-image").getAttribute("src"), null);
    });
  });

  it("draws the preview images a G-code result embedded in its own header", async () => {
    await withConsole({}, async ({ upstream, base }) => {
      const harness = await bootPage(base);

      jobButton(harness, 1, "Preview").click();
      const stage = harness.byId("gcode-thumbnails");
      assert.ok(
        await harness.settle(() => stage.hidden === false),
        `the embedded previews never reached the panel (${harness.byId("preview-meta").textContent})`,
      );
      const image = cells(stage).find((node) => node.tagName === "IMG");
      assert.ok(image, "a Base64 block was not turned into a picture");
      // The bytes go to the browser as a data URI, so the picture costs no second request.
      assert.equal(image.getAttribute("src"), "data:image/png;base64,aGVsbG8=");
      assert.equal(image.getAttribute("alt"), "model_top");
      assert.equal(
        upstream.calls.filter((call) => /\/result$/.test(call.url)).length,
        1,
        "the preview images were fetched a second time",
      );
      const label = cells(stage).find((node) => node.className === "thumbnail-label");
      assert.equal(label.textContent, "model_top (8x8)", "the picture is not attributed to a view and size");
      assert.deepEqual(harness.logErrors(), []);
    });
  });

  it("quotes the engine's reason for a view that never became a picture", async () => {
    await withConsole({}, async ({ base }) => {
      const harness = await bootPage(base);

      jobButton(harness, 1, "Preview").click();
      assert.ok(
        await harness.settle(() => harness.byId("gcode-thumbnails").hidden === false),
        "the preview strip stayed hidden",
      );
      const notes = cells(harness.byId("gcode-thumbnails")).filter((node) => node.className === "thumbnail-note");
      assert.equal(notes.length, 1, notes.map((node) => node.textContent).join(" | "));
      // The skip notice states which view is missing and why, in the engine's own word for it.
      assert.equal(notes[0].textContent, "model_side was not embedded: mesh_too_large");
      assert.deepEqual(harness.logErrors(), []);
    });
  });

  it("tells the operator how to get previews when a result embedded none", async () => {
    await withConsole({}, async ({ upstream, base }) => {
      // The same job, run without the option: a plain file with no comment header at all.
      upstream.state.inlineResult = "G21\nG90\nG1 X1.0 Y2.0 E3.0 F1200\n";
      const harness = await bootPage(base);

      jobButton(harness, 1, "Preview").click();
      assert.ok(
        await harness.settle(() => /gcode_thumbnails/.test(harness.byId("preview-meta").textContent)),
        `a preview-less run said nothing about the option (${harness.byId("preview-meta").textContent})`,
      );
      // An empty strip is an empty strip: no box, no leftover picture from an earlier result.
      assert.equal(harness.byId("gcode-thumbnails").hidden, true);
      assert.deepEqual(cells(harness.byId("gcode-thumbnails")), []);
      assert.deepEqual(harness.logErrors(), []);
    });
  });

  it("opens the images of a packed result and steps through them without re-listing", async () => {
    await withConsole({}, async ({ upstream, base }) => {
      const harness = await bootPage(base);
      // job-1's result is a binary download, which is exactly the case Images exists for.
      jobButton(harness, 0, "Preview").click();
      assert.ok(
        await harness.settle(() => /not text/.test(harness.byId("preview-meta").textContent)),
        "the binary result never landed",
      );

      harness.byId("package-browse").click();
      assert.ok(
        await harness.settle(() => harness.byId("package-stage").hidden === false),
        `the package strip stayed shut (${harness.byId("package-meta").textContent})`,
      );
      const listings = () => upstream.calls.filter((call) => call.url === "/api/jobs/job-1/package").length;
      assert.equal(listings(), 1, "opening the package listed it more than once");
      // The first image is the lowest layer, addressed by the entry URL the tested helper builds.
      assert.equal(harness.byId("package-image").getAttribute("src"), "/api/jobs/job-1/package/layers/layer_0.png");
      assert.equal(harness.byId("package-meta").textContent, "layers/layer_0.png (1/2)");

      harness.byId("package-next").click();
      assert.ok(
        await harness.settle(() => /layer_1\.png/.test(harness.byId("package-image").getAttribute("src") || "")),
        `Next never moved the strip (${harness.byId("package-meta").textContent})`,
      );
      assert.equal(harness.byId("package-meta").textContent, "layers/layer_1.png (2/2)");

      // Both ends stop instead of wrapping, and none of the stepping costs the server a listing.
      harness.byId("package-next").click();
      await harness.settle(() => true);
      assert.match(harness.byId("package-image").getAttribute("src"), /layer_1\.png$/);
      harness.byId("package-prev").click();
      assert.ok(
        await harness.settle(() => /layer_0\.png/.test(harness.byId("package-image").getAttribute("src") || "")),
        "Prev never went back",
      );
      assert.equal(listings(), 1, "stepping through the package relisted it");

      // The strip belongs to the job it was opened for. Previewing another result has to put its
      // pictures away, or the operator reads job-1's layer under job-2's heading - the fake DOM
      // keeps every id detached, so only this behavioural assertion can catch a missed reset.
      jobButton(harness, 1, "Preview").click();
      assert.ok(
        await harness.settle(() => /job-2/.test(harness.byId("preview-title").textContent)),
        `the second preview never reached the heading (${harness.byId("preview-title").textContent})`,
      );
      assert.equal(harness.byId("package-stage").hidden, true, "the previous job's layer stayed on screen");
      assert.equal(harness.byId("package-image").getAttribute("src"), null, "the strip still points at the old entry");
      assert.equal(harness.byId("package-meta").textContent, "", "the old caption survived the switch");
      assert.deepEqual(harness.logErrors(), []);
    });
  });

  it("says plainly when there is no package to open", async () => {
    await withConsole({}, async ({ base }) => {
      const harness = await bootPage(base);

      // Nothing previewed yet: the strip has no job to ask about, and must not guess one.
      harness.byId("package-browse").click();
      assert.ok(
        await harness.settle(() => /preview a result first/.test(logLines(harness).join(" | "))),
        "Images with no result on screen said nothing",
      );
      assert.deepEqual(harness.logErrors(), []);

      // A single-file result: the server's own sentence is what the strip shows, and the failure is
      // still logged because an operator reading the screen cannot see the response body.
      jobButton(harness, 1, "Preview").click();
      assert.ok(await harness.settle(() => harness.byId("preview").hidden === false), "the preview never came up");
      harness.byId("package-browse").click();
      assert.ok(
        await harness.settle(() => /not a package/.test(harness.byId("package-meta").textContent)),
        `a single-file result was not explained (${harness.byId("package-meta").textContent})`,
      );
      assert.equal(harness.byId("package-meta").textContent, "this result is a single file, not a package");
      assert.equal(harness.byId("package-stage").hidden, true);
      assert.equal(harness.logErrors().length, 1, harness.logErrors().join(" | "));
      assert.match(harness.logErrors()[0], /result is not a package/);
    });
  });

  it("sends the preview-image flag only to the pipeline that embeds previews", async () => {
    await withConsole({}, async ({ upstream, base }) => {
      const harness = await bootPage(base);
      assert.ok(
        await harness.settle(() => harness.byId("kind").value === "fdm" && harness.byId("config").value.length > 0),
        "the pipeline list or its default config never arrived",
      );
      assert.equal(harness.byId("fdm-thumbnails").disabled, false, "fdm is selected and the option is greyed out");
      const posted = () =>
        upstream.calls.filter((call) => call.method === "POST" && (call.url || "").startsWith("/api/jobs/fdm"));

      toggleCheckbox(harness, "fdm-thumbnails");
      harness.byId("submit").click();
      assert.ok(await harness.settle(() => posted().length === 1), "no submission reached the server");
      assert.equal(JSON.parse(posted()[0].body).gcode_thumbnails, 1);
      // The patch adds a key, it does not replace the config the operator loaded.
      assert.equal(JSON.parse(posted()[0].body).layer_height, 0.2);

      toggleCheckbox(harness, "fdm-thumbnails");
      harness.byId("submit").click();
      assert.ok(await harness.settle(() => posted().length === 2), "the second submission never arrived");
      assert.equal(JSON.parse(posted()[1].body).gcode_thumbnails, 0, "an unticked box must still say so");

      // A config the console cannot parse is submitted exactly as it stands, with a line in the log.
      harness.byId("config").value = "layer_height = 0.2\n";
      harness.byId("submit").click();
      assert.ok(await harness.settle(() => posted().length === 3), "the verbatim submission never arrived");
      assert.equal(posted()[2].body, "layer_height = 0.2");
      assert.match(logLines(harness).join(" | "), /gcode_thumbnails not applied/);

      // Another pipeline has no G-code to embed, so its option is off and its config is untouched.
      const kind = harness.byId("kind");
      kind.value = "sla";
      fire(kind, "change");
      assert.ok(
        await harness.settle(() => harness.byId("fdm-thumbnails").disabled === true),
        "a package-writing kind kept the preview option enabled",
      );
      assert.deepEqual(harness.logErrors(), []);
    });
  });

  it("refuses to preview a binary result but still downloads it under the server's name", async () => {
    await withConsole({}, async ({ base }) => {
      const harness = await bootPage(base);

      jobButton(harness, 0, "Preview").click();
      assert.ok(
        await harness.settle(() => /not text/.test(harness.byId("preview-meta").textContent)),
        `a binary body was previewed as text (${harness.byId("preview-meta").textContent})`,
      );
      assert.equal(harness.byId("preview").hidden, true);
      assert.equal(harness.byId("preview").textContent, "");

      jobButton(harness, 0, "Result").click();
      assert.ok(
        await harness.settle(() => cells(harness.body).some((child) => child.tagName === "A")),
        "no download anchor was created",
      );
      const anchor = cells(harness.body).find((child) => child.tagName === "A");
      assert.equal(anchor.download, "part.gcode");
      assert.equal(anchor.clicked, 1);
      assert.equal(anchor.removed, true);
      assert.deepEqual(harness.logErrors(), []);
    });
  });

  it("draws a finished job's layer as mirrored SVG paths", async () => {
    await withConsole({}, async ({ base }) => {
      const harness = await bootPage(base);
      jobButton(harness, 0, "Geometry").click();
      assert.ok(
        await harness.settle(() => harness.byId("geometry-stage").hidden === false),
        `the geometry stage stayed hidden (${harness.byId("geometry-meta").textContent})`,
      );
      // The view fits the layer with its margin, and the y axis is flipped about the mid-height so
      // the far side of the bed is at the top of the screen, as every slicer preview shows it.
      assert.equal(harness.byId("geometry-svg").getAttribute("viewBox"), "-0.4 -0.4 20.8 10.8");
      assert.deepEqual(svgGroups(harness), [
        { class: "geometry-outline", d: ["M 0 10 L 20 10 L 20 0 L 0 0 Z"] },
        { class: "geometry-fill", d: ["M 2 8 L 18 8"] },
      ]);
      const meta = harness.byId("geometry-meta").textContent;
      assert.match(meta, /layer 1 \/ 2/);
      assert.match(meta, /z 0\.3 mm/);
      assert.match(meta, /outlines 1 \/ fills 1 \/ supports 0/);
      assert.equal(harness.byId("geometry-layer").max, "1");
      assert.equal(harness.byId("geometry-layer").value, "0");
      assert.deepEqual(harness.logErrors(), []);
    });
  });

  it("steps to the next layer and refetches only that layer", async () => {
    await withConsole({}, async ({ upstream, base }) => {
      const harness = await bootPage(base);
      jobButton(harness, 0, "Geometry").click();
      assert.ok(
        await harness.settle(() => harness.byId("geometry-layer-label").textContent === "1 / 2"),
        `the first layer never landed (${harness.byId("geometry-meta").textContent})`,
      );
      const geometryCalls = () =>
        upstream.calls.filter((call) => /\/geometry(\/\d+)?$/.test(call.url)).map((call) => call.url);

      harness.byId("geometry-next").click();
      assert.ok(
        await harness.settle(() => harness.byId("geometry-layer-label").textContent === "2 / 2"),
        "stepping forward never reached the panel",
      );
      assert.deepEqual(geometryCalls(), [
        "/api/jobs/job-1/geometry",
        "/api/jobs/job-1/geometry/0",
        "/api/jobs/job-1/geometry/1",
      ]);
      assert.match(harness.byId("geometry-meta").textContent, /z 0\.6 mm/);
      assert.deepEqual(svgGroups(harness), [
        { class: "geometry-outline", d: ["M 2 8 L 18 8 L 18 2 L 2 2 Z"] },
        { class: "geometry-fill", d: ["M 4 6 L 16 6 L 16 4 L 4 4 Z"] },
        { class: "geometry-support", d: ["M 10 8 L 10 2"] },
      ]);
      assert.deepEqual(harness.logErrors(), []);
    });
  });

  it("hides a category without asking the server again", async () => {
    await withConsole({}, async ({ upstream, base }) => {
      const harness = await bootPage(base);
      jobButton(harness, 0, "Geometry").click();
      assert.ok(
        await harness.settle(() => svgGroups(harness).length === 2),
        "both categories of the first layer never appeared",
      );
      const before = upstream.calls.length;
      toggleCheckbox(harness, "geometry-fill");
      assert.ok(
        await harness.settle(() => svgGroups(harness).length === 1),
        "unchecking infill left it on screen",
      );
      assert.deepEqual(svgGroups(harness).map((group) => group.class), ["geometry-outline"]);
      assert.equal(upstream.calls.length, before, "toggling a category refetched the layer");
      assert.deepEqual(harness.logErrors(), []);
    });
  });

  it("states why a job with no captured geometry cannot be previewed", async () => {
    await withConsole({}, async ({ upstream, base }) => {
      const harness = await bootPage(base);
      const body = harness.byId("jobs-body");
      jobButton(harness, 0, "Geometry").click();
      assert.ok(
        await harness.settle(() => harness.byId("geometry-stage").hidden === false),
        "the drawing of the first job never appeared",
      );

      // A job the server refuses to answer has to take the stale drawing with it, and say so in the
      // panel: an error only in the log is invisible to someone looking at the picture.
      upstream.state.jobs = [DEFAULT_JOBS[0], Object.assign({}, DEFAULT_JOBS[0], { id: "job-3" })];
      harness.byId("jobs-refresh").click();
      assert.ok(
        await harness.settle(() =>
          body.children.length === 2 && body.children[1].children[0].textContent === "job-3"),
        "the job without geometry never reached the table",
      );
      jobButton(harness, 1, "Geometry").click();
      assert.ok(
        await harness.settle(() => /geometry=1/.test(harness.byId("geometry-meta").textContent)),
        `no server sentence in the panel (${harness.byId("geometry-meta").textContent})`,
      );
      assert.equal(harness.byId("geometry-stage").hidden, true);
      assert.deepEqual(svgGroups(harness), []);
      assert.match(harness.byId("geometry-meta").textContent, /submit with \?geometry=1/);
      assert.match(harness.byId("geometry-meta").textContent, /snapshot pool/);
      assert.match(harness.logErrors().join(" "), /job captured no layer geometry/);
    });
  });

  it("keeps Geometry off for a job whose snapshot is gone", async () => {
    await withConsole({}, async ({ base }) => {
      const harness = await bootPage(base);
      assert.equal(jobButton(harness, 0, "Geometry").disabled, false, "a captured job lost its button");
      assert.equal(jobButton(harness, 1, "Geometry").disabled, true, "a job with no geometry is offered a preview");
    });
  });

  it("stacks the sampled layers of the same snapshot without a second server", async () => {
    await withConsole({}, async ({ upstream, base }) => {
      const harness = await bootPage(base);
      jobButton(harness, 0, "Geometry").click();
      assert.ok(
        await harness.settle(() => harness.byId("geometry-stage").hidden === false),
        "the layer view never came up",
      );
      const before = layerCalls(upstream).length;

      harness.byId("geometry-view-stack").click();
      assert.ok(
        await harness.settle(() => /stack 2 layers/.test(harness.byId("geometry-meta").textContent)),
        `the stack never reached the panel (${harness.byId("geometry-meta").textContent})`,
      );
      // Each sampled layer is its own group, so a ring can still be attributed to a height.
      const planes = descendants(harness.byId("geometry-svg")).filter((node) => node.getAttribute("data-layer") !== null);
      assert.deepEqual(planes.map((node) => node.getAttribute("data-layer")), ["0", "1"]);
      assert.match(planes[0].getAttribute("class"), /geometry-focus/, "the sample highlights no layer");
      // The sample is what a two-layer job has: both layers, asked for one at a time.
      assert.deepEqual(layerCalls(upstream).slice(before), [
        "/api/jobs/job-1/geometry/0",
        "/api/jobs/job-1/geometry/1",
      ]);
      // A stacked drawing has no single layer to step through, so the stepper stands down.
      assert.equal(harness.byId("geometry-layer").disabled, true);
      assert.deepEqual(harness.logErrors(), []);
    });
  });

  it("keeps a sampled stack in memory when the operator switches back and forth", async () => {
    await withConsole({}, async ({ upstream, base }) => {
      const harness = await bootPage(base);
      jobButton(harness, 0, "Geometry").click();
      assert.ok(
        await harness.settle(() => harness.byId("geometry-stage").hidden === false),
        "the layer view never came up",
      );
      harness.byId("geometry-view-stack").click();
      assert.ok(
        await harness.settle(() => /stack 2 layers/.test(harness.byId("geometry-meta").textContent)),
        "the stack never reached the panel",
      );
      const stacked = layerCalls(upstream).length;

      // Spinning the view must re-project from what is already fetched: asking the server for the
      // same two layers again would put a hundred requests behind the slice the page is watching.
      harness.byId("geometry-view-layer").click();
      assert.ok(
        await harness.settle(() => /layer 1 \/ 2/.test(harness.byId("geometry-meta").textContent)),
        "the single layer never came back",
      );
      const afterLayer = layerCalls(upstream).length;
      assert.equal(afterLayer, stacked + 1, "returning to the layer view fetched more than its own layer");

      harness.byId("geometry-view-stack").click();
      assert.ok(
        await harness.settle(() => /stack 2 layers/.test(harness.byId("geometry-meta").textContent)),
        "the stack never came back",
      );
      assert.equal(layerCalls(upstream).length, afterLayer, "the second stack refetched its layers");
      assert.deepEqual(harness.logErrors(), []);
    });
  });

  it("draws the continuous toolpath from the path endpoint alone", async () => {
    await withConsole({}, async ({ upstream, base }) => {
      const harness = await bootPage(base);
      jobButton(harness, 0, "Geometry").click();
      assert.ok(
        await harness.settle(() => harness.byId("geometry-stage").hidden === false),
        "the layer view never came up",
      );

      harness.byId("geometry-view-path").click();
      assert.ok(
        await harness.settle(() => /path 6 points/.test(harness.byId("geometry-meta").textContent)),
        `the path never reached the panel (${harness.byId("geometry-meta").textContent})`,
      );
      const asked = upstream.calls.filter((call) => /\/geometry\/path$/.test(call.url));
      assert.equal(asked.length, 1);
      // The polyline is its own data: getting it must not drag the per-layer endpoint along.
      assert.deepEqual(layerCalls(upstream), ["/api/jobs/job-1/geometry/0"]);

      // What the page drew has to be what the helpers say the served body projects to.
      const http_ = await loadHelpers(base);
      const expected = http_.projectedPathPolyline(
        GEOMETRY_PATH_BODY.xyz,
        http_.GEOMETRY_STACK_ANGLES,
      );
      const groups = descendants(harness.byId("geometry-svg")).filter((node) => node.getAttribute("class") === "geometry-path");
      assert.equal(groups.length, 1);
      assert.equal(cells(groups[0])[0].getAttribute("d"), expected.d);
      assert.match(harness.byId("geometry-meta").textContent, /z 0\.3 - 2\.4 mm/);

      // A second press is a redraw, not a second download of the same polyline.
      harness.byId("geometry-view-path").click();
      harness.byId("geometry-view-path").click();
      await harness.settle(() => true);
      assert.equal(
        upstream.calls.filter((call) => /\/geometry\/path$/.test(call.url)).length,
        1,
        "re-entering the path view refetched the toolpath",
      );
      assert.deepEqual(harness.logErrors(), []);
    });
  });

  it("refuses the path view for a run that deposited no continuous line", async () => {
    await withConsole({}, async ({ upstream, base }) => {
      const harness = await bootPage(base);
      // The listing stands in for a run that captured its layers but never deposited a helix: the
      // overview says has_path 0, and that is the only thing the panel has to go on.
      upstream.state.jobs = [
        DEFAULT_JOBS[0],
        Object.assign({}, DEFAULT_JOBS[1], { geometry_available: 1, geometry_layers: 2 }),
      ];
      harness.byId("jobs-refresh").click();
      const body = harness.byId("jobs-body");
      assert.ok(
        await harness.settle(() => body.children.length === 2 && body.children[1].children[0].textContent === "job-2"),
        "the run without a path never reached the table",
      );
      jobButton(harness, 1, "Geometry").click();
      assert.ok(
        await harness.settle(() => harness.byId("geometry-stage").hidden === false),
        "the layers of a non-spiral run were not drawn",
      );
      assert.equal(harness.byId("geometry-view-path").disabled, true, "a run with no path is offered the Path view");

      // A forced click (the DOM still routes a disabled button's handler to nobody in a browser,
      // but the page's own gate has to hold) says so without spending a request that can only 404.
      const before = upstream.calls.length;
      harness.byId("geometry-view-path").click();
      assert.ok(
        await harness.settle(() => /spiral_mode=1/.test(harness.byId("geometry-meta").textContent)),
        `the refusal never reached the panel (${harness.byId("geometry-meta").textContent})`,
      );
      assert.equal(harness.byId("geometry-stage").hidden, true, "a refused view kept the stale drawing");
      assert.equal(upstream.calls.length, before, "the refusal still asked the server for a path");
      assert.equal(harness.logErrors().length, 1, harness.logErrors().join(" | "));
      assert.match(harness.logErrors()[0], /spiral_mode=1/);
    });
  });

  it("colours the drawing by deposition order without asking the server again", async () => {
    await withConsole({}, async ({ upstream, base }) => {
      const harness = await bootPage(base);
      jobButton(harness, 0, "Geometry").click();
      assert.ok(
        await harness.settle(() => harness.byId("geometry-stage").hidden === false),
        "the layer view never came up",
      );
      harness.byId("geometry-view-stack").click();
      assert.ok(
        await harness.settle(() => /stack 2 layers/.test(harness.byId("geometry-meta").textContent)),
        "the stack never reached the panel",
      );
      const before = upstream.calls.length;

      toggleCheckbox(harness, "geometry-order");
      assert.ok(
        await harness.settle(() => harness.byId("geometry-svg").getAttribute("class") === "geometry-ordered"),
        "the ordered class never reached the drawing",
      );
      const strokes = descendants(harness.byId("geometry-svg"))
        .filter((node) => node.getAttribute("d") !== null && node.getAttribute("stroke") !== null)
        .map((node) => node.getAttribute("stroke"));
      assert.ok(strokes.length >= 4, `only ${strokes.length} paths were coloured`);
      assert.equal(new Set(strokes).size, strokes.length, "two elements of the sequence shared a colour");
      for (const stroke of strokes) {
        assert.match(stroke, /^#[0-9a-f]{6}$/);
      }
      assert.ok(
        descendants(harness.byId("geometry-svg")).some((node) => node.getAttribute("class") === "geometry-arrow"),
        "the coloured drawing carries no direction mark",
      );

      // Unchecking hands the colours back to the stylesheet, attributes and all.
      toggleCheckbox(harness, "geometry-order");
      assert.ok(
        await harness.settle(() => harness.byId("geometry-svg").getAttribute("class") === null),
        "the ordered class outlived the checkbox",
      );
      assert.ok(
        descendants(harness.byId("geometry-svg")).every((node) => node.getAttribute("stroke") === null),
        "a path kept its sequence colour after the switch went off",
      );
      assert.equal(upstream.calls.length, before, "toggling the colouring refetched the geometry");
      assert.deepEqual(harness.logErrors(), []);
    });
  });

  it("cancels a running job and refreshes the listing", async () => {
    await withConsole({}, async ({ upstream, base }) => {
      const harness = await bootPage(base);
      const body = harness.byId("jobs-body");
      upstream.state.jobs = [
        {
          id: "job-3", kind: "fdm", state: "running", success: 0, progress_percent: 30,
          progress_stage: "", progress_stage_id: "slicing.layer", total_layers: 5,
          elapsed_seconds: 1.5, output_path: "", error_message: "",
          geometry_layers: 0, geometry_available: 0,
        },
      ];
      harness.byId("jobs-refresh").click();
      assert.ok(
        await harness.settle(() =>
          body.children.length === 1 && cells(body.children[0])[2].textContent === "running"),
        "the running job never reached the table",
      );
      // A pipeline that reports no free-form text still shows the stable stage key the API derived.
      assert.match(cells(body.children[0])[3].textContent, /30% slicing\.layer/);
      assert.equal(jobButton(harness, 0, "Cancel").disabled, false);
      assert.equal(jobButton(harness, 0, "Preview").disabled, true);
      assert.equal(jobButton(harness, 0, "Delete").disabled, true);

      const listed = upstream.calls.filter((call) => call.url.startsWith("/api/jobs?limit")).length;
      jobButton(harness, 0, "Cancel").click();
      assert.ok(
        await harness.settle(() =>
          upstream.calls.some((call) => call.method === "POST" && call.url === "/api/jobs/job-3/cancel")),
        "no cancel request reached the server",
      );
      assert.ok(
        await harness.settle(() =>
          upstream.calls.filter((call) => call.url.startsWith("/api/jobs?limit")).length > listed),
        "the listing was never refreshed after cancelling",
      );
      // Cancellation is cooperative, so the log must not claim the job already stopped.
      assert.match(cells(harness.byId("log")).pop().textContent, /next safe point/);
      assert.deepEqual(harness.logErrors(), []);
    });
  });
});
