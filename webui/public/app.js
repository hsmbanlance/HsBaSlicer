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
 *
 * The stacked views keep the raw layer bodies in stackLayers, because their drawing depends on the
 * camera: spinning the view re-projects from memory and must never re-request a layer.
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
  view: "layer",
  angles: { yaw: 0, pitch: 0 },
  sideFace: 0,
  turn: 0,
  path: { xyz: [], count: 0, zRange: [], loaded: false },
  stack: null,
  stackLayers: new Map(),
};

/** Drag bookkeeping of the geometry stage, in client pixels. */
let drag = null;

/** @brief Pixels of horizontal drag that flip the side view onto its other face. */
const SIDE_FACE_PIXELS = 24;

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
  // Filling the select picks its first option without any change event reaching the page, so the
  // preview-image box has to be re-evaluated here rather than only when the operator switches kind.
  syncThumbnailOption();
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

/**
 * @brief Enable the preview-image option only for the pipeline that can embed one.
 *
 * Only FDM writes G-code, and only FDM's engine path projects a model into it, so leaving the box
 * tickable for another kind would let an operator ask for a picture that can never arrive.
 */
function syncThumbnailOption() {
  byId("fdm-thumbnails").disabled = byId("kind").value !== "fdm";
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

  let body = config;
  if (kind === "fdm") {
    body = HsBaHttp.applyConfigFlags(config, { gcode_thumbnails: byId("fdm-thumbnails").checked ? 1 : 0 });
    if (body === config) {
      // A config the console cannot parse is a config it cannot patch, and the operator deserves to
      // know the box had no effect rather than to wonder why the result carries no pictures.
      writeLog("config is not JSON - gcode_thumbnails not applied", "info");
    }
  }

  const capture = byId("geometry-capture").checked ? "?geometry=1" : "";
  const path = "/api/jobs/" + encodeURIComponent(kind) + capture;
  const file = byId("model").files[0];
  let job;
  if (file) {
    const form = new FormData();
    form.append("config", new Blob([body], { type: "application/json" }), "config.json");
    form.append("model", file, file.name);
    job = await callJson(path, { method: "POST", body: form });
  } else {
    job = await callJson(path, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: body
    });
  }
  writeLog("submitted " + job.id + " (" + kind + ")", "ok");
  await refreshJobs();
}

async function refreshJobs() {
  const listing = await callJson("/api/jobs?limit=50");
  const jobs = (listing && listing.jobs) || [];
  tableFor("jobs-body", JOB_COLUMNS).render(jobs, buildJobRow, "no jobs yet");
  // A package strip belongs to a job; once that job leaves the list its result may be gone, and a
  // strip that keeps pointing at a deleted archive is worse than one that says it stopped.
  if (pack.loaded && !jobs.some((job) => job.id === pack.id)) {
    clearPackage();
  }
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

/** @brief The result currently on screen, so Images can ask for the package of the right job. */
let previewedJob = { id: "", kind: "" };

/**
 * Show the start of a job result in the preview panel instead of only offering a download.
 *
 * A real G-code file is far too large to render whole, so the text is cut to the preview budget
 * and the panel states how much was hidden. Binary results (a packed export, for instance) are
 * described rather than decoded, because printing bytes as UTF-8 text would be meaningless.
 *
 * The same body is scanned for embedded preview images, which is why the text is read once into a
 * variable: the blocks live at the head of the file, so they survive the truncation the text view
 * applies, and a second fetch would ask the server for megabytes it already sent.
 */
async function previewResult(id, kind) {
  const box = byId("preview");
  const meta = byId("preview-meta");
  const { blob, name, contentType } = await fetchResult(id, kind);

  previewedJob = { id: id, kind: kind };
  clearThumbnails();
  // Both picture strips belong to the result on screen, so both have to be retired with it: leaving
  // the package strip up would show one job's layer under another job's heading.
  clearPackage();
  byId("preview-title").textContent = "Result preview - " + name;
  if (!HsBaHttp.isPreviewable(contentType)) {
    box.hidden = true;
    box.textContent = "";
    meta.textContent =
      formatBytes(blob.size) + " of " + (contentType || "unknown type") + ", not text - use Result to download it";
    writeLog("preview needs a download instead: " + name + " is " + (contentType || "binary"), "info");
    return;
  }

  const body = await blob.text();
  const view = HsBaHttp.previewText(body);
  box.textContent = view.text;
  box.hidden = false;
  const blocks = renderGcodeThumbnails(body);
  meta.textContent =
    formatBytes(view.bytes) + ", " + view.lines + " lines" +
    (view.truncated ? " - showing the first " + formatBytes(view.shownBytes) + ", use Result for the whole file" : "") +
    (kind === "fdm" && blocks === 0 ? " - no embedded previews, set gcode_thumbnails to 1" : "");
  writeLog("previewed " + name + " (" + formatBytes(view.shownBytes) + " of " + formatBytes(view.bytes) + ")", "ok");
}

/**
 * @brief Preview images the G-code of this result embeds, drawn above the text.
 * @param {string} text Result body (only its head is ever read, see HsBaHttp.parseGcodeThumbnails).
 * @returns {number} How many blocks the file carried, so the caller can say when it carried none.
 *
 * A block without bytes is a skip notice the engine wrote, and the honest thing to do with it is to
 * quote its reason instead of leaving a blank where the picture should have been.
 */
function renderGcodeThumbnails(text) {
  const stage = byId("gcode-thumbnails");
  const blocks = HsBaHttp.parseGcodeThumbnails(text);
  for (const block of blocks) {
    const url = HsBaHttp.thumbnailDataUrl(block);
    if (block.skipped) {
      const note = document.createElement("p");
      note.className = "thumbnail-note";
      note.textContent = (block.name || "preview") + " was not embedded: " + block.reason;
      stage.appendChild(note);
      continue;
    }
    if (!url) {
      continue;
    }
    const image = document.createElement("img");
    image.setAttribute("src", url);
    image.setAttribute("alt", block.name || "model preview");
    stage.appendChild(image);
    const label = document.createElement("span");
    label.className = "thumbnail-label";
    label.textContent = (block.name || "preview") + " (" + block.width + "x" + block.height + ")";
    stage.appendChild(label);
  }
  stage.hidden = stage.childElementCount === 0;
  return blocks.length;
}

/** @brief Drop whatever preview images the last result embedded. */
function clearThumbnails() {
  const stage = byId("gcode-thumbnails");
  stage.replaceChildren();
  stage.hidden = true;
}

/**
 * Which packed result the Images strip is showing, and how far into it the operator got.
 *
 * The entry list is fetched once and stepped through locally: a layer browser that asked the server
 * for the listing again on every click would be slower and would look like a bug when a package is
 * large.
 */
const pack = { id: "", entries: [], index: -1, loaded: false };

/** @brief Say one sentence in the package strip and put its picture away. */
function setPackageMessage(text) {
  byId("package-stage").hidden = true;
  byId("package-image").removeAttribute("src");
  byId("package-meta").textContent = text;
  pack.entries = [];
  pack.index = -1;
}

/** Forget the package of a job that is no longer being looked at. */
function clearPackage() {
  pack.id = "";
  pack.loaded = false;
  setPackageMessage("");
}

/**
 * List the images of the result just previewed and show the first one.
 *
 * The two answers a console can legitimately get for a job without a package are told apart because
 * they mean different things to the operator: a single-file result never has images, while a package
 * whose entries are all non-images means the export wrote something unexpected.
 */
async function browsePackage(id) {
  if (!id) {
    writeLog("preview a result first - Images works on the job whose result is on screen", "info");
    return;
  }
  let listing;
  try {
    listing = await callJson("/api/jobs/" + encodeURIComponent(id) + "/package");
  } catch (error) {
    setPackageMessage(
      /result is not a package/.test(error.message)
        ? "this result is a single file, not a package"
        : "no package to read: " + error.message,
    );
    // The server's own sentence belongs in the log, which is what run() writes for a rejected call.
    throw error;
  }
  pack.id = (listing && listing.id) || id;
  pack.entries = HsBaHttp.packageImages((listing && listing.entries) || []);
  pack.loaded = true;
  if (!pack.entries.length) {
    setPackageMessage("package has no images - SLA exports PNG/JPG/SVG layers");
    writeLog("package of " + pack.id + " holds no images", "info");
    return;
  }
  showPackageImage(0);
}

/** @brief Point the strip at one entry, without asking the server for anything but the picture. */
function showPackageImage(index) {
  const entry = pack.entries[index];
  if (!entry) {
    return;
  }
  pack.index = index;
  byId("package-image").setAttribute("src", HsBaHttp.packageEntryUrl(pack.id, entry.path));
  byId("package-meta").textContent = entry.path + " (" + (index + 1) + "/" + pack.entries.length + ")";
  byId("package-stage").hidden = false;
}

/** Step to the neighbouring image, or stay put at either end of the package. */
function stepPackage(delta) {
  if (!pack.loaded || !pack.entries.length) {
    return;
  }
  showPackageImage(Math.min(pack.entries.length - 1, Math.max(0, pack.index + delta)));
}

function clearPreview() {
  const box = byId("preview");
  box.hidden = true;
  box.textContent = "";
  byId("preview-title").textContent = "Result preview";
  byId("preview-meta").textContent = "no result previewed yet";
  previewedJob = { id: "", kind: "" };
  clearThumbnails();
  clearPackage();
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

/** @brief Whether the overview says this snapshot holds a continuous toolpath at all. */
function hasContinuousPath() {
  return (geometry.summary || {}).has_path === 1;
}

/** @brief Camera of the current view; the side view is quantized to two faces on purpose. */
function geometryAngles() {
  if (geometry.view === "side") {
    return { yaw: HsBaHttp.GEOMETRY_SIDE_YAWS[geometry.sideFace], pitch: 0 };
  }
  return geometry.angles;
}

function setGeometryControlsEnabled(enabled) {
  // The stepper only drives the single-layer view: a stacked drawing shows a sample of all layers.
  const stepping = enabled && geometry.view === "layer";
  byId("geometry-layer").disabled = !stepping;
  byId("geometry-prev").disabled = !stepping;
  byId("geometry-next").disabled = !stepping;
  byId("geometry-reload").disabled = !enabled;
  byId("geometry-fit").disabled = !enabled;
  for (const view of HsBaHttp.GEOMETRY_VIEWS) {
    byId("geometry-view-" + view).disabled = !enabled;
  }
  // A run that only deposited per-layer rings has no polyline to ask for, and the endpoint would
  // answer 404, so the button says so instead of letting the operator pay for the round trip.
  if (enabled && !hasContinuousPath()) {
    byId("geometry-view-path").disabled = true;
  }
}

/** @brief Fetch and draw whatever the selected view needs. */
function showGeometryView() {
  switch (geometry.view) {
    case "path":
      return loadGeometryPath(false);
    case "stack":
    case "side":
      return loadGeometryStack(false);
    default:
      return showGeometryLayer(geometry.layer);
  }
}

/** @brief Reload button: drop the caches of the current view and ask the server again. */
function reloadGeometry() {
  switch (geometry.view) {
    case "path":
      return loadGeometryPath(true);
    case "stack":
    case "side":
      return loadGeometryStack(true);
    default:
      return showGeometryLayer(geometry.layer);
  }
}

/** @brief Keep the pressed look of the view buttons in step with the state. */
function markGeometryView(view) {
  // A projected drawing turns with the pointer rather than sliding, and the cursor has to say so.
  byId("geometry-stage").setAttribute("class", view === "layer" ? "geometry-stage" : "geometry-stage geometry-3d");
  for (const name of HsBaHttp.GEOMETRY_VIEWS) {
    const button = byId("geometry-view-" + name);
    if (name === view) {
      button.setAttribute("class", "active");
    } else {
      button.removeAttribute("class");
    }
  }
}

/** @brief Back to the angle a view starts with: a projection fitted to a plan view fits nothing. */
function resetGeometryCamera() {
  geometry.zoom = 1;
  geometry.pan = { x: 0, y: 0 };
  geometry.sideFace = 0;
  geometry.turn = 0;
  geometry.angles = { yaw: HsBaHttp.GEOMETRY_STACK_ANGLES.yaw, pitch: HsBaHttp.GEOMETRY_STACK_ANGLES.pitch };
}

/**
 * Switch to another way of looking at the same snapshot.
 *
 * A path that the overview did not announce is refused here rather than requested: the server can
 * only ever answer that with a 404, and the panel states which pipelines can produce one.
 */
async function selectGeometryView(view) {
  if (geometry.view === view) {
    return;
  }
  if (view === "path" && !hasContinuousPath()) {
    const refusal = "this run deposited no continuous path - spiral_mode=1 belongs to fdm, tdp and waam";
    setGeometryMessage(refusal);
    writeLog(refusal, "error");
    return;
  }
  geometry.view = view;
  markGeometryView(view);
  resetGeometryCamera();
  await showGeometryView();
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
    geometry.path = { xyz: [], count: 0, zRange: [], loaded: false };
    geometry.stack = null;
    geometry.stackLayers.clear();
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
    // The selected view survives a switch to another job unless the new snapshot cannot serve it:
    // asking for a path the overview never announced would only trade the drawing for a 404.
    if (geometry.view === "path" && !hasContinuousPath()) {
      geometry.view = "layer";
    }
    markGeometryView(geometry.view);
    const slider = byId("geometry-layer");
    slider.min = "0";
    slider.max = String(count - 1);
    slider.value = String(geometry.layer);
    await showGeometryView();
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

/**
 * Append one <g> per category to @p parent, in deposition order.
 *
 * An empty category gets no <g> at all: a pipeline that reports no supports (most of them) would
 * otherwise leave an invisible group in the document for every layer that is drawn. With @p total
 * above zero the caller wants the sequence coloured instead of left to the stylesheet, which is the
 * only way to tell the first wall from the last one, and every coloured path also gets a triangle
 * pointing where the run was heading.
 */
function drawGeometryGroup(parent, entries, total) {
  for (const entry of entries) {
    if (!entry.items.length) {
      continue;
    }
    const group = document.createElementNS(SVG_NS, "g");
    group.setAttribute("class", entry.className);
    const marks = [];
    entry.items.forEach((item, index) => {
      const path = document.createElementNS(SVG_NS, "path");
      path.setAttribute("d", item.d);
      if (total > 0) {
        // A presentation attribute, so the stylesheet below can still drive an unordered drawing.
        path.setAttribute("stroke", HsBaHttp.sequenceColor(entry.start + index, total));
        for (const mark of HsBaHttp.arrowMark(item.points || [], geometry.base)) {
          marks.push(mark);
        }
      }
      group.appendChild(path);
    });
    parent.appendChild(group);
    if (marks.length) {
      const container = document.createElementNS(SVG_NS, "g");
      container.setAttribute("class", "geometry-arrow");
      for (const mark of marks) {
        const path = document.createElementNS(SVG_NS, "path");
        path.setAttribute("d", mark);
        container.appendChild(path);
      }
      parent.appendChild(container);
    }
  }
}

/**
 * Fetch the continuous toolpath a spiral run deposited, and draw it as one polyline.
 *
 * This is the only view built from real 3D data instead of a per-layer cross-section: the server
 * hands back the helix the run actually extruded, so its z rises along the line instead of
 * stepping between layers.
 */
async function loadGeometryPath(force) {
  if (!geometry.id) {
    return;
  }
  if (geometry.path.loaded && !force) {
    setGeometryControlsEnabled(true);
    renderGeometry();
    return;
  }
  try {
    const body = await callJson(geometryPath(geometry.id, "/geometry/path"));
    geometry.path = {
      xyz: (body && body.xyz) || [],
      count: Number(body && body.point_count) || 0,
      zRange: (body && body.z_range) || [],
      loaded: true,
    };
    setGeometryControlsEnabled(true);
    renderGeometry();
  } catch (error) {
    setGeometryMessage(error.message + " - spiral runs only");
    setGeometryControlsEnabled(false);
    writeLog(error.message, "error");
  }
}

/**
 * Fetch the layers a stacked view samples, one after another, and draw them.
 *
 * The sample is deliberately small (GEOMETRY_STACK_LAYERS) and the requests are serial: a page that
 * asked for a hundred layers at once would queue them behind the slice it is also watching.
 * Answers are cached per index, because spinning the view re-projects from memory.
 */
async function loadGeometryStack(force) {
  if (!geometry.id) {
    return;
  }
  const count = geometryLayerCount();
  if (!count) {
    return;
  }
  if (force) {
    geometry.stackLayers.clear();
    geometry.stack = null;
  }
  const sample = HsBaHttp.sampleLayers(count, HsBaHttp.GEOMETRY_STACK_LAYERS);
  try {
    for (const index of sample.indices) {
      if (!geometry.stackLayers.has(index)) {
        geometry.stackLayers.set(index, await callJson(geometryPath(geometry.id, "/geometry/" + index)));
      }
    }
    geometry.stack = { indices: sample.indices, focus: sample.focus };
    setGeometryControlsEnabled(true);
    renderGeometry();
  } catch (error) {
    setGeometryMessage(error.message);
    setGeometryControlsEnabled(false);
    writeLog(error.message, "error");
  }
}

/** The single-layer view: the behaviour the panel shipped with, unchanged. */
function renderLayerView(svg, ordered) {
  svg.setAttribute("viewBox", HsBaHttp.currentViewBox(geometry.base, geometry.zoom, geometry.pan));
  let total = 0;
  const entries = GEOMETRY_GROUPS
    .filter((group) => byId(group.toggle).checked && (geometry.paths[group.key] || []).length)
    .map((group) => {
      const items = geometry.paths[group.key].map((d) => ({ d: d, points: null }));
      const entry = { className: group.className, items: items, start: total };
      total += items.length;
      return entry;
    });
  svg.replaceChildren();
  drawGeometryGroup(svg, entries, ordered ? total : 0);
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

/**
 * Draw the stack or the side view: a sample of layers seen from one fixed camera.
 *
 * Every layer is its own <g> so the eye can attribute a ring to a height, and the budget is shared
 * between them (see HsBaHttp.stackBudget) so a twelve-layer sample cannot add up to twelve times the
 * node count of one layer. Nothing connects one layer to the next: the projection makes a straight
 * line between neighbouring rings look like travel, and travel is inferred, not measured.
 */
function renderStackView(svg, ordered) {
  const angles = geometryAngles();
  const indices = geometry.stack.indices;
  const budget = HsBaHttp.stackBudget(indices.length);
  const vertices = [];
  const planes = [];
  let total = 0;
  let dropped = 0;
  for (const index of indices) {
    const layer = geometry.stackLayers.get(index);
    if (!layer) {
      continue;
    }
    const z = Number(layer.z_height) || 0;
    let remaining = budget;
    const entries = [];
    for (const group of GEOMETRY_GROUPS) {
      if (!byId(group.toggle).checked) {
        continue;
      }
      const drawn = HsBaHttp.projectedRingPaths(layer[group.key] || [], z, angles, remaining);
      remaining -= drawn.used;
      dropped += drawn.dropped;
      if (!drawn.paths.length) {
        continue;
      }
      entries.push({
        className: group.className,
        items: drawn.paths.map((d, at) => ({ d: d, points: drawn.points[at] })),
      });
      total += drawn.paths.length;
      for (const points of drawn.points) {
        vertices.push.apply(vertices, points);
      }
    }
    if (!entries.length) {
      continue;
    }
    const node = document.createElementNS(SVG_NS, "g");
    node.setAttribute("class", index === geometry.stack.focus ? "geometry-layer geometry-focus" : "geometry-layer");
    node.setAttribute("data-layer", String(index));
    planes.push({ node: node, entries: entries });
  }
  if (!planes.length) {
    return;
  }

  geometry.base = HsBaHttp.projectedViewBox(vertices);
  svg.setAttribute("viewBox", HsBaHttp.currentViewBox(geometry.base, geometry.zoom, geometry.pan));
  svg.replaceChildren();
  let start = 0;
  for (const plane of planes) {
    for (const entry of plane.entries) {
      entry.start = start;
      start += entry.items.length;
    }
    drawGeometryGroup(plane.node, plane.entries, ordered ? total : 0);
    svg.appendChild(plane.node);
  }
  byId("geometry-stage").hidden = false;

  const undrawn = dropped ? " (" + dropped + " left undrawn, " + budget + " per layer)" : "";
  byId("geometry-meta").textContent =
    geometry.view + " " + planes.length + " layers of " + geometryLayerCount() +
    ", " + total + " paths" + undrawn + " - lines between rings are inferred travel";
}

/** Draw the continuous polyline of a spiral run. */
function renderPathView(svg, ordered) {
  const drawn = HsBaHttp.projectedPathPolyline(geometry.path.xyz, geometryAngles());
  if (!drawn.d) {
    return;
  }
  geometry.base = HsBaHttp.projectedViewBox(drawn.points);
  svg.setAttribute("viewBox", HsBaHttp.currentViewBox(geometry.base, geometry.zoom, geometry.pan));
  svg.replaceChildren();
  drawGeometryGroup(svg, [{ className: "geometry-path", items: [{ d: drawn.d, points: drawn.points }], start: 0 }],
    ordered ? 1 : 0);
  byId("geometry-stage").hidden = false;

  const range = geometry.path.zRange || [];
  byId("geometry-meta").textContent =
    "path " + geometry.path.count + " points, z " + (range[0] || 0) + " - " + (range[1] || 0) +
    " mm - one uninterrupted line, the arrow marks where the run was heading";
}

/** Redraw the selected view from what is already fetched - no request, no re-parse. */
function renderGeometry() {
  const ordered = byId("geometry-order").checked;
  const svg = byId("geometry-svg");
  // Only the coloured drawing may override the category colours, so it is the class that switches.
  if (ordered) {
    svg.setAttribute("class", "geometry-ordered");
  } else {
    svg.removeAttribute("class");
  }
  switch (geometry.view) {
    case "path":
      if (geometry.path.loaded) {
        renderPathView(svg, ordered);
      }
      return;
    case "stack":
    case "side":
      if (geometry.stack) {
        renderStackView(svg, ordered);
      }
      return;
    default:
      if (geometry.base) {
        renderLayerView(svg, ordered);
      }
  }
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
  // The side view is only ever shown from two faces, and a fit that keeps the hidden second face
  // selected would leave the button bar and the drawing disagreeing about which one is on screen.
  geometry.sideFace = 0;
  geometry.turn = 0;
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

/** Slide the viewBox by the pointer delta, in the viewBox's own (already mirrored) space. */
function panDrag(dx, dy) {
  const perPixel = geometry.base.w / geometry.zoom / stageWidth();
  geometry.pan.x -= dx * perPixel;
  geometry.pan.y -= dy * perPixel;
}

/**
 * Move the view with the pointer.
 *
 * What a drag means depends on what is on screen: a single cross-section has nothing to rotate, so
 * it pans, and that is also what Shift gives everywhere else. A projection turns instead, because
 * panning a fixed-perspective drawing by eye is useless while spinning it is informative. The pan
 * lives in the viewBox's own (already mirrored) space, so both axes follow the pointer and the flip
 * ringPath applies to the coordinates never has to be repeated here.
 */
function moveDrag(event) {
  if (!drag || !geometry.base) {
    return;
  }
  const x = Number(event.clientX) || 0;
  const y = Number(event.clientY) || 0;
  const dx = x - drag.x;
  const dy = y - drag.y;
  drag = { x: x, y: y };
  if (event.shiftKey || geometry.view === "layer") {
    panDrag(dx, dy);
    run(renderGeometry);
    return;
  }
  if (geometry.view === "stack") {
    geometry.angles.yaw += dx * 0.01;
    geometry.angles.pitch = Math.max(0, Math.min(Math.PI / 2, geometry.angles.pitch - dy * 0.01));
    run(renderGeometry);
    return;
  }
  // The side view is an orthographic elevation, and every angle between two faces shows the same
  // silhouette: the drag steps between the faces instead of inventing a third projection.
  geometry.turn += dx;
  const steps = Math.trunc(geometry.turn / SIDE_FACE_PIXELS);
  if (steps) {
    const faces = HsBaHttp.GEOMETRY_SIDE_YAWS.length;
    geometry.sideFace = ((geometry.sideFace + steps) % faces + faces) % faces;
    geometry.turn -= steps * SIDE_FACE_PIXELS;
  }
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
  byId("kind").addEventListener("change", () => run(syncThumbnailOption));
  byId("submit").addEventListener("click", () => run(submitJob));
  byId("jobs-refresh").addEventListener("click", () => run(async () => {
    await refreshJobs();
    await refreshUploads();
  }));
  byId("uploads-refresh").addEventListener("click", () => run(refreshUploads));
  byId("preview-clear").addEventListener("click", () => run(clearPreview));
  byId("package-browse").addEventListener("click", () => run(() => browsePackage(previewedJob.id)));
  byId("package-prev").addEventListener("click", () => run(() => stepPackage(-1)));
  byId("package-next").addEventListener("click", () => run(() => stepPackage(1)));
  clearPreview();
  syncThumbnailOption();

  byId("geometry-prev").addEventListener("click", () => run(() => stepGeometry(-1)));
  byId("geometry-next").addEventListener("click", () => run(() => stepGeometry(1)));
  byId("geometry-reload").addEventListener("click", () => run(reloadGeometry));
  byId("geometry-fit").addEventListener("click", () => run(resetGeometryView));
  byId("geometry-layer").addEventListener("input", () => run(() => showGeometryLayer(byId("geometry-layer").value)));
  for (const group of GEOMETRY_GROUPS) {
    byId(group.toggle).addEventListener("click", () => run(renderGeometry));
  }
  // One button per way of looking at the same snapshot, plus the switch that colours them by order.
  for (const view of HsBaHttp.GEOMETRY_VIEWS) {
    byId("geometry-view-" + view).addEventListener("click", () => run(() => selectGeometryView(view)));
  }
  byId("geometry-order").addEventListener("click", () => run(renderGeometry));
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
