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
    progress_stage: "done", total_layers: 5, elapsed_seconds: 1.25,
    output_path: "/data/hsba/jobs/job-1/part.gcode", error_message: "",
  },
  {
    id: "job-2", kind: "fdm", state: "success", success: 1, progress_percent: 100,
    progress_stage: "done", total_layers: 3, elapsed_seconds: 0.5,
    output_path: "/data/hsba/jobs/job-2/inline.gcode", error_message: "",
  },
];

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
  // upload pool switched off, which is how a server without [upload] dir answers.
  server.state = { jobs: DEFAULT_JOBS, uploads: null };
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
      // The FDM inline branch: text without a download header.
      res.writeHead(200, { "Content-Type": "text/plain; charset=utf-8" });
      res.end("G21\nG90\nG1 X1.0 Y2.0 E3.0 F1200\n");
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
      assert.doesNotMatch(script, /https?:\/\//);

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

      const response = await fetch(`${base}/api/jobs/fdm`, { method: "POST", body: form });
      assert.equal(response.status, 404); // the fake API has no /api/jobs/fdm route
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
  const documentStub = {
    body,
    createElement: (tag) => makeElement(tag),
    getElementById: (id) => {
      if (!elements.has(id)) {
        elements.set(id, makeElement("div"));
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
      const gcode = "G21\nG90\nG1 X1.0 Y2.0 E3.0 F1200\n";

      jobButton(harness, 1, "Preview").click();
      assert.ok(
        await harness.settle(() => harness.byId("preview").hidden === false),
        `the text result stayed hidden (${harness.byId("preview-meta").textContent})`,
      );
      assert.equal(harness.byId("preview").textContent, gcode);
      // No Content-Disposition on the inline branch, so the panel titles it job id + .gcode.
      assert.match(harness.byId("preview-title").textContent, /job-2\.gcode/);
      assert.match(harness.byId("preview-meta").textContent, new RegExp(`${Buffer.byteLength(gcode, "utf8")} B, 4 lines`));
      assert.doesNotMatch(harness.byId("preview-meta").textContent, /first/);
      assert.deepEqual(harness.logErrors(), []);

      harness.byId("preview-clear").click();
      assert.ok(await harness.settle(() => harness.byId("preview").hidden === true));
      assert.equal(harness.byId("preview-title").textContent, "Result preview");
      assert.equal(harness.byId("preview-meta").textContent, "no result previewed yet");
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
});
