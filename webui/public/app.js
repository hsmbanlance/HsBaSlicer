"use strict";

/**
 * @file app.js
 * @brief Browser console for HsBaSlicerRestServer; every call goes to /api/* on this origin.
 *
 * The page is served by webui/server.js, which proxies /api/* to the REST server, so all
 * requests here are same-origin and need no CORS support on the C++ side.
 */

const TOKEN_KEY = "hsba.console.token";
const HEALTH_EVERY_MS = 5000;
const JOBS_EVERY_MS = 3000;
const LOG_LIMIT = 200;
const JOB_COLUMNS = 8;
const UPLOAD_COLUMNS = 8;
const FALLBACK_STAGE_WIDTH = 640;

// The one URI literal this bundle contains, and it is never dereferenced: it is the identifier the
// DOM uses to create real SVG nodes rather than unknown HTML elements, so neither the CSP nor
// working off the network is affected by it.
const SVG_NS = "http://www.w3.org/2000/svg";

/** The three geometry categories the API reports, each with the checkbox that shows it. */
const GEOMETRY_GROUPS = [
  { key: "outlines", className: "geometry-outline", toggle: "geometry-outline" },
  { key: "fills", className: "geometry-fill", toggle: "geometry-fill" },
  { key: "supports", className: "geometry-support", toggle: "geometry-support" },
];

const byId = (id) => document.getElementById(id);

function storedToken() {
  return localStorage.getItem(TOKEN_KEY) || "";
}

function writeLog(message, level) {
  const line = document.createElement("div");
  line.className = "log-line log-" + (level || "info");
  line.textContent = new Date().toLocaleTimeString() + "  " + message;
  const box = byId("log");
  box.appendChild(line);
  while (box.childElementCount > LOG_LIMIT) {
    box.removeChild(box.firstElementChild);
  }
  box.scrollTop = box.scrollHeight;
}

/** Surface a failed response, preferring the {code, message, detail} error body. */
function failureText(status, body, path) {
  const raw = (body || "").trim();
  try {
    const parsed = JSON.parse(raw);
    const detail = [parsed.message, parsed.detail].filter(Boolean).join(" / ");
    if (detail) {
      return "HTTP " + status + " " + path + ": " + detail;
    }
  } catch (ignored) {
    // Not a JSON error body; report the raw payload instead.
  }
  return "HTTP " + status + " " + path + ": " + (raw || "empty response");
}

/** Fetch with JSON negotiation and the optional shared token. */
async function call(path, options) {
  const settings = Object.assign({}, options || {});
  const headers = new Headers(settings.headers || {});
  headers.set("Accept", "application/json");
  const credential = storedToken();
  if (credential) {
    headers.set("X-API-Key", credential);
  }
  settings.headers = headers;

  let response;
  try {
    response = await fetch(path, settings);
  } catch (network) {
    throw new Error(path + ": " + network.message);
  }
  const text = await response.text();
  if (!response.ok) {
    throw new Error(failureText(response.status, text, path));
  }
  return { response: response, text: text };
}

async function callJson(path, options) {
  const result = await call(path, options);
  return result.text ? JSON.parse(result.text) : null;
}

function addCell(row, text, className) {
  const cell = document.createElement("td");
  if (className) {
    cell.className = className;
  }
  cell.textContent = text;
  row.appendChild(cell);
  return cell;
}

function emptyRow(body, columns, text) {
  const row = document.createElement("tr");
  addCell(row, text, "empty").colSpan = columns;
  body.appendChild(row);
  return row;
}

/**
 * Make sure `row` owns a cell at `index` and bring it up to date.
 *
 * Cells are created once and then rewritten, which is what lets a refresh keep the nodes (and the
 * handlers bound to them) instead of throwing the table away and building an identical one.
 */
function cellAt(row, index, values) {
  let cell = row.children[index];
  if (!cell) {
    cell = document.createElement("td");
    row.appendChild(cell);
  }
  if (values.className !== undefined) {
    cell.className = values.className;
  }
  // Assigning textContent wipes children, so only a plain text column is ever given one, and the
  // assignment only happens when the text really changed: even writing back the identical string
  // replaces the text node, which is what cancelled a selection inside the path column on the next
  // poll while the row itself survived.
  if (values.text !== undefined && cell.textContent !== values.text) {
    cell.textContent = values.text;
  }
  cell.title = values.title || "";
  return cell;
}

/** Bind a click handler the first time a node appears; every later refresh reuses that binding. */
function bindOnce(node, handler) {
  if (node.bound) {
    return;
  }
  node.bound = true;
  node.addEventListener("click", () => run(handler));
}

/**
 * The Actions cell: one button per entry, created once and re-armed on every refresh. The handlers
 * read the object the row currently holds rather than closing over one listing, so a button that
 * survived a refresh cannot act on a stale snapshot of its job.
 */
function actionsCell(row, index, actions) {
  const cell = cellAt(row, index, { className: "actions" });
  const item = row.job || row.upload;
  actions.forEach((action, at) => {
    let button = cell.children[at];
    if (!button) {
      button = document.createElement("button");
      button.type = "button";
      button.textContent = action.label;
      cell.appendChild(button);
      bindOnce(button, () => action.handler(row));
    }
    button.disabled = !action.enabled(item);
  });
  return cell;
}

/**
 * The path column of a row: the value the API reports, written in full (CSS wraps it rather than
 * clipping it) or the job's error text when nothing was produced. Clicking copies the current
 * value, so a row that only grows a path later is already clickable.
 */
function pathCell(row, index, path, fallback, read) {
  const cell = cellAt(row, index, {
    className: "path" + (path ? " has-value" : ""),
    text: path || fallback || "-",
    title: path ? path + "\n(click to copy)" : fallback || "",
  });
  bindOnce(cell, () => copyText(read(row)));
  return cell;
}

/**
 * What the Layer geometry panel currently shows. The paths are kept after a fetch so that toggling
 * a category or zooming redraws from memory instead of asking the server for the layer again.
 */
const geometry = {
  id: "",
  summary: null,
  layer: 0,
  z: 0,
  base: null,
  mirror: 0,
  paths: { outlines: [], fills: [], supports: [] },
  counts: null,
  zoom: 1,
  pan: { x: 0, y: 0 },
};

/** Drag bookkeeping of the geometry stage, in client pixels. */
let drag = null;

const JOB_ACTIONS = [
  { label: "Preview", enabled: (job) => job.state === "success", handler: (row) => previewResult(row.job.id, row.job.kind) },
  { label: "Result", enabled: (job) => job.state === "success", handler: (row) => downloadResult(row.job.id, row.job.kind) },
  // geometry_available is the server's own answer to "can the geometry endpoints serve this job":
  // it already folds in the terminal-state rule and the snapshot pool having recycled this job.
  { label: "Geometry", enabled: (job) => job.geometry_available === 1, handler: (row) => openGeometry(row.job.id, row.job) },
  { label: "Cancel", enabled: (job) => job.state === "queued" || job.state === "running", handler: (row) => cancelJob(row.job.id) },
  { label: "Delete", enabled: (job) => job.state !== "running", handler: (row) => deleteJob(row.job.id) },
];

const UPLOAD_ACTIONS = [
  { label: "Delete", enabled: (upload) => upload.state !== "in_use", handler: (row) => deleteUpload(row.upload.id) },
];

/** One job row: the values of the current listing, kept in step with the row already on screen. */
function buildJobRow(row, job) {
  row.job = job;
  cellAt(row, 0, { className: "mono", text: job.id });
  cellAt(row, 1, { text: job.kind });
  cellAt(row, 2, { className: "state-" + job.state, text: job.state, title: job.error_message || "" });
  cellAt(row, 3, {
    className: "muted",
    // A Lua pipeline reports free-form progress text and may report none; the stable stage key the
    // API derives is the fallback, so a row is never just a bare percentage.
    text: job.progress_percent + "%" +
      (job.progress_stage || job.progress_stage_id ? " " + (job.progress_stage || job.progress_stage_id) : ""),
    title: job.progress_stage_id || "",
  });
  cellAt(row, 4, { text: String(job.total_layers) });
  cellAt(row, 5, { text: formatSeconds(job.elapsed_seconds) });
  pathCell(row, 6, job.output_path, job.error_message, (r) => r.job.output_path || r.job.error_message);
  actionsCell(row, 7, JOB_ACTIONS);
}

/** One upload row: the stored server-side path is a column of its own, not a tooltip. */
function buildUploadRow(row, upload) {
  row.upload = upload;
  cellAt(row, 0, { className: "mono", text: upload.id });
  cellAt(row, 1, { className: "mono", text: upload.job_id || "-" });
  cellAt(row, 2, { text: upload.original_name || upload.file_name });
  pathCell(row, 3, upload.path, "-", (r) => r.upload.path);
  cellAt(row, 4, { text: formatBytes(upload.size_bytes) });
  cellAt(row, 5, { className: "state-" + upload.state, text: upload.state });
  cellAt(row, 6, { className: "muted", text: formatRemaining(upload.remaining_seconds) });
  actionsCell(row, 7, UPLOAD_ACTIONS);
}

/**
 * A table body that keeps exactly one <tr> per row id.
 *
 * Rebuilding the whole body on every poll, as this file used to, threw away every node about every
 * three seconds: a selection in the path column vanished, the hovered row lost its highlight, and a
 * click that landed while the table was swapped out hit a detached node and did nothing. Here a row
 * that is already in place is only updated; new rows are inserted, stale ones removed, and nothing
 * else moves.
 */
function keyedTable(body, columns) {
  const rows = new Map();
  let placeholder = null;
  // index.html ships a placeholder row so the table is never empty before the first response; the
  // first render takes the body over, because that row is not one of the keyed rows.
  let ownsBody = false;

  /** Put `row` at `index`, leaving every row that is already in position untouched. */
  function place(index, row) {
    if (body.children[index] === row) {
      return;
    }
    // `children` is a live HTMLCollection: it has indexed access and a length, but no Array method,
    // so asking it for a position has to go through Array.prototype.
    if (Array.prototype.indexOf.call(body.children, row) !== -1) {
      body.removeChild(row);
    }
    const reference = body.children[index];
    if (reference) {
      body.insertBefore(row, reference);
    } else {
      body.appendChild(row);
    }
  }

  function render(items, buildRow, emptyText) {
    if (!ownsBody) {
      ownsBody = true;
      body.replaceChildren();
    }
    if (!items.length) {
      rows.clear();
      body.replaceChildren();
      placeholder = emptyRow(body, columns, emptyText);
      return;
    }
    if (placeholder) {
      body.removeChild(placeholder);
      placeholder = null;
    }
    const alive = new Set();
    items.forEach((item, index) => {
      alive.add(item.id);
      let row = rows.get(item.id);
      if (!row) {
        row = document.createElement("tr");
        rows.set(item.id, row);
      }
      buildRow(row, item);
      place(index, row);
    });
    for (const [id, row] of Array.from(rows)) {
      if (!alive.has(id)) {
        rows.delete(id);
        body.removeChild(row);
      }
    }
  }

  return { render: render };
}

/** The keyed body of one table, created on its first refresh. */
const tables = {};

function tableFor(id, columns) {
  if (!tables[id]) {
    tables[id] = keyedTable(byId(id), columns);
  }
  return tables[id];
}

async function copyText(value) {
  if (!navigator.clipboard) {
    throw new Error("clipboard unavailable: open the console on localhost or over https");
  }
  await navigator.clipboard.writeText(value);
  writeLog("copied " + value, "ok");
}

/** Run an action, reporting failures in the log panel instead of the browser console. */
function run(action) {
  Promise.resolve()
    .then(action)
    .catch((error) => writeLog(error.message, "error"));
}

function formatSeconds(value) {
  return value > 0 ? value.toFixed(2) + " s" : "-";
}

function formatBytes(value) {
  if (!(value > 0)) {
    return "0 B";
  }
  const units = ["B", "KiB", "MiB", "GiB"];
  let size = value;
  let unit = 0;
  while (size >= 1024 && unit < units.length - 1) {
    size /= 1024;
    unit += 1;
  }
  return size.toFixed(unit === 0 ? 0 : 1) + " " + units[unit];
}

function formatRemaining(remaining) {
  if (!(remaining > 0)) {
    return "never";
  }
  if (remaining < 3600) {
    return Math.max(1, Math.round(remaining / 60)) + " min";
  }
  return Math.round(remaining / 3600) + " h";
}

async function refreshHealth() {
  const badge = byId("health");
  try {
    const health = await callJson("/api/health");
    badge.textContent = "ok - " + health.uptime_seconds + " s";
    badge.className = "badge badge-ok";
  } catch (error) {
    badge.textContent = error.message.indexOf("HTTP 401") === 0 ? "token required" : "unreachable";
    badge.className = "badge badge-down";
  }
}

async function refreshVersion() {
  const target = byId("version");
  try {
    const info = await callJson("/api/version");
    target.textContent = [info.librariesName, info.version, info.buildType].filter(Boolean).join(" ");
  } catch (error) {
    target.textContent = error.message;
  }
}

async function loadKinds() {
  const select = byId("kind");
  const listing = await callJson("/api/pipelines");
  select.replaceChildren();
  for (const entry of (listing && listing.kinds) || []) {
    const option = document.createElement("option");
    option.value = entry.name;
    option.textContent = entry.name;
    select.appendChild(option);
  }
  if (!select.childElementCount) {
    throw new Error("the REST server reported no pipeline kinds");
  }
}

async function loadDefaultConfig() {
  const kind = byId("kind").value;
  if (!kind) {
    throw new Error("no pipeline selected");
  }
  const result = await call("/api/config/default/" + encodeURIComponent(kind) + "?format=json");
  byId("config").value = result.text;
  writeLog("loaded the default config of " + kind, "ok");
}

async function submitJob() {
  const kind = byId("kind").value;
  const config = byId("config").value.trim();
  if (!kind) {
    throw new Error("no pipeline selected");
  }
  if (!config) {
    throw new Error("load a default config (or paste one) before submitting");
  }

  const capture = byId("geometry-capture").checked ? "?geometry=1" : "";
  const path = "/api/jobs/" + encodeURIComponent(kind) + capture;
  const file = byId("model").files[0];
  let job;
  if (file) {
    const form = new FormData();
    form.append("config", new Blob([config], { type: "application/json" }), "config.json");
    form.append("model", file, file.name);
    job = await callJson(path, { method: "POST", body: form });
  } else {
    job = await callJson(path, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: config
    });
  }
  writeLog("submitted " + job.id + " (" + kind + ")", "ok");
  await refreshJobs();
}

async function refreshJobs() {
  const listing = await callJson("/api/jobs?limit=50");
  tableFor("jobs-body", JOB_COLUMNS).render((listing && listing.jobs) || [], buildJobRow, "no jobs yet");
}

async function refreshUploads() {
  let uploads = [];
  let notice = "no uploaded models";
  try {
    const listing = await callJson("/api/uploads?limit=50");
    uploads = (listing && listing.uploads) || [];
  } catch (error) {
    // An upload pool that is switched off is a state worth showing, not a failed action.
    notice = error.message;
  }
  tableFor("uploads-body", UPLOAD_COLUMNS).render(uploads, buildUploadRow, notice);
}

/**
 * Fetch one finished job result together with the metadata the preview and the download need.
 *
 * The name comes from Content-Disposition and is normalized by HsBaHttp.resultFilename, which is
 * also what the unit tests exercise: a server-side path must never reach the save dialog.
 */
async function fetchResult(id, kind) {
  const path = "/api/jobs/" + encodeURIComponent(id) + "/result";
  const credential = storedToken();
  const response = await fetch(path, { headers: credential ? { "X-API-Key": credential } : {} });
  if (!response.ok) {
    throw new Error(failureText(response.status, await response.text(), path));
  }
  return {
    blob: await response.blob(),
    name: HsBaHttp.resultFilename(response.headers.get("Content-Disposition"), id, kind),
    contentType: response.headers.get("Content-Type") || "",
  };
}

async function downloadResult(id, kind) {
  const { blob, name } = await fetchResult(id, kind);

  const link = document.createElement("a");
  link.href = URL.createObjectURL(blob);
  link.download = name;
  document.body.appendChild(link);
  link.click();
  link.remove();
  window.setTimeout(() => URL.revokeObjectURL(link.href), 10000);
  writeLog("downloaded " + name + " (" + formatBytes(blob.size) + ")", "ok");
}

/**
 * Show the start of a job result in the preview panel instead of only offering a download.
 *
 * A real G-code file is far too large to render whole, so the text is cut to the preview budget
 * and the panel states how much was hidden. Binary results (a packed export, for instance) are
 * described rather than decoded, because printing bytes as UTF-8 text would be meaningless.
 */
async function previewResult(id, kind) {
  const box = byId("preview");
  const meta = byId("preview-meta");
  const { blob, name, contentType } = await fetchResult(id, kind);

  byId("preview-title").textContent = "Result preview - " + name;
  if (!HsBaHttp.isPreviewable(contentType)) {
    box.hidden = true;
    box.textContent = "";
    meta.textContent =
      formatBytes(blob.size) + " of " + (contentType || "unknown type") + ", not text - use Result to download it";
    writeLog("preview needs a download instead: " + name + " is " + (contentType || "binary"), "info");
    return;
  }

  const view = HsBaHttp.previewText(await blob.text());
  box.textContent = view.text;
  box.hidden = false;
  meta.textContent =
    formatBytes(view.bytes) + ", " + view.lines + " lines" +
    (view.truncated ? " - showing the first " + formatBytes(view.shownBytes) + ", use Result for the whole file" : "");
  writeLog("previewed " + name + " (" + formatBytes(view.shownBytes) + " of " + formatBytes(view.bytes) + ")", "ok");
}

function clearPreview() {
  const box = byId("preview");
  box.hidden = true;
  box.textContent = "";
  byId("preview-title").textContent = "Result preview";
  byId("preview-meta").textContent = "no result previewed yet";
}

/** @brief URL of one geometry endpoint of one job. */
function geometryPath(id, suffix) {
  return "/api/jobs/" + encodeURIComponent(id) + suffix;
}

/** Show a sentence in the panel instead of a drawing, and drop whatever was drawn before. */
function setGeometryMessage(text) {
  byId("geometry-stage").hidden = true;
  byId("geometry-svg").replaceChildren();
  byId("geometry-meta").textContent = text;
}

function geometryLayerCount() {
  return Number((geometry.summary && geometry.summary.layer_count) || 0) || 0;
}

function setGeometryControlsEnabled(enabled) {
  byId("geometry-layer").disabled = !enabled;
  byId("geometry-prev").disabled = !enabled;
  byId("geometry-next").disabled = !enabled;
  byId("geometry-reload").disabled = !enabled;
  byId("geometry-fit").disabled = !enabled;
}

/**
 * Open one job's geometry overview and draw its first layer.
 *
 * A job that never asked for capture, or whose snapshot has already been recycled by the pool, is
 * answered by the server with a 404 and its own sentence; that sentence is what the panel shows,
 * rather than an error being pushed into the log where nobody looking at the drawing would see it.
 */
async function openGeometry(id, job) {
  if (geometry.id !== id) {
    geometry.id = id;
    geometry.summary = null;
    geometry.layer = 0;
    geometry.zoom = 1;
    geometry.pan = { x: 0, y: 0 };
  }
  try {
    const summary = await callJson(geometryPath(id, "/geometry"));
    const count = Number(summary.layer_count) || 0;
    if (count <= 0) {
      setGeometryMessage("this job holds no layer geometry - submit it with capture geometry enabled");
      setGeometryControlsEnabled(false);
      return;
    }
    geometry.summary = summary;
    geometry.base = HsBaHttp.geometryView(summary.bounds);
    geometry.mirror = HsBaHttp.mirrorLine(summary.bounds);
    const slider = byId("geometry-layer");
    slider.min = "0";
    slider.max = String(count - 1);
    slider.value = String(geometry.layer);
    await showGeometryLayer(geometry.layer);
    writeLog("opened the geometry of " + id + " (" + count + " layers, " + job.kind + ")", "ok");
  } catch (error) {
    setGeometryMessage(error.message + " - the snapshot pool keeps only the last few jobs");
    setGeometryControlsEnabled(false);
    writeLog(error.message, "error");
  }
}

/** Fetch and draw one layer by index; the index is clamped into what the overview reported. */
async function showGeometryLayer(index) {
  if (!geometry.id) {
    return;
  }
  const count = geometryLayerCount();
  const wanted = Math.max(0, Math.min(count - 1, Math.trunc(Number(index) || 0)));
  try {
    const layer = await callJson(geometryPath(geometry.id, "/geometry/" + wanted));
    const bounds = layer.bounds && layer.bounds.length ? layer.bounds : (geometry.summary || {}).bounds;
    geometry.layer = wanted;
    geometry.base = HsBaHttp.geometryView(bounds);
    geometry.mirror = HsBaHttp.mirrorLine(bounds);
    geometry.z = Number(layer.z_height) || 0;
    const drawn = HsBaHttp.layerPaths(layer, geometry.mirror);
    geometry.paths = drawn;
    geometry.counts = drawn.counts;
    byId("geometry-layer-label").textContent = wanted + 1 + " / " + count;
    byId("geometry-layer").value = String(wanted);
    setGeometryControlsEnabled(true);
    renderGeometry();
  } catch (error) {
    setGeometryMessage(error.message);
    setGeometryControlsEnabled(false);
    writeLog(error.message, "error");
  }
}

/** One <g> per category: the class carries the colour, the path data carries the layer. */
function makeGeometryGroup(group, paths) {
  const container = document.createElementNS(SVG_NS, "g");
  container.setAttribute("class", group.className);
  for (const d of paths) {
    const path = document.createElementNS(SVG_NS, "path");
    path.setAttribute("d", d);
    container.appendChild(path);
  }
  return container;
}

/** Redraw the current layer from what is already fetched - no request, no re-parse. */
function renderGeometry() {
  if (!geometry.base) {
    return;
  }
  const svg = byId("geometry-svg");
  svg.setAttribute("viewBox", HsBaHttp.currentViewBox(geometry.base, geometry.zoom, geometry.pan));
  // An empty category gets no <g> at all: a pipeline that reports no supports (most of them) would
  // otherwise leave an invisible group in the document for every layer that is drawn.
  const visible = GEOMETRY_GROUPS
    .filter((group) => byId(group.toggle).checked && (geometry.paths[group.key] || []).length)
    .map((group) => makeGeometryGroup(group, geometry.paths[group.key]));
  svg.replaceChildren(...visible);
  byId("geometry-stage").hidden = false;

  const counts = geometry.counts || { outlines: 0, fills: 0, supports: 0, dropped: 0 };
  const undrawn = counts.dropped
    ? " (" + counts.dropped + " paths left undrawn, " + HsBaHttp.GEOMETRY_MAX_PATHS + " is the budget)"
    : "";
  byId("geometry-meta").textContent =
    "layer " + (geometry.layer + 1) + " / " + geometryLayerCount() +
    ", z " + geometry.z + " mm, outlines " + geometry.paths.outlines.length +
    " / fills " + geometry.paths.fills.length +
    " / supports " + geometry.paths.supports.length + undrawn;
}

/** Step to the neighbouring layer, or do nothing when the current one is already the edge. */
function stepGeometry(delta) {
  const count = geometryLayerCount();
  const next = geometry.layer + delta;
  if (!count || next < 0 || next > count - 1 || next === geometry.layer) {
    return;
  }
  return showGeometryLayer(next);
}

/** Fit is the identity at zoom 1, so resetting means dropping zoom and pan, not refetching. */
function resetGeometryView() {
  geometry.zoom = 1;
  geometry.pan = { x: 0, y: 0 };
  renderGeometry();
}

function stageWidth() {
  return Number(byId("geometry-stage").clientWidth) || FALLBACK_STAGE_WIDTH;
}

function zoomGeometry(event) {
  if (!geometry.base) {
    return;
  }
  if (typeof event.preventDefault === "function") {
    event.preventDefault();
  }
  const step = Number(event.deltaY) || 0;
  geometry.zoom = HsBaHttp.clampZoom(geometry.zoom * Math.exp(-step * 0.001));
  run(renderGeometry);
}

function beginDrag(event) {
  if (!geometry.base) {
    return;
  }
  drag = { x: Number(event.clientX) || 0, y: Number(event.clientY) || 0 };
}

/**
 * Move the view with the pointer.
 *
 * The pan lives in the viewBox's own (already mirrored) space, so both axes follow the pointer and
 * the flip ringPath applies to the coordinates never has to be repeated here.
 */
function moveDrag(event) {
  if (!drag || !geometry.base) {
    return;
  }
  const perPixel = geometry.base.w / geometry.zoom / stageWidth();
  const x = Number(event.clientX) || 0;
  const y = Number(event.clientY) || 0;
  geometry.pan.x -= (x - drag.x) * perPixel;
  geometry.pan.y -= (y - drag.y) * perPixel;
  drag = { x: x, y: y };
  run(renderGeometry);
}

function endDrag() {
  drag = null;
}

async function deleteJob(id) {
  await call("/api/jobs/" + encodeURIComponent(id), { method: "DELETE" });
  writeLog("deleted " + id, "ok");
  await refreshJobs();
}

async function deleteUpload(id) {
  await call("/api/uploads/" + encodeURIComponent(id), { method: "DELETE" });
  writeLog("deleted " + id, "ok");
  await refreshUploads();
}

/**
 * Ask a running job to stop.
 *
 * The request is a request: the pipeline notices it at its next safe point, so the row is expected
 * to keep saying "running" for a while. The log states that instead of claiming the job is gone.
 */
async function cancelJob(id) {
  const view = await callJson("/api/jobs/" + encodeURIComponent(id) + "/cancel", { method: "POST" });
  writeLog(
    "cancel requested for " + id + ": " + (view.detail || "accepted") + "; the job stops at its next safe point",
    "ok",
  );
  await refreshJobs();
}

function saveToken() {
  const value = byId("token").value.trim();
  if (value) {
    localStorage.setItem(TOKEN_KEY, value);
    writeLog("token saved for this browser", "ok");
  } else {
    localStorage.removeItem(TOKEN_KEY);
    writeLog("token cleared", "ok");
  }
  run(refreshVersion);
}

function init() {
  byId("origin").textContent = location.origin;
  byId("token").value = storedToken();

  byId("token-save").addEventListener("click", saveToken);
  byId("token-clear").addEventListener("click", () => {
    byId("token").value = "";
    saveToken();
  });
  byId("load-config").addEventListener("click", () => run(loadDefaultConfig));
  byId("submit").addEventListener("click", () => run(submitJob));
  byId("jobs-refresh").addEventListener("click", () => run(async () => {
    await refreshJobs();
    await refreshUploads();
  }));
  byId("uploads-refresh").addEventListener("click", () => run(refreshUploads));
  byId("preview-clear").addEventListener("click", () => run(clearPreview));
  clearPreview();

  byId("geometry-prev").addEventListener("click", () => run(() => stepGeometry(-1)));
  byId("geometry-next").addEventListener("click", () => run(() => stepGeometry(1)));
  byId("geometry-reload").addEventListener("click", () => run(() => showGeometryLayer(geometry.layer)));
  byId("geometry-fit").addEventListener("click", () => run(resetGeometryView));
  byId("geometry-layer").addEventListener("input", () => run(() => showGeometryLayer(byId("geometry-layer").value)));
  for (const group of GEOMETRY_GROUPS) {
    byId(group.toggle).addEventListener("click", () => run(renderGeometry));
  }
  const stage = byId("geometry-stage");
  stage.addEventListener("wheel", zoomGeometry);
  stage.addEventListener("mousedown", beginDrag);
  stage.addEventListener("mousemove", moveDrag);
  stage.addEventListener("mouseup", endDrag);
  stage.addEventListener("mouseleave", endDrag);
  setGeometryMessage("no layer geometry previewed yet");
  setGeometryControlsEnabled(false);

  run(refreshVersion);
  run(async () => {
    await loadKinds();
    await loadDefaultConfig();
  });
  run(refreshJobs);
  run(refreshUploads);
  run(refreshHealth);

  window.setInterval(() => run(refreshHealth), HEALTH_EVERY_MS);
  window.setInterval(() => {
    if (byId("jobs-auto").checked) {
      run(async () => {
        await refreshJobs();
        await refreshUploads();
      });
    }
  }, JOBS_EVERY_MS);
}

document.addEventListener("DOMContentLoaded", init);
