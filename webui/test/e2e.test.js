/**
 * @file e2e.test.js
 * @brief End-to-end test: this console in front of a real HsBaSlicerRestServer process.
 *
 * Unlike server.test.js (which pairs the console with a recording fake REST server), this
 * case talks to the genuine C++ binary, so it covers the parts only the real server can
 * prove: the actual JSON member names, the health/version handshake of a fresh process, and -
 * most importantly - genuine slicing runs. Every pipeline that writes a file is submitted and
 * checked, because a console that only lists endpoints has proved nothing.
 *
 * The model is the same public-domain Stanford Bunny the samples build downloads (see
 * samples/FDM/CMakeLists.txt, which fetches this exact URL into this exact path): the test reuses
 * the sample's model instead of inventing a mesh, so a real 112k-triangle part has to survive the
 * whole path from upload to reported output file.
 *
 * The binary is looked up in $HSBA_REST_SERVER, then in the usual CMake output trees, preferring a
 * Release build since these cases measure a real workload. When it is absent (default desktop
 * build: -DHSBA_BUILD_REST_API=OFF) the case reports as skipped instead of failing, which keeps
 * `node --test` runnable on any checkout.
 *
 * @author HsBa
 */
import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { existsSync, readdirSync } from "node:fs";
import { mkdir, mkdtemp, readFile, rm, stat, writeFile } from "node:fs/promises";
import net from "node:net";
import os from "node:os";
import path from "node:path";
import process from "node:process";
import { after, before, describe, it } from "node:test";
import { fileURLToPath } from "node:url";
import vm from "node:vm";

import { startConsole } from "../server.js";

const REPO_ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..", "..");
const EXE_NAME = process.platform === "win32" ? "HsBaSlicerRestServer.exe" : "HsBaSlicerRestServer";
const SEARCH_ROOTS = ["out/build", "out/install", "build"];
const EXPECTED_KINDS = ["fdm", "sla", "sls", "slm", "lom", "tdp", "waam", "custom", "filetransfer"];

/** @brief Lower is better: a Release tree first, an unknown layout second, a Debug build last. */
function buildRank(candidate) {
  if (/release/i.test(candidate)) {
    return 0;
  }
  return /debug/i.test(candidate) ? 2 : 1;
}

/** @brief Locate the REST server binary without walking unrelated source trees. */
function findRestServer() {
  const configured = (process.env.HSBA_REST_SERVER || "").trim();
  if (configured) {
    return existsSync(configured) ? path.resolve(configured) : null;
  }
  const candidates = new Set();
  for (const root of SEARCH_ROOTS) {
    const start = path.join(REPO_ROOT, root);
    if (!existsSync(start)) {
      continue;
    }
    let entries;
    try {
      entries = readdirSync(start, { recursive: true, withFileTypes: true });
    } catch {
      continue;
    }
    for (const entry of entries) {
      if (entry.isFile() && entry.name === EXE_NAME) {
        candidates.add(path.join(entry.parentPath || entry.path, entry.name));
      }
    }
  }
  // These cases slice a real part, so they belong on the build users ship: a Debug binary runs the
  // same code far slower and its timings say nothing about a real deployment.
  const ordered = [...candidates].sort((a, b) => buildRank(a) - buildRank(b) || a.localeCompare(b));
  return ordered[0] || null;
}

/** @brief Reserve a free loopback port, then release it for the child process. */
async function freePort() {
  const probe = net.createServer();
  await new Promise((resolve, reject) => {
    probe.once("error", reject);
    probe.listen(0, "127.0.0.1", resolve);
  });
  const { port } = probe.address();
  await new Promise((resolve) => probe.close(() => resolve()));
  return port;
}

/**
 * @brief Poll /api/health until the server answers, forwarding its own diagnostics on failure.
 */
async function waitForHealth(base, child, transcript, timeoutMs = 30000) {
  const deadline = Date.now() + timeoutMs;
  for (;;) {
    if (child.exitCode !== null) {
      throw new Error(`REST server exited with code ${child.exitCode}\n${transcript()}`);
    }
    try {
      const response = await fetch(`${base}/api/health`);
      if (response.ok) {
        return;
      }
    } catch {
      // Not listening yet.
    }
    if (Date.now() > deadline) {
      throw new Error(`REST server did not become healthy in ${timeoutMs} ms\n${transcript()}`);
    }
    await new Promise((resolve) => setTimeout(resolve, 150));
  }
}

async function stopChild(child) {
  if (!child || child.exitCode !== null) {
    return;
  }
  const exited = new Promise((resolve) => child.once("exit", resolve));
  child.kill("SIGTERM");
  const timer = setTimeout(() => child.kill("SIGKILL"), 5000);
  await exited;
  clearTimeout(timer);
}

/** @brief Hang guard for one real run; a Release build slices the sample model in a second. */
const PIPELINE_TIMEOUT_MS = 120000;

/** @brief Windows accepts both separators, and JSON payloads travel better with "/". */
function toPosix(filePath) {
  return filePath.split(path.sep).join("/");
}

/** @brief The open model the samples build downloads - same URL, same cache path, on purpose. */
const SAMPLE_MODEL_URL = "https://upload.wikimedia.org/wikipedia/commons/4/43/Stanford_Bunny.stl";
const SAMPLE_MODEL_PATH = path.join(REPO_ROOT, "samples", "FDM", "models", "stanford_bunny.stl");

/** @brief Layer count the runs are sized to, so a real part still slices quickly. */
const LAYERS = 5;

/** @brief The eight bytes every PNG file starts with, from the layer image to the embedded preview. */
const PNG_SIGNATURE = [0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a];

/**
 * @brief Triangle count and z extent of an STL, binary or ASCII.
 *
 * The layer height of a run is derived from the model's own height, which keeps a 112k-triangle
 * part down to a handful of layers instead of hard-coding a number for one geometry.
 */
function stlBounds(buffer) {
  const triangles = buffer.length >= 84 ? buffer.readUInt32LE(80) : 0;
  if (triangles > 0 && buffer.length === 84 + triangles * 50) {
    let minZ = Infinity;
    let maxZ = -Infinity;
    for (let index = 0; index < triangles; index += 1) {
      const base = 84 + index * 50 + 12;
      for (let vertex = 0; vertex < 3; vertex += 1) {
        const z = buffer.readFloatLE(base + vertex * 12 + 8);
        minZ = Math.min(minZ, z);
        maxZ = Math.max(maxZ, z);
      }
    }
    return { triangles, minZ, maxZ, height: maxZ - minZ };
  }
  const text = buffer.toString("latin1");
  const pattern = /vertex\s+([+-]?[\d.eE+-]+)\s+([+-]?[\d.eE+-]+)\s+([+-]?[\d.eE+-]+)/g;
  let vertices = 0;
  let minZ = Infinity;
  let maxZ = -Infinity;
  for (let match = pattern.exec(text); match !== null; match = pattern.exec(text)) {
    vertices += 1;
    minZ = Math.min(minZ, Number(match[3]));
    maxZ = Math.max(maxZ, Number(match[3]));
  }
  return { triangles: Math.round(vertices / 3), minZ, maxZ, height: maxZ - minZ };
}

/**
 * @brief Resolve the sample model, fetching it into the samples' own path when it is absent.
 *
 * @returns {Promise<object|null>} The model and the layer height it slices into, or null after
 *   printing the reason: with neither a local file nor a network there is no genuine slice to
 *   run, and substituting a made-up mesh would only imitate that coverage.
 */
async function resolveSampleModel() {
  const configured = (process.env.HSBA_SAMPLE_MODEL || "").trim();
  const target = configured ? path.resolve(configured) : SAMPLE_MODEL_PATH;
  if (!existsSync(target)) {
    try {
      const response = await fetch(SAMPLE_MODEL_URL, { signal: AbortSignal.timeout(120000) });
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}`);
      }
      await mkdir(path.dirname(target), { recursive: true });
      await writeFile(target, Buffer.from(await response.arrayBuffer()));
    } catch (failure) {
      console.warn(`e2e: no model at ${target} and ${SAMPLE_MODEL_URL} could not be fetched (${failure.message})`);
      return null;
    }
  }
  const buffer = await readFile(target);
  const bounds = stlBounds(buffer);
  if (!(bounds.triangles > 100) || !(bounds.height > 0) || !Number.isFinite(bounds.minZ)) {
    console.warn(`e2e: ${target} is not a usable STL (triangles=${bounds.triangles}, height=${bounds.height})`);
    return null;
  }
  // 2% above the exact quotient: the engine counts layers as 1 + ceil((height - first) / layer), so
  // sitting on an integer boundary would let floating-point noise change the expected count.
  const layer = Number(((bounds.height * 1.02) / LAYERS).toFixed(6));
  return { buffer, path: toPosix(target), name: path.basename(target), bounds, layer };
}

/** @brief Poll a job until the worker reports a terminal state, or the budget runs out. */
async function waitForTerminalJob(base, id, timeoutMs = PIPELINE_TIMEOUT_MS) {
  const deadline = Date.now() + timeoutMs;
  for (;;) {
    const response = await fetch(`${base}/api/jobs/${id}?format=json`);
    assert.equal(response.status, 200, `job ${id} status probe failed`);
    const job = await response.json();
    if (job.state === "success" || job.state === "failed") {
      return job;
    }
    if (Date.now() > deadline) {
      throw new Error(`job ${id} is still "${job.state}" after ${timeoutMs} ms (stage: ${job.progress_stage})`);
    }
    await new Promise((resolve) => setTimeout(resolve, 200));
  }
}

const serverPath = findRestServer();

/**
 * The model every pipeline case slices, resolved once for the file: it is a few megabytes, and the
 * suites share one server and one job queue.
 */
const sample = serverPath ? await resolveSampleModel() : null;

/** @brief Absolute path of a sample Lua script - the same scripts the sample programs hand to the DLL. */
function sampleScript(relative) {
  return toPosix(path.join(REPO_ROOT, "samples", relative));
}

/**
 * @brief Every pipeline that writes a result file, and whatever it needs to be able to.
 *
 * The Lua entries are not decoration: SLS, SLM, LOM and 3DP have no built-in exporter and fail with
 * "requires a Lua export script" unless one is supplied, and Custom will not run without a pipeline
 * script - so the matrix uses the sample scripts, exactly as the samples' own main.cpp does.
 * `inline` marks the kinds whose /result answers with content from memory rather than from disk.
 */
const OUTPUT_PIPELINES = [
  {
    kind: "fdm",
    file: "fdm.gcode",
    inline: true,
    config: (model) => ({
      layer_height: model.layer,
      first_layer_height: model.layer,
      enable_support: 0,
      wall_count: 2,
      top_layer_count: 1,
      bottom_layer_count: 1,
    }),
  },
  {
    kind: "sla",
    file: "sla.zip",
    package: true,
    config: (model) => ({
      layer_height: model.layer,
      first_layer_height: model.layer,
      enable_support: 0,
      image_width: 256,
      image_height: 256,
    }),
  },
  {
    kind: "sls",
    file: "sls.zip",
    package: true,
    lua: { file: "SLS/scripts/my_sls_export.lua", script: "export_lua_script", func: "export_lua_func", entry: "export_sls" },
    config: (model) => ({ layer_height: model.layer, first_layer_height: model.layer }),
  },
  {
    kind: "slm",
    file: "slm.zip",
    package: true,
    lua: { file: "SLM/scripts/my_slm_export.lua", script: "export_lua_script", func: "export_lua_func", entry: "export_slm" },
    config: (model) => ({ layer_height: model.layer, first_layer_height: model.layer }),
  },
  {
    kind: "lom",
    file: "lom.zip",
    package: true,
    lua: { file: "LOM/scripts/my_lom_export.lua", script: "export_lua_script", func: "export_lua_func", entry: "export_lom" },
    config: (model) => ({ layer_height: model.layer, first_layer_height: model.layer }),
  },
  {
    kind: "tdp",
    file: "tdp.zip",
    package: true,
    lua: { file: "TDP/scripts/my_tdp_export.lua", script: "export_lua_script", func: "export_lua_func", entry: "export_tdp" },
    config: (model) => ({ layer_height: model.layer, first_layer_height: model.layer }),
  },
  {
    kind: "waam",
    file: "waam.txt",
    lua: { file: "WAAM/scripts/my_waam_path.lua", script: "path_lua_script", func: "path_lua_func", entry: "export_waam" },
    config: (model) => ({ layer_height: model.layer, first_layer_height: model.layer }),
  },
  {
    kind: "custom",
    file: "custom.gcode",
    inline: true,
    // A Lua pipeline reads its machine settings from an injected snippet rather than from JSON
    // fields, so the layer height goes where samples/Custom/main.cpp puts it.
    lua: { file: "Custom/scripts/my_fdm_pipeline.lua", script: "pipeline_lua_script", func: "entry_func", entry: "run_pipeline" },
    source: (model) => `machine = { layer_height = ${model.layer}, first_layer_height = ${model.layer}, wall_count = 2 }\n`,
    config: () => ({}),
    // The script counts layers itself, so only a lower bound is claimed here.
    minLayers: 2,
  },
];

/**
 * Fetch and evaluate the *served* preview helpers, exactly as the browser does.
 *
 * Running them against output from a real slice is what proves the console's Preview button works:
 * a helper tested only on fake bodies could still be wrong about genuine G-code.
 */
async function loadHelpers(base) {
  const response = await fetch(`${base}/httphelpers.js`);
  assert.equal(response.status, 200, "the console does not serve the preview helpers");
  const sandbox = { TextEncoder };
  vm.createContext(sandbox);
  vm.runInContext(await response.text(), sandbox, { filename: "httphelpers.js" });
  assert.ok(sandbox.HsBaHttp, "httphelpers.js did not publish globalThis.HsBaHttp");
  return sandbox.HsBaHttp;
}

/**
 * @brief Bring a value the helpers built inside their own realm back here, ready for a strict compare.
 *
 * `node:assert/strict` deep-equality also checks the prototype, and an array or object made in a
 * `node:vm` context carries that context's `Array` or `Object` - including anything `filter` or `map`
 * returns from it - so such a value is never reference-equal to a literal written in this file.
 */
function local(value) {
  return JSON.parse(JSON.stringify(value));
}

describe("end to end with a real HsBaSlicerRestServer", () => {
  if (!serverPath) {
    it("is skipped without a restapi build", { skip: "no HsBaSlicerRestServer binary (build with HSBA_BUILD_REST_API=ON or set HSBA_REST_SERVER)" }, () => {});
    return;
  }

  let tempDir;
  let child;
  let consoleServer;
  let restBase;
  let output = "";

  before(async () => {
    tempDir = await mkdtemp(path.join(os.tmpdir(), "hsba-webui-e2e-"));
    const port = await freePort();
    // Multipart uploads stay enabled: the pipeline cases submit config+model the same way
    // the console's upload form does, so the pool also has to hand the file to the worker.
    const uploadsDir = toPosix(path.join(tempDir, "uploads"));
    const ini = [
      "[server]",
      "host = 127.0.0.1",
      `port = ${port}`,
      "default_format = json",
      "max_concurrent_jobs = 1",
      "[upload]",
      `dir = ${uploadsDir}`,
      "max_files = 4",
      "max_file_mb = 32",
      "[paramstore]",
      "backend =",
      "",
    ].join("\n");
    const iniPath = path.join(tempDir, "restapi.ini");
    await writeFile(iniPath, ini, "utf8");

    child = spawn(serverPath, ["--config", iniPath], { cwd: tempDir, stdio: ["ignore", "pipe", "pipe"] });
    const capture = (chunk) => {
      output += chunk.toString();
    };
    child.stdout.on("data", capture);
    child.stderr.on("data", capture);

    restBase = `http://127.0.0.1:${port}`;
    await waitForHealth(restBase, child, () => output);
    consoleServer = await startConsole({ restUrl: restBase, host: "127.0.0.1", port: 0 });
  });

  after(async () => {
    if (consoleServer) {
      consoleServer.closeAllConnections?.();
      await new Promise((resolve) => consoleServer.close(() => resolve()));
    }
    await stopChild(child);
    if (tempDir) {
      if (process.env.HSBA_E2E_KEEP_OUTPUT) {
        // Manual inspection knob: keep the workspace so the slices each pipeline reported
        // can be opened outside the test run. Never enabled in CI.
        console.info(`e2e workspace kept: ${tempDir}`);
      } else {
        await rm(tempDir, { recursive: true, force: true });
      }
    }
  });

  /** Base URL of the console under test (assigned in `before`). */
  const consoleUrlOf = () => `http://127.0.0.1:${consoleServer.address().port}`;

  it("serves the console page next to the live API", async () => {
    const page = await fetch(`${consoleUrlOf()}/`);
    assert.equal(page.status, 200);
    assert.match(page.headers.get("content-type"), /^text\/html/);
    assert.match(await page.text(), /HsBaSlicer REST Console/);

    // The whole bundle has to be reachable from the page's own origin, including the helpers
    // behind the Preview button; a missing asset would only show up as a browser console error.
    for (const asset of ["/app.css", "/httphelpers.js", "/model.js", "/view.js", "/app.js", "/favicon.svg"]) {
      const response = await fetch(`${consoleUrlOf()}${asset}`);
      assert.equal(response.status, 200, asset);
      assert.ok(response.headers.get("content-type"), `${asset} has no media type`);
    }
  });

  it("proxies health and version of the real server", async () => {
    const health = await fetch(`${consoleUrlOf()}/api/health`);
    assert.equal(health.status, 200);
    const healthBody = await health.json();
    assert.equal(healthBody.status, "ok");
    assert.ok(Number(healthBody.uptime_seconds) >= 0);

    const version = await fetch(`${consoleUrlOf()}/api/version`);
    assert.equal(version.status, 200);
    const versionBody = await version.json();
    assert.equal(typeof versionBody.version, "string");
    assert.ok(Object.keys(versionBody).length > 0, JSON.stringify(versionBody));
  });

  it("lists every pipeline kind the client offers", async () => {
    const listing = await (await fetch(`${consoleUrlOf()}/api/pipelines`)).json();
    const names = (listing.kinds || []).map((entry) => entry.name);
    assert.deepEqual(names, EXPECTED_KINDS);
  });

  it("returns a real default config that the page can load", async () => {
    const response = await fetch(`${consoleUrlOf()}/api/config/default/fdm?format=json`);
    assert.equal(response.status, 200);
    assert.match(response.headers.get("content-type"), /json/);
    const config = JSON.parse(await response.text());
    assert.equal(Object.keys(config).length > 0, true);
  });

  it("reports an empty job and upload list before anything is submitted", async () => {
    const jobs = await (await fetch(`${consoleUrlOf()}/api/jobs?limit=5`)).json();
    assert.equal(jobs.count, 0);
    assert.deepEqual(jobs.jobs, []);

    // [upload] dir points into the temp tree, so the pool answers instead of 503.
    const uploads = await fetch(`${consoleUrlOf()}/api/uploads`);
    assert.equal(uploads.status, 200);
    const payload = await uploads.json();
    assert.equal(payload.count, 0);
    assert.deepEqual(payload.uploads, []);
  });

  it("rejects an unknown pipeline kind with the server's own error body", async () => {
    const response = await fetch(`${consoleUrlOf()}/api/config/default/no-such-kind?format=json`);
    assert.equal(response.status, 404);
    const payload = await response.json();
    assert.equal(payload.code, 404);
    assert.match(payload.message, /unknown pipeline kind/);
  });

  it("proxies a layer geometry request to the real server", async () => {
    // The console spells no path that server.js does not already forward, so what only the genuine
    // binary can prove is that a three-segment job sub-resource reaches it intact and the server's
    // own error body comes back unchanged. Nothing is sliced here on purpose: geometry is only
    // readable once a job is terminal, and the snapshot pool keeps just the last few jobs.
    const response = await fetch(`${consoleUrlOf()}/api/jobs/hsba-no-such-job/geometry`);
    assert.equal(response.status, 404);
    assert.match(response.headers.get("content-type"), /json/);
    const payload = await response.json();
    assert.equal(payload.code, 404);
    assert.match(payload.message, /not found/);
    assert.equal(payload.detail, "hsba-no-such-job");

    // The reserved "path" segment has to survive the proxy too, and must not be swallowed by the
    // layer-number route, which would answer 400 about a layer index instead of 404 about a job.
    const pathProbe = await fetch(`${consoleUrlOf()}/api/jobs/hsba-no-such-job/geometry/path`);
    assert.equal(pathProbe.status, 404);
    const pathBody = await pathProbe.json();
    assert.match(pathBody.message, /not found/);
    assert.equal(pathBody.detail, "hsba-no-such-job");
  });

  it("rejects a model part for a pipeline kind that accepts none", async () => {
    const form = new FormData();
    form.set("config", new Blob(["{}"], { type: "application/json" }), "config.json");
    // The part never reaches a parser: filetransfer refuses it by kind, so these bytes are noise.
    form.set("model", new Blob(["not a model"], { type: "model/stl" }), "refused.stl");
    const response = await fetch(`${consoleUrlOf()}/api/jobs/filetransfer`, { method: "POST", body: form });
    assert.equal(response.status, 400);
    const payload = await response.json();
    assert.match(payload.message, /accepts no model file/);
  });

  describe("slices the model the samples download", () => {
    if (!sample) {
      it("is skipped without the sample model", {
        skip: `neither ${SAMPLE_MODEL_PATH} nor a download of ${SAMPLE_MODEL_URL} is available (set HSBA_SAMPLE_MODEL to a usable STL)`,
      }, () => {});
      return;
    }

    /** @brief FDM settings sized so that a 112k-triangle part still slices into a handful of layers. */
    const fdmConfig = async (outputPath) => {
      const response = await fetch(`${consoleUrlOf()}/api/config/default/fdm?format=json`);
      assert.equal(response.status, 200);
      const config = JSON.parse(await response.text());
      config.layer_height = sample.layer;
      config.first_layer_height = sample.layer;
      config.enable_support = 0;
      config.wall_count = 2;
      config.top_layer_count = 1;
      config.bottom_layer_count = 1;
      config.output_path = outputPath;
      return config;
    };

    it("runs a real FDM slicing job from an uploaded model and reports its G-code file", async () => {
      const outputPath = toPosix(path.join(tempDir, "out", "bunny-upload.gcode"));
      const form = new FormData();
      form.set(
        "config",
        new Blob([JSON.stringify(await fdmConfig(outputPath))], { type: "application/json" }),
        "config.json",
      );
      form.set("model", new Blob([sample.buffer], { type: "model/stl" }), sample.name);

      // Submitted through the console, so the same-origin proxy has to forward the
      // multipart body and its boundary unchanged.
      const submit = await fetch(`${consoleUrlOf()}/api/jobs/fdm`, { method: "POST", body: form });
      const submitBody = await submit.text();
      assert.equal(submit.status, 202, submitBody);
      const accepted = JSON.parse(submitBody);
      assert.equal(accepted.state, "queued");
      assert.ok(accepted.id, `no job id in ${submitBody}`);

      const job = await waitForTerminalJob(consoleUrlOf(), accepted.id);
      assert.equal(job.state, "success", job.error_message);
      // The C ABI reports the flag as an int, so the JSON member is 1, not true.
      assert.equal(job.success, 1);
      assert.equal(job.total_layers, LAYERS, `unexpected layer count: ${JSON.stringify(job)}`);
      assert.equal(job.output_path, outputPath);

      // The pipeline wrote a real file to the path it reports: that is what the console's
      // Output column shows and copies.
      const gcode = await readFile(outputPath, "utf8");
      assert.ok(gcode.length > 5000, `G-code is only ${gcode.length} bytes for a real part`);
      assert.match(gcode, /G1 X/, "no extrusion moves in the G-code");

      const result = await fetch(`${consoleUrlOf()}/api/jobs/${accepted.id}/result`);
      assert.equal(result.status, 200);
      assert.match(result.headers.get("content-type"), /text\/plain/);
      const resultText = await result.text();
      assert.equal(resultText, gcode);

      // What the Preview button does with this very response: the served helpers must class real
      // G-code as text, keep the whole file's size, and never cut a line-oriented body arbitrarily.
      const helpers = await loadHelpers(consoleUrlOf());
      assert.ok(helpers.isPreviewable(result.headers.get("content-type")));
      const view = helpers.previewText(resultText);
      assert.equal(view.bytes, Buffer.byteLength(gcode, "utf8"));
      assert.equal(view.truncated, view.bytes > helpers.PREVIEW_LIMIT_BYTES);
      assert.ok(view.shownBytes <= helpers.PREVIEW_LIMIT_BYTES);
      assert.ok(gcode.startsWith(view.text), "the preview must be a prefix of the result");
      assert.ok(view.lines >= 2, JSON.stringify({ lines: view.lines, bytes: view.bytes }));

      // Inline results carry no Content-Disposition, so the save name falls back to job id + .gcode.
      assert.equal(result.headers.get("content-disposition"), null);
      assert.equal(helpers.resultFilename("", accepted.id, "fdm"), `${accepted.id}.gcode`);

      // The pool kept the stored model, tied it to the finished job, and did not corrupt the
      // several megabytes that travelled through the proxy.
      const uploads = await (await fetch(`${consoleUrlOf()}/api/uploads`)).json();
      assert.equal(uploads.count, 1, JSON.stringify(uploads));
      assert.equal(uploads.uploads[0].job_id, accepted.id);
      assert.ok(existsSync(uploads.uploads[0].path), uploads.uploads[0].path);
      const stored = await readFile(uploads.uploads[0].path);
      assert.ok(stored.equals(sample.buffer), `the pool holds ${stored.length} bytes, the sample has ${sample.buffer.length}`);
    });

    it("captures and serves the real spiral path", async () => {
      // A spiral run is the only thing that produces a continuous polyline, so this is the case that
      // proves the whole chain: the engine stores the helix it extruded, the snapshot keeps it, the
      // endpoint serves it, and the page's own projection math runs on genuine millimetre data.
      const outputPath = toPosix(path.join(tempDir, "out", "bunny-spiral.gcode"));
      // The default config carries an empty model path of its own, so the model has to win the merge.
      const config = {
        ...(await fdmConfig(outputPath)),
        model_name: "stanford_bunny",
        model_path: sample.path,
        spiral_mode: 1,
      };
      const submit = await fetch(`${consoleUrlOf()}/api/jobs/fdm?geometry=1`, {
        method: "POST",
        headers: { "content-type": "application/json" },
        body: JSON.stringify(config),
      });
      const submitBody = await submit.text();
      assert.equal(submit.status, 202, submitBody);
      const accepted = JSON.parse(submitBody);

      const job = await waitForTerminalJob(consoleUrlOf(), accepted.id);
      assert.equal(job.state, "success", job.error_message);
      assert.equal(job.geometry_available, 1, JSON.stringify(job));

      const summary = await (await fetch(`${consoleUrlOf()}/api/jobs/${accepted.id}/geometry?format=json`)).json();
      assert.equal(summary.layer_count, LAYERS, JSON.stringify(summary));
      // The overview is what gates the Path button, so it has to tell the truth about this run.
      assert.equal(summary.has_path, 1, JSON.stringify(summary));

      const body = await (await fetch(`${consoleUrlOf()}/api/jobs/${accepted.id}/geometry/path?format=json`)).json();
      assert.equal(body.id, accepted.id);
      const points = Number(body.point_count);
      // Every layer contributes the ring it wound around, so a polyline shorter than two points per
      // layer could not have come from this job's own layers.
      assert.ok(points >= 2 * LAYERS, `only ${points} path points for ${LAYERS} layers`);
      assert.equal(body.xyz.length, 3 * points, "xyz is not a flat triple per point");
      for (let at = 1; at < points; at += 1) {
        assert.ok(
          body.xyz[at * 3 + 2] >= body.xyz[(at - 1) * 3 + 2] - 1e-9,
          `the path sank at point ${at}: ${body.xyz[(at - 1) * 3 + 2]} -> ${body.xyz[at * 3 + 2]}`,
        );
      }
      assert.ok(Number(body.z_range[1]) > Number(body.z_range[0]), JSON.stringify(body.z_range));

      // The page draws what the served numbers project to - the same helpers the browser loaded.
      const helpers = await loadHelpers(consoleUrlOf());
      const drawn = helpers.projectedPathPolyline(body.xyz, helpers.GEOMETRY_STACK_ANGLES);
      assert.equal(drawn.points.length, points);
      assert.ok(drawn.d.startsWith("M "), drawn.d.slice(0, 40));
      assert.ok(!drawn.d.includes("NaN") && !drawn.d.includes("-0 "), "a projected coordinate is junk");
      const box = helpers.projectedViewBox(drawn.points);
      assert.ok(box.w > 0 && box.h > 0, JSON.stringify(box));

      // And the layer route still counts "path" as a reserved word rather than as a layer index.
      const beyond = await fetch(`${consoleUrlOf()}/api/jobs/${accepted.id}/geometry/${summary.layer_count}`);
      assert.equal(beyond.status, 404);
      const beyondBody = await beyond.json();
      assert.match(beyondBody.message, /layer index out of range/);
      assert.equal(beyondBody.detail, `layer_count=${LAYERS}`);
    });

    it("lists and serves the images inside a real result package", async () => {
      // The package endpoints have one job: hand a browser the file that is inside the archive the
      // pipeline wrote. Only the genuine exporter can prove the entry names, the types derived from
      // them and the bytes behind them, so this slices an SLA run rather than inventing a zip.
      const outputPath = toPosix(path.join(tempDir, "out", "bunny-package.zip"));
      await mkdir(path.dirname(outputPath), { recursive: true });
      const config = {
        model_name: "stanford_bunny",
        model_path: sample.path,
        output_path: outputPath,
        layer_height: sample.layer,
        first_layer_height: sample.layer,
        enable_support: 0,
        image_width: 128,
        image_height: 128,
      };
      const submit = await fetch(`${consoleUrlOf()}/api/jobs/sla`, {
        method: "POST",
        headers: { "content-type": "application/json" },
        body: JSON.stringify(config),
      });
      const submitBody = await submit.text();
      assert.equal(submit.status, 202, submitBody);
      const accepted = JSON.parse(submitBody);
      const job = await waitForTerminalJob(consoleUrlOf(), accepted.id);
      assert.equal(job.state, "success", job.error_message);

      const listing = await (await fetch(`${consoleUrlOf()}/api/jobs/${accepted.id}/package?format=json`)).json();
      assert.equal(listing.id, accepted.id);
      // The archive is named as one file, never as the directory the server slices into.
      assert.equal(listing.name, "bunny-package.zip");
      assert.equal(Number(listing.entry_count), listing.entries.length, JSON.stringify(listing.entry_count));

      const helpers = await loadHelpers(consoleUrlOf());
      // The exporter writes layers/layer_N.png, floor/floor_raft.png and a config.json; the console
      // has to find the pictures by the type it derives from those names, in layer order.
      const images = helpers.packageImages(listing.entries);
      const layerImages = images.filter((entry) => /^layers\/layer_\d+\.png$/.test(entry.path));
      // A layer whose render came out empty never reaches the archive, so the count is a bound, not a
      // promise; what the browser depends on is the order the images are handed to it.
      assert.ok(layerImages.length >= 2, `only ${layerImages.length} layer images of ${LAYERS}`);
      assert.ok(layerImages.length <= LAYERS, JSON.stringify(images.map((entry) => entry.path)));
      const numbers = layerImages.map((entry) => Number(/layer_(\d+)\./.exec(entry.path)[1]));
      assert.deepEqual(
        local(numbers),
        local([...numbers].sort((a, b) => a - b)),
        "the images reached the layer browser out of order",
      );
      assert.equal(numbers[0], 0, `the lowest layer is ${numbers[0]}`);
      // An image whose name carries no layer number is kept behind every numbered one.
      assert.match(images[images.length - 1].path, /^floor\//, JSON.stringify(images.map((entry) => entry.path)));
      assert.equal(
        listing.entries.find((entry) => entry.path === "config.json").media_type,
        "application/json",
        JSON.stringify(listing.entries),
      );

      const first = layerImages[0];
      assert.equal(first.path, "layers/layer_0.png");
      const response = await fetch(`${consoleUrlOf()}${helpers.packageEntryUrl(accepted.id, first.path)}`);
      assert.equal(response.status, 200, first.path);
      assert.equal(response.headers.get("content-type"), "image/png");
      // A picture the console hands to an <img> keeps the header that stops it being reinterpreted.
      assert.equal(response.headers.get("X-Content-Type-Options"), "nosniff");
      const bytes = Buffer.from(await response.arrayBuffer());
      assert.deepEqual([...bytes.subarray(0, 8)], PNG_SIGNATURE, `${first.path} is not a PNG`);
      // IHDR width and height, read from the real file the exporter rendered at the requested size.
      assert.equal(bytes.readUInt32BE(16), 128);
      assert.equal(bytes.readUInt32BE(20), 128);
      assert.equal(Number(first.uncompressed_size), bytes.length, `${first.path} is not the size it listed`);

      // One entry the archive does not hold is a 404 about that entry, not a broken image.
      const missing = await fetch(`${consoleUrlOf()}/api/jobs/${accepted.id}/package/layers/no_such_layer.png`);
      assert.equal(missing.status, 404);
      const missingBody = await missing.json();
      assert.match(missingBody.message, /package entry not found/);
      assert.equal(missingBody.detail, "layers/no_such_layer.png");

      // A job that never produced an archive is answered about the job, not about the entry.
      const ghost = await fetch(`${consoleUrlOf()}/api/jobs/hsba-no-such-job/package?format=json`);
      assert.equal(ghost.status, 404);
      assert.match((await ghost.json()).message, /not found/);
    });

    it("embeds the model previews an FDM run asked for into its own G-code", async () => {
      // This is the whole stage-2 chain on real data: the engine projects the sliced model, encodes a
      // PNG, writes it into the comment header, and the console's own parser reads it back out of the
      // served result - the same bytes the browser would put in an <img>.
      const outputPath = toPosix(path.join(tempDir, "out", "bunny-thumbs.gcode"));
      const config = {
        ...(await fdmConfig(outputPath)),
        model_name: "stanford_bunny",
        model_path: sample.path,
        gcode_thumbnails: 1,
        gcode_thumbnail_size: 64,
      };
      const submit = await fetch(`${consoleUrlOf()}/api/jobs/fdm`, {
        method: "POST",
        headers: { "content-type": "application/json" },
        body: JSON.stringify(config),
      });
      const submitBody = await submit.text();
      assert.equal(submit.status, 202, submitBody);
      const accepted = JSON.parse(submitBody);
      const job = await waitForTerminalJob(consoleUrlOf(), accepted.id);
      assert.equal(job.state, "success", job.error_message);

      const text = await (await fetch(`${consoleUrlOf()}/api/jobs/${accepted.id}/result`)).text();
      // The blocks head the file, which is what lets a truncated preview still carry the pictures.
      assert.match(text, /^; thumbnail begin name=model_top /m);
      assert.ok(text.startsWith("; thumbnail begin"), "the previews are not at the head of the file");
      assert.match(text, /; Generated by HsBaSlicer/);
      assert.match(text, /G1 X/, "the run wrote no extrusion moves");

      const helpers = await loadHelpers(consoleUrlOf());
      const blocks = helpers.parseGcodeThumbnails(text);
      const drawn = blocks.filter((block) => !block.skipped);
      // A 112k-triangle part is under the projection budget, so both views have to be pictures.
      assert.deepEqual(
        local(drawn.map((block) => block.name)),
        ["model_top", "model_side"],
        JSON.stringify(blocks.map((block) => ({ name: block.name, reason: block.reason }))),
      );
      for (const block of drawn) {
        const bytes = Buffer.from(block.data, "base64");
        assert.deepEqual([...bytes.subarray(0, 8)], PNG_SIGNATURE, `${block.name} is not a PNG`);
        assert.equal(block.width, 64, block.name);
        assert.equal(block.height, 64, block.name);
        assert.equal(bytes.readUInt32BE(16), block.width, `${block.name} is not the width its header claims`);
        assert.equal(bytes.readUInt32BE(20), block.height, `${block.name} is not the height its header claims`);
        // The length the begin line states is the length the reader rejoined, chunk line or no chunk line.
        const declared = new RegExp(`; thumbnail begin name=${block.name} [^\n]*b64len=(\\d+)`).exec(text);
        assert.ok(declared, `no begin line for ${block.name}`);
        assert.equal(Number(declared[1]), block.data.length, `${block.name} misreported its Base64 length`);
        // A silhouette of a solid part is never empty, and a picture of it is never one blank plate.
        assert.ok(bytes.length > 100, `${block.name} is ${bytes.length} bytes of nothing`);
        assert.ok(helpers.thumbnailDataUrl(block).startsWith("data:image/png;base64,"), block.name);
      }

      // The same pipeline without the option writes a file whose head is untouched, so the flag adds
      // comments and changes nothing else about the machine code.
      const plainPath = toPosix(path.join(tempDir, "out", "bunny-nothumbs.gcode"));
      const plain = await fetch(`${consoleUrlOf()}/api/jobs/fdm`, {
        method: "POST",
        headers: { "content-type": "application/json" },
        body: JSON.stringify({ ...config, gcode_thumbnails: 0, output_path: plainPath }),
      });
      const plainBody = await plain.text();
      assert.equal(plain.status, 202, plainBody);
      const plainJob = await waitForTerminalJob(consoleUrlOf(), JSON.parse(plainBody).id);
      assert.equal(plainJob.state, "success", plainJob.error_message);
      const plainText = await readFile(plainPath, "utf8");
      assert.doesNotMatch(plainText, /^; thumbnail/m);
      assert.equal(helpers.parseGcodeThumbnails(plainText).length, 0);
      // The banner is still the first line, byte for byte as the run without the option writes it.
      assert.ok(plainText.startsWith("; Generated by HsBaSlicer\n"), "the header moved with the flag off");
    });

    for (const pipeline of OUTPUT_PIPELINES) {
      it(`${pipeline.kind} reports and serves the file it wrote`, async () => {
        const outputPath = toPosix(path.join(tempDir, "matrix", pipeline.file));
        await mkdir(path.dirname(outputPath), { recursive: true });
        const config = {
          model_name: "stanford_bunny",
          model_path: sample.path,
          output_path: outputPath,
          ...pipeline.config(sample),
        };
        if (pipeline.lua) {
          config[pipeline.lua.script] = sampleScript(pipeline.lua.file);
          config[pipeline.lua.func] = pipeline.lua.entry;
        }
        if (pipeline.source) {
          config.pipeline_lua_source = pipeline.source(sample);
        }

        const submit = await fetch(`${consoleUrlOf()}/api/jobs/${pipeline.kind}`, {
          method: "POST",
          headers: { "content-type": "application/json" },
          body: JSON.stringify(config),
        });
        const submitBody = await submit.text();
        assert.equal(submit.status, 202, `${pipeline.kind}: ${submitBody}`);
        const accepted = JSON.parse(submitBody);

        const job = await waitForTerminalJob(consoleUrlOf(), accepted.id);
        assert.equal(job.state, "success", `${pipeline.kind}: ${job.error_message}`);
        // The path the caller asked for is the path the page shows: not renamed, not defaulted
        // to the pipeline's own working directory, not dropped on the floor.
        assert.equal(job.output_path, outputPath, pipeline.kind);
        const written = await stat(outputPath);
        assert.ok(written.size > 0, `${pipeline.kind} reported ${outputPath}, which is empty`);
        if (pipeline.minLayers) {
          assert.ok(job.total_layers >= pipeline.minLayers, `${pipeline.kind} sliced into ${job.total_layers} layers`);
        } else {
          assert.equal(job.total_layers, LAYERS, `${pipeline.kind} sliced into ${job.total_layers} layers`);
        }

        // The listing the console renders carries the very same path, not just the detail view.
        const listing = await (await fetch(`${consoleUrlOf()}/api/jobs?limit=50&format=json`)).json();
        const listed = listing.jobs.find((entry) => entry.id === accepted.id);
        assert.ok(listed, `${pipeline.kind} is missing from the job listing`);
        assert.equal(listed.output_path, outputPath);

        // And the download endpoint hands back exactly that file.
        const result = await fetch(`${consoleUrlOf()}/api/jobs/${accepted.id}/result`);
        assert.equal(result.status, 200, pipeline.kind);
        const served = Buffer.from(await result.arrayBuffer());
        const helpers = await loadHelpers(consoleUrlOf());
        if (pipeline.inline) {
          assert.match(result.headers.get("content-type"), /text\/plain/, pipeline.kind);
          assert.ok(served.length > 0, `${pipeline.kind} answered inline with an empty body`);
        } else {
          assert.match(result.headers.get("content-type"), /octet-stream/, pipeline.kind);
          assert.equal(served.length, written.size, `${pipeline.kind} served a body that is not its file`);
          if (pipeline.package) {
            assert.equal(served.subarray(0, 2).toString("latin1"), "PK", `${pipeline.kind} did not write a package`);
          }
          const disposition = result.headers.get("content-disposition") || "";
          assert.match(disposition, /attachment/, disposition);
          assert.doesNotMatch(disposition, /[\\/]/, "the download name must not leak the server directory");
          assert.equal(helpers.filenameFromDisposition(disposition, ""), path.basename(outputPath));
        }
      });
    }

    it("a pipeline that cannot export reports no path instead of a stale one", async () => {
      // SLS has no built-in exporter. Refusing to run is only half the contract: it must also keep
      // quiet about a file it never wrote, or the console would offer to download nothing.
      const outputPath = toPosix(path.join(tempDir, "matrix", "sls-without-export.zip"));
      const config = {
        model_name: "stanford_bunny",
        model_path: sample.path,
        output_path: outputPath,
        layer_height: sample.layer,
        first_layer_height: sample.layer,
      };
      const submit = await fetch(`${consoleUrlOf()}/api/jobs/sls`, {
        method: "POST",
        headers: { "content-type": "application/json" },
        body: JSON.stringify(config),
      });
      const submitBody = await submit.text();
      assert.equal(submit.status, 202, submitBody);
      const accepted = JSON.parse(submitBody);
      const job = await waitForTerminalJob(consoleUrlOf(), accepted.id);
      assert.equal(job.state, "failed", JSON.stringify(job));
      assert.match(job.error_message, /export_lua_script/, job.error_message);
      assert.equal(job.output_path, "");
      assert.ok(!existsSync(outputPath), `${outputPath} exists although the run failed`);

      const result = await fetch(`${consoleUrlOf()}/api/jobs/${accepted.id}/result`);
      assert.equal(result.status, 409);
    });
  });
});
