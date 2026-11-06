"use strict";

/**
 * @file model.js
 * @brief MVC Model of the REST console: the client-side state and every read of the /api/* surface.
 *
 * This layer holds the data the console keeps about the current session (the geometry snapshot, the
 * packed result being browsed, the result on screen, the jobs/upload polling coordination) and the
 * functions that fetch it from the REST server. It touches no DOM and never renders: a View reads
 * this state and a Controller drives it, but the Model knows nothing about either. It is loaded
 * before view.js and app.js and publishes its surface on globalThis.HsBaApp.Model.
 */

(function () {
  const TOKEN_KEY = "hsba.console.token";

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

  /**
   * Which packed result the Images strip is showing, and how far into it the operator got.
   *
   * The entry list is fetched once and stepped through locally: a layer browser that asked the server
   * for the listing again on every click would be slower and would look like a bug when a package is
   * large.
   */
  const pack = { id: "", entries: [], index: -1, loaded: false };

  /** @brief The result currently on screen, so Images can ask for the package of the right job. */
  const previewedJob = { id: "", kind: "" };

  // Per-list monotonic generation counter: every refresh owns a slot, and a fetch whose slot has been
  // retaken by a newer refresh drops out before rendering. Together with the polling inFlight guard
  // below this eliminates both request stacking and "old response overwrites new render" flickers.
  const refreshSeq = { jobs: 0, uploads: 0 };

  // One self-scheduling loop drives the jobs + uploads auto-refresh: the next tick is queued only
  // after the previous one finishes, so a slow backend cannot stack requests, and a 401 pauses the
  // loop until the API key is (re-)saved. The inFlight flag is belt-and-suspenders for any external
  // caller that might kick a tick before the previous one unwound.
  const jobsPolling = { inFlight: false, paused: false, timer: 0 };

  function storedToken() {
    return localStorage.getItem(TOKEN_KEY) || "";
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
      const err = new Error(failureText(response.status, text, path));
      // Expose the HTTP status so callers can branch on 401 without parsing the message. Existing
      // consumers keep seeing the same thrown Error with the same message text.
      err.status = response.status;
      throw err;
    }
    return { response: response, text: text };
  }

  async function callJson(path, options) {
    const result = await call(path, options);
    return result.text ? JSON.parse(result.text) : null;
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

  /** @brief URL of one geometry endpoint of one job. */
  function geometryPath(id, suffix) {
    return "/api/jobs/" + encodeURIComponent(id) + suffix;
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

  globalThis.HsBaApp = globalThis.HsBaApp || {};
  globalThis.HsBaApp.Model = {
    TOKEN_KEY: TOKEN_KEY,
    geometry: geometry,
    pack: pack,
    previewedJob: previewedJob,
    refreshSeq: refreshSeq,
    jobsPolling: jobsPolling,
    storedToken: storedToken,
    failureText: failureText,
    call: call,
    callJson: callJson,
    fetchResult: fetchResult,
    geometryPath: geometryPath,
    geometryLayerCount: geometryLayerCount,
    hasContinuousPath: hasContinuousPath,
    geometryAngles: geometryAngles,
  };
})();
