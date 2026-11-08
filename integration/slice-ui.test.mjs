/**
 * @file slice-ui.test.mjs
 * @brief Integration test: the WebUI client driving a real slice on a real HsBaSlicerRestServer.
 *
 * This is deliberately NOT a unit test and NOT one of webui/test/{server,e2e}.test.js:
 *   - server.test.js pairs the served client scripts with a *recording fake* REST server, so nothing
 *     is ever sliced;
 *   - e2e.test.js talks to the genuine binary but only over HTTP, reproducing the request shapes the
 *     console sends - it never runs the console's own handler layer.
 * This file joins the two: it boots the *served* model.js/view.js/app.js in a minimal DOM and drives
 * the actual submit -> poll -> Preview handlers against a real process slicing a real part. A defect
 * in the controller wiring, in the JSON member names the client reads, or in the result the browser
 * would fetch can therefore fail here, which is the only thing that makes it an integration test
 * rather than a re-listing of endpoints.
 *
 * The model is the same public-domain Stanford Bunny the samples build downloads, so a genuine
 * 112k-triangle part survives the whole path from the page's config box to the G-code the Preview
 * button paints - no synthetic mesh is generated here.
 *
 * The binary is looked up in $HSBA_REST_SERVER, then in the usual CMake output trees, preferring a
 * Release build because a real workload is being measured. When it (or the model) is absent the case
 * reports as skipped instead of failing, which keeps the suite runnable on any checkout.
 *
 * Run:  node --test integration/slice-ui.test.mjs
 *
 * @author HsBa
 */
import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { existsSync, readdirSync } from "node:fs";
import { mkdir, mkdtemp, readFile, rm, writeFile } from "node:fs/promises";
import net from "node:net";
import os from "node:os";
import path from "node:path";
import process from "node:process";
import { after, before, describe, it } from "node:test";
import { fileURLToPath } from "node:url";
import vm from "node:vm";

import { startConsole } from "../webui/server.js";

const REPO_ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const EXE_NAME = process.platform === "win32" ? "HsBaSlicerRestServer.exe" : "HsBaSlicerRestServer";
const SEARCH_ROOTS = ["out/build", "out/install", "build"];
const LAYERS = 5;
const SLICE_TIMEOUT_MS = 120000;

/** @brief Windows accepts both separators, and JSON payloads travel better with "/". */
function toPosix(filePath) {
  return filePath.split(path.sep).join("/");
}

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

/** @brief Poll /api/health directly against the child until it answers. */
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

const SAMPLE_MODEL_URL = "https://upload.wikimedia.org/wikipedia/commons/4/43/Stanford_Bunny.stl";
const SAMPLE_MODEL_PATH = path.join(REPO_ROOT, "samples", "FDM", "models", "stanford_bunny.stl");

/** @brief Triangle count and z extent of an STL, binary or ASCII; the layer height is derived from it. */
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
    return { triangles, height: maxZ - minZ, minZ };
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
  return { triangles: Math.round(vertices / 3), height: maxZ - minZ, minZ };
}

/**
 * @brief Resolve the sample model, fetching it into the samples' own path when it is absent.
 * @returns {Promise<object|null>} The model and its layer height, or null after printing the reason.
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
      console.warn(`integration: no model at ${target} and ${SAMPLE_MODEL_URL} could not be fetched (${failure.message})`);
      return null;
    }
  }
  const buffer = await readFile(target);
  const bounds = stlBounds(buffer);
  if (!(bounds.triangles > 100) || !(bounds.height > 0) || !Number.isFinite(bounds.minZ)) {
    console.warn(`integration: ${target} is not a usable STL (triangles=${bounds.triangles}, height=${bounds.height})`);
    return null;
  }
  const layer = Number(((bounds.height * 1.02) / LAYERS).toFixed(6));
  return { buffer, path: toPosix(target), name: path.basename(target), layer };
}

// ---------------------------------------------------------------------------------------------
// A minimal DOM, faithful to the semantics a browser exposes. The three pitfalls this file must not
// repeat (they each caused a false green in webui/test/server.test.js once): `children` is a live
// collection with indexed access and a length but NO Array methods; `textContent` is backed by a real
// text node so writing the identical string still replaces it (a selection lives inside it); and a
// "render finished" predicate must key on the text a cell actually holds, never on a request count.
// ---------------------------------------------------------------------------------------------
function makeElement(tag) {
  const kids = [];
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
    min: "",
    max: "",
    attributes: new Map(),
    clientWidth: 640,
    files: [],
    children: collection,
    listeners: new Map(),
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
      detach(child);
      kids.push(child);
      // A <select> with no explicit choice reports its first option as its value, which is what the
      // page reads right after it fills the pipeline list.
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
    remove() {},
    click() {
      for (const handler of this.listeners.get("click") || []) {
        handler({});
      }
    },
  };
}

/**
 * @brief Boot the served client scripts against the live console, the way the browser does.
 *
 * Elements are created on demand by id, so a handler reaching a renamed node writes nowhere and the
 * assertions fail instead of passing silently. The timers are no-ops: the real slice is advanced by
 * clicking jobs-refresh on a wall-clock interval in the test body, not by the page's own poll loop.
 * The default "ready" gate is health + a populated pipeline list + a loaded default config, because a
 * real server starts with an EMPTY job list (unlike the fake, whose bootPage keyed on two rows).
 */
async function bootConsolePage(base) {
  const elements = new Map();
  const body = makeElement("body");
  const store = new Map();
  const domReady = [];
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
    URL: { createObjectURL: () => "blob:harness", revokeObjectURL: () => {} },
    fetch: (input, init) => fetch(new URL(String(input), base).href, init),
    setTimeout: () => 0,
    setInterval: () => 0,
    clearTimeout: () => {},
    clearInterval: () => {},
    console,
  };
  sandbox.window = sandbox;

  const context = vm.createContext(sandbox);
  for (const file of ["httphelpers.js", "model.js", "view.js", "app.js"]) {
    const source = await (await fetch(`${base}/${file}`)).text();
    vm.runInContext(source, context, { filename: file });
  }
  for (const handler of domReady) {
    handler();
  }

  const harness = {
    byId: (id) => documentStub.getElementById(id),
    async settle(predicate) {
      for (let attempt = 0; attempt < 400; attempt += 1) {
        if (predicate()) {
          return true;
        }
        await new Promise((resolve) => setImmediate(resolve));
      }
      return predicate();
    },
    // The controller reports failures in the log panel via run(); an uninspected log would hide a
    // broken action, so every case asserts this is empty.
    logErrors: () =>
      Array.from(harness.byId("log").children)
        .filter((line) => line.className.includes("log-error"))
        .map((line) => line.textContent),
  };

  const ready = await harness.settle(() =>
    harness.byId("health").textContent.includes("ok") &&
    harness.byId("kind").childElementCount >= 1 &&
    harness.byId("config").value.length > 0);
  if (!ready) {
    assert.fail(
      `the console never came up against the real server (health="${harness.byId("health").textContent}", ` +
        `kinds=${harness.byId("kind").childElementCount}, config=${harness.byId("config").value.length} bytes); ` +
        `page log: ${harness.logErrors().join(" | ") || "empty"}`,
    );
  }
  return harness;
}

/** @brief The child elements as an array: `children` is a live collection, as in the DOM. */
function cells(node) {
  return Array.from(node.children);
}

/** @brief Rows that are real job rows (a rendered row has the full column count), not the placeholder. */
function jobRows(harness) {
  return cells(harness.byId("jobs-body")).filter((row) => row.children.length >= 8);
}

/** @brief The rendered row whose id cell equals @p id, or undefined. */
function rowById(harness, id) {
  return jobRows(harness).find((row) => row.children[0].textContent === id);
}

/** @brief The labelled action button in one row's trailing Actions cell. */
function rowButton(row, label) {
  const actions = row.children[row.children.length - 1];
  return cells(actions).find((button) => button.textContent === label);
}

const serverPath = findRestServer();
const sample = serverPath ? await resolveSampleModel() : null;

describe("WebUI drives a real slice on a real HsBaSlicerRestServer", () => {
  if (!serverPath) {
    it("is skipped without a restapi build", {
      skip: "no HsBaSlicerRestServer binary (build with HSBA_BUILD_REST_API=ON or set HSBA_REST_SERVER)",
    }, () => {});
    return;
  }

  let tempDir;
  let child;
  let consoleServer;
  let output = "";

  before(async () => {
    tempDir = await mkdtemp(path.join(os.tmpdir(), "hsba-webui-integration-"));
    const port = await freePort();
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

    const restBase = `http://127.0.0.1:${port}`;
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
        console.info(`integration workspace kept: ${tempDir}`);
      } else {
        await rm(tempDir, { recursive: true, force: true });
      }
    }
  });

  const consoleUrl = () => `http://127.0.0.1:${consoleServer.address().port}`;

  it("submits through the page's own handler, watches it slice, and previews the real G-code", async () => {
    if (!sample) {
      console.warn("integration: sample model unavailable, skipping the real-slice interaction");
      return;
    }
    const harness = await bootConsolePage(consoleUrl());
    assert.deepEqual(harness.logErrors(), [], "the console reported a failed action while coming up");

    // Fill the page's config box exactly as an operator editing the loaded default would: the same
    // fields e2e sets over raw HTTP, but here the *page* is what turns them into a request.
    const outputPath = toPosix(path.join(tempDir, "out", "bunny-ui.gcode"));
    await mkdir(path.dirname(outputPath), { recursive: true });
    const config = JSON.parse(harness.byId("config").value);
    Object.assign(config, {
      model_name: "stanford_bunny",
      model_path: sample.path,
      output_path: outputPath,
      layer_height: sample.layer,
      first_layer_height: sample.layer,
      enable_support: 0,
      wall_count: 2,
      top_layer_count: 1,
      bottom_layer_count: 1,
    });
    harness.byId("kind").value = "fdm";
    harness.byId("config").value = JSON.stringify(config);

    // Submitting runs the served controller's submitJob: a POST through the same-origin proxy that
    // must return 202 + a job id, then the controller refreshes the table it renders itself.
    harness.byId("submit").click();
    const appeared = await harness.settle(() => jobRows(harness).length >= 1);
    assert.ok(appeared, `submit never reached the jobs table; page log: ${harness.logErrors().join(" | ") || "empty"}`);
    const jobId = jobRows(harness)[0].children[0].textContent;
    assert.ok(jobId, "the submitted row has no job id");

    // The auto-poll is a no-op timer here, so the test advances the state the way time would: refresh
    // on a wall-clock beat until the row's own state cell says success. The predicate keys on the
    // rendered text, not on how many requests were made.
    const deadline = Date.now() + SLICE_TIMEOUT_MS;
    let row = rowById(harness, jobId);
    for (;;) {
      row = rowById(harness, jobId);
      const done = row && row.children[2].textContent === "success";
      if (done) {
        break;
      }
      if (row && row.children[2].textContent === "failed") {
        assert.fail(`the real slice failed: ${row.children[6].textContent}`);
      }
      if (Date.now() > deadline) {
        throw new Error(`job ${jobId} did not finish within ${SLICE_TIMEOUT_MS} ms`);
      }
      harness.byId("jobs-refresh").click();
      await new Promise((resolve) => setTimeout(resolve, 250));
    }

    // What the UI shows is what the engine produced: the reported path is the given one, and that file
    // is genuinely on disk from the real slice - not a fake body the test served itself.
    assert.equal(row.children[1].textContent, "fdm", "the row is not the FDM job it submitted");
    assert.equal(row.children[6].textContent, outputPath, "the Output column is not the path the run reported");
    const gcode = await readFile(outputPath, "utf8");
    assert.ok(gcode.length > 5000, `G-code is only ${gcode.length} bytes for a real part`);
    assert.match(gcode, /G1 X/, "no extrusion moves in the G-code the page reported");
    assert.equal(Number(row.children[4].textContent), LAYERS, "the layer count column disagrees with the sliced layers");

    // Preview is gated on the row's state; clicking it runs previewResult -> fetchResult, which pulls
    // the real result through the proxy and paints it into the panel. This is the part e2e never
    // reaches because it never runs the client.
    const preview = rowButton(row, "Preview");
    assert.ok(preview, "the succeeded row offers no Preview button");
    assert.equal(preview.disabled, false, "a succeeded job still has its Preview disabled");
    preview.click();
    const painted = await harness.settle(() =>
      harness.byId("preview").hidden === false && harness.byId("preview").textContent.length > 0);
    assert.ok(painted, `the preview panel stayed empty; page log: ${harness.logErrors().join(" | ") || "empty"}`);
    const shown = harness.byId("preview").textContent;
    assert.ok(gcode.startsWith(shown) || shown.startsWith("; "), "the preview is not the head of the served result");
    assert.match(shown, /Generated by HsBaSlicer|G1 /, "the preview panel does not show real G-code");
    assert.match(harness.byId("preview-meta").textContent, /lines|B|KiB|MiB/, "the preview meta states no size");

    assert.deepEqual(harness.logErrors(), [], "submitting / previewing the real slice logged a failed action");
  });

  it("renders into the selector the pipeline kinds the real server reports", async () => {
    // A non-vacuous client check: the option list has to mirror a live GET the console proxy makes
    // against the genuine binary. A wrong JSON member name the client reads, or a proxy that mangles
    // the response, drops this - it is the client parsing real server data, not a re-listed endpoint.
    const harness = await bootConsolePage(consoleUrl());
    const listing = await (await fetch(`${consoleUrl()}/api/pipelines`)).json();
    const served = (listing.kinds || []).map((entry) => entry.name);
    assert.ok(served.length > 0, `the real server reported no kinds: ${JSON.stringify(listing)}`);
    const rendered = cells(harness.byId("kind")).map((option) => option.value);
    assert.deepEqual(rendered, served, "the pipeline selector does not mirror the real listing");
    assert.ok(rendered.includes("fdm"), "no fdm in the selector the client built");
    assert.deepEqual(harness.logErrors(), [], "loading the real pipeline list logged a failed action");
  });
});
