"use strict";

/**
 * @file app.js
 * @brief MVC Controller of the REST console: the orchestration and event wiring around the Model.
 *
 * The page is served by webui/server.js, which proxies /api/* to the REST server, so all requests
 * here are same-origin and need no CORS support on the C++ side. Client-side state and every read of
 * that surface live in model.js (HsBaApp.Model); everything that turns state into DOM lives in
 * view.js (HsBaApp.View). This file is what ties them together: it decides what a click means, drives
 * the polling loop, and binds the page, publishing the two helpers and the two action tables the View
 * reaches back into on globalThis.HsBaApp.Controller. It is loaded last, after model.js and view.js.
 */

(function () {
  const Model = globalThis.HsBaApp.Model;
  const View = globalThis.HsBaApp.View;

  // Session state and the /api/* reads live in the Model; every DOM-facing helper lives in the View.
  // Naming them once here lets the orchestration functions below keep their original bodies verbatim.
  const TOKEN_KEY = Model.TOKEN_KEY;
  const geometry = Model.geometry;
  const pack = Model.pack;
  const previewedJob = Model.previewedJob;
  const refreshSeq = Model.refreshSeq;
  const jobsPolling = Model.jobsPolling;
  const storedToken = Model.storedToken;
  const call = Model.call;
  const callJson = Model.callJson;
  const fetchResult = Model.fetchResult;
  const geometryPath = Model.geometryPath;
  const geometryLayerCount = Model.geometryLayerCount;
  const hasContinuousPath = Model.hasContinuousPath;

  const byId = View.byId;
  const tr = View.tr;
  const writeLog = View.writeLog;
  const formatBytes = View.formatBytes;
  const GEOMETRY_GROUPS = View.GEOMETRY_GROUPS;
  const tableFor = View.tableFor;
  const buildJobRow = View.buildJobRow;
  const buildUploadRow = View.buildUploadRow;
  const renderGcodeThumbnails = View.renderGcodeThumbnails;
  const clearThumbnails = View.clearThumbnails;
  const setPackageMessage = View.setPackageMessage;
  const clearPackage = View.clearPackage;
  const showPackageImage = View.showPackageImage;
  const setGeometryMessage = View.setGeometryMessage;
  const setGeometryControlsEnabled = View.setGeometryControlsEnabled;
  const markGeometryView = View.markGeometryView;
  const stageWidth = View.stageWidth;
  const renderGeometry = View.renderGeometry;

  const HEALTH_EVERY_MS = 5000;
  const JOBS_EVERY_MS = 3000;
  const JOB_COLUMNS = 8;
  const UPLOAD_COLUMNS = 8;

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

  async function copyText(value) {
    if (!navigator.clipboard) {
      throw new Error(tr("clipboard unavailable: open the console on localhost or over https"));
    }
    await navigator.clipboard.writeText(value);
    writeLog(tr("copied") + " " + value, "ok");
  }

  /** Run an action, reporting failures in the log panel instead of the browser console. */
  function run(action) {
    Promise.resolve()
      .then(action)
      .catch((error) => writeLog(error.message, "error"));
  }

  async function refreshHealth() {
    const badge = byId("health");
    try {
      const health = await callJson("/api/health");
      badge.textContent = tr("ok") + " - " + health.uptime_seconds + " s";
      badge.className = "badge badge-ok";
    } catch (error) {
      badge.textContent = error.message.indexOf("HTTP 401") === 0 ? tr("token required") : tr("unreachable");
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
      throw new Error(tr("the REST server reported no pipeline kinds"));
    }
    // Filling the select picks its first option without any change event reaching the page, so the
    // preview-image box has to be re-evaluated here rather than only when the operator switches kind.
    syncThumbnailOption();
  }

  async function loadDefaultConfig() {
    const kind = byId("kind").value;
    if (!kind) {
      throw new Error(tr("no pipeline selected"));
    }
    const result = await call("/api/config/default/" + encodeURIComponent(kind) + "?format=json");
    byId("config").value = result.text;
    writeLog(tr("loaded the default config of") + " " + kind, "ok");
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
      throw new Error(tr("no pipeline selected"));
    }
    if (!config) {
      throw new Error(tr("load a default config (or paste one) before submitting"));
    }

    let body = config;
    if (kind === "fdm") {
      body = HsBaHttp.applyConfigFlags(config, { gcode_thumbnails: byId("fdm-thumbnails").checked ? 1 : 0 });
      if (body === config) {
        // A config the console cannot parse is a config it cannot patch, and the operator deserves to
        // know the box had no effect rather than to wonder why the result carries no pictures.
        writeLog(tr("config is not JSON - gcode_thumbnails not applied"), "info");
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
    writeLog(tr("submitted") + " " + job.id + " (" + kind + ")", "ok");
    await refreshJobs();
  }

  async function refreshJobs() {
    // A newer refresh has taken over while this one awaited the network, so its result is stale and
    // applying it would flicker the table back to a past snapshot. The counter is bumped here and
    // checked after each await so manual clicks and the auto-tick can share one invalidation path.
    const my = ++refreshSeq.jobs;
    const listing = await callJson("/api/jobs?limit=50");
    if (my !== refreshSeq.jobs) return;
    const jobs = (listing && listing.jobs) || [];
    tableFor("jobs-body", JOB_COLUMNS).render(jobs, buildJobRow, tr("no jobs yet"));
    // A package strip belongs to a job; once that job leaves the list its result may be gone, and a
    // strip that keeps pointing at a deleted archive is worse than one that says it stopped.
    if (pack.loaded && !jobs.some((job) => job.id === pack.id)) {
      clearPackage();
    }
  }

  async function refreshUploads() {
    const my = ++refreshSeq.uploads;
    let uploads = [];
    let notice = tr("no uploaded models");
    try {
      const listing = await callJson("/api/uploads?limit=50");
      if (my !== refreshSeq.uploads) return;
      uploads = (listing && listing.uploads) || [];
    } catch (error) {
      if (my !== refreshSeq.uploads) return;
      // An upload pool that is switched off is a state worth showing, not a failed action.
      notice = error.message;
    }
    tableFor("uploads-body", UPLOAD_COLUMNS).render(uploads, buildUploadRow, notice);
  }

  async function jobsPollTick() {
    if (jobsPolling.inFlight || jobsPolling.paused) return;
    jobsPolling.inFlight = true;
    try {
      if (byId("jobs-auto").checked) {
        await refreshJobs();
        await refreshUploads();
      }
    } catch (error) {
      if (error && error.status === 401) {
        jobsPolling.paused = true;
        writeLog(tr("jobs auto-refresh paused: unauthorized (401); save an API key to resume"), "error");
        return;
      }
      writeLog(error.message, "error");
    } finally {
      jobsPolling.inFlight = false;
    }
    jobsPolling.timer = window.setTimeout(jobsPollTick, JOBS_EVERY_MS);
  }

  function startJobsPolling() {
    // Skip the clear when there is nothing to clear: a fresh module and a test stub that returns 0
    // from setTimeout would otherwise hand clearTimeout an id it has never issued.
    if (jobsPolling.timer) window.clearTimeout(jobsPolling.timer);
    jobsPolling.timer = window.setTimeout(jobsPollTick, JOBS_EVERY_MS);
  }

  /** @brief Clear the 401 pause and re-arm the loop; called when the API token is saved or cleared. */
  function resumeJobsPolling() {
    if (!jobsPolling.paused) return;
    jobsPolling.paused = false;
    writeLog(tr("jobs auto-refresh resumed after token change"), "ok");
    startJobsPolling();
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
    writeLog(tr("downloaded") + " " + name + " (" + formatBytes(blob.size) + ")", "ok");
  }

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

    previewedJob.id = id;
    previewedJob.kind = kind;
    clearThumbnails();
    // Both picture strips belong to the result on screen, so both have to be retired with it: leaving
    // the package strip up would show one job's layer under another job's heading.
    clearPackage();
    byId("preview-title").textContent = tr("Result preview") + " - " + name;
    if (!HsBaHttp.isPreviewable(contentType)) {
      box.hidden = true;
      box.textContent = "";
      meta.textContent =
        tr("{size} of {type}, not text - use Result to download it",
          { size: formatBytes(blob.size), type: contentType || tr("unknown type") });
      writeLog(tr("preview needs a download instead:") + " " + name + " " + tr("is") + " " +
        (contentType || tr("binary")), "info");
      return;
    }

    const body = await blob.text();
    const view = HsBaHttp.previewText(body);
    box.textContent = view.text;
    box.hidden = false;
    const blocks = renderGcodeThumbnails(body);
    meta.textContent =
      formatBytes(view.bytes) + ", " + view.lines + " " + tr("lines") +
      (view.truncated
        ? " - " + tr("showing the first") + " " + formatBytes(view.shownBytes) + ", " +
          tr("use Result for the whole file")
        : "") +
      (kind === "fdm" && blocks === 0
        ? " - " + tr("no embedded previews, set gcode_thumbnails to 1")
        : "");
    writeLog(tr("previewed") + " " + name + " (" + formatBytes(view.shownBytes) + " of " +
      formatBytes(view.bytes) + ")", "ok");
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
      writeLog(tr("preview a result first - Images works on the job whose result is on screen"), "info");
      return;
    }
    let listing;
    try {
      listing = await callJson("/api/jobs/" + encodeURIComponent(id) + "/package");
    } catch (error) {
      setPackageMessage(
        /result is not a package/.test(error.message)
          ? tr("this result is a single file, not a package")
          : tr("no package to read:") + " " + error.message,
      );
      // The server's own sentence belongs in the log, which is what run() writes for a rejected call.
      throw error;
    }
    pack.id = (listing && listing.id) || id;
    pack.entries = HsBaHttp.packageImages((listing && listing.entries) || []);
    pack.loaded = true;
    if (!pack.entries.length) {
      setPackageMessage(tr("package has no images - SLA exports PNG/JPG/SVG layers"));
      writeLog(tr("package of") + " " + pack.id + " " + tr("holds no images"), "info");
      return;
    }
    showPackageImage(0);
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
    byId("preview-title").textContent = tr("Result preview");
    byId("preview-meta").textContent = tr("no result previewed yet");
    previewedJob.id = "";
    previewedJob.kind = "";
    clearThumbnails();
    clearPackage();
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
      const refusal = tr("this run deposited no continuous path - spiral_mode=1 belongs to fdm, tdp and waam");
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
        setGeometryMessage(tr("this job holds no layer geometry - submit it with capture geometry enabled"));
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
      writeLog(tr("opened the geometry of") + " " + id + " (" + count + " " + tr("layers") + ", " + job.kind + ")", "ok");
    } catch (error) {
      setGeometryMessage(error.message + " - " + tr("the snapshot pool keeps only the last few jobs"));
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
      setGeometryMessage(error.message + " - " + tr("spiral runs only"));
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
    writeLog(tr("deleted") + " " + id, "ok");
    await refreshJobs();
  }

  async function deleteUpload(id) {
    await call("/api/uploads/" + encodeURIComponent(id), { method: "DELETE" });
    writeLog(tr("deleted") + " " + id, "ok");
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
      tr("cancel requested for") + " " + id + ": " + (view.detail || tr("accepted")) + "; " +
        tr("the job stops at its next safe point"),
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
    // A token edit is the only supported way out of the 401 pause; re-arm the loop here so the user
    // does not have to reload the page after providing a fresh key.
    resumeJobsPolling();
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
    setGeometryMessage(tr("no layer geometry previewed yet"));
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
    // A self-scheduling tick replaces the old setInterval for jobs/uploads: overlapping requests on
    // a slow backend are impossible now, and 401 stops the loop until a fresh token is saved.
    startJobsPolling();
  }

  globalThis.HsBaApp = globalThis.HsBaApp || {};
  globalThis.HsBaApp.Controller = {
    run: run,
    copyText: copyText,
    JOB_ACTIONS: JOB_ACTIONS,
    UPLOAD_ACTIONS: UPLOAD_ACTIONS,
  };

  document.addEventListener("DOMContentLoaded", init);
})();
