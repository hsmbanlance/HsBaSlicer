"use strict";

/**
 * @file httphelpers.js
 * @brief DOM-free helpers behind the REST console's result download, preview and layer geometry.
 *
 * They turn HTTP metadata into a file name, a bounded text view and a set of SVG path strings, so
 * they can be unit tested without a browser: webui/test/server.test.js evaluates this file in a
 * node:vm sandbox and calls the functions directly. The page loads it as a plain script before
 * /app.js and reads the same global object.
 *
 * @author HsBa
 */

/** @brief Longest result text the console renders before pointing at the download. */
const PREVIEW_LIMIT_BYTES = 64 * 1024;

/** @brief Decimals kept in an SVG path coordinate: 0.001 mm, far below any printer's resolution. */
const GEOMETRY_ROUND_DIGITS = 3;

/** @brief Fraction of the longer layer side added around the view, so strokes are not clipped. */
const GEOMETRY_VIEW_PAD = 0.02;

/** @brief viewBox used when there is nothing sane to look at yet. */
const GEOMETRY_EMPTY_VIEW = "0 0 1 1";

/** @brief Path nodes one layer may add: thousands of tiny paths, not megabytes of text, is what
 *  makes a browser crawl, so the console draws a budgeted subset and says how many it left out. */
const GEOMETRY_MAX_PATHS = 3000;

/** @brief Zoom bounds that keep a layer both recognizable and inspectable. */
const GEOMETRY_MIN_ZOOM = 0.05;
const GEOMETRY_MAX_ZOOM = 40;

/**
 * @brief Extension the console adds when it has to name a result after the job id.
 *
 * Every pipeline that writes a file reports that file, so the server normally sends its name in
 * Content-Disposition; this table only covers the fallback (and the kinds answering inline).
 * It mirrors what each pipeline produces: G-code for FDM, a package for the image and hatch
 * kinds, plain text for the robot path and for a Lua script's return value. filetransfer moves
 * other people's files and has no result of its own, so it stays unlisted.
 */
const RESULT_EXTENSIONS = {
  fdm: "gcode",
  sla: "zip",
  sls: "zip",
  slm: "zip",
  lom: "zip",
  tdp: "zip",
  waam: "txt",
  custom: "txt",
};

/** @brief Last path component, so a leaked directory cannot break the save dialog. */
function baseName(value) {
  return value.split(/[\\/]/).pop().trim();
}

/**
 * @brief File name a browser should save a job result under.
 * @param {string} disposition Content-Disposition value of the response ("" when absent).
 * @param {string} fallback Name to use when the header carries nothing usable.
 * @returns {string} A single path component, never empty when @p fallback is set.
 *
 * The REST server advertises the bare file name, but an RFC 5987 `filename*` (which carries
 * non-ASCII names) wins when present, and an unquoted or path-bearing value is normalized
 * rather than trusted.
 */
function filenameFromDisposition(disposition, fallback) {
  const header = disposition || "";

  const extended = /filename\*[^']*'[^']*'([^;]+)/i.exec(header);
  if (extended) {
    try {
      const decoded = baseName(decodeURIComponent(extended[1].trim()));
      if (decoded) {
        return decoded;
      }
    } catch (malformed) {
      // Percent-decoding of a broken escape falls through to the plain filename.
    }
  }

  const quoted = /filename\s*=\s*"([^"]*)"/i.exec(header);
  const plain = quoted ? quoted[1] : (/filename\s*=\s*([^;]+)/i.exec(header) || [])[1] || "";
  return baseName(plain.replace(/^"|"$/g, "")) || fallback || "";
}

/**
 * @brief Download name for one finished job.
 * @param {string} disposition Content-Disposition value of the result response.
 * @param {string} jobId Job id, used when the server sent no name (inline results).
 * @param {string} kind Pipeline kind, so a result without a server name still gets its extension.
 */
function resultFilename(disposition, jobId, kind) {
  const named = filenameFromDisposition(disposition, "");
  if (named) {
    return named;
  }
  const extension = RESULT_EXTENSIONS[kind];
  return extension ? jobId + "." + extension : jobId;
}

/** @brief Byte length of a string in the UTF-8 encoding the console sends and shows. */
function byteLength(text) {
  return new TextEncoder().encode(text).length;
}

/**
 * @brief Whether a result body is worth rendering as text instead of only offering a download.
 * @param {string} contentType Content-Type value of the result response.
 */
function isPreviewable(contentType) {
  const type = (contentType || "").split(";")[0].trim().toLowerCase();
  return (
    type.startsWith("text/") ||
    type === "application/json" ||
    type === "application/xml" ||
    type.endsWith("+json") ||
    type.endsWith("+xml")
  );
}

/**
 * @brief Cut a result body down to the preview budget, keeping the totals of the whole file.
 * @param {string} text Complete response body.
 * @param {number} limitBytes Budget for the returned text (defaults to PREVIEW_LIMIT_BYTES).
 * @returns {{text: string, truncated: boolean, bytes: number, shownBytes: number, lines: number}}
 *
 * G-code from a real model is far larger than a panel should hold, so the view reports what was
 * hidden; the byte budget is honored by dropping half the remaining text at a time and a trailing
 * high surrogate is removed so a multi-byte character is never split in half.
 */
function previewText(text, limitBytes) {
  const limit = limitBytes || PREVIEW_LIMIT_BYTES;
  const body = text || "";
  const bytes = byteLength(body);
  const lines = body.length ? body.split("\n").length : 0;
  if (bytes <= limit) {
    return { text: body, truncated: false, bytes: bytes, shownBytes: bytes, lines: lines };
  }

  let cut = Math.min(body.length, limit);
  while (cut > 0 && byteLength(body.slice(0, cut)) > limit) {
    cut = Math.floor(cut / 2);
  }
  const shown = body.slice(0, cut).replace(/[\uD800-\uDBFF]$/, "");
  return {
    text: shown,
    truncated: true,
    bytes: bytes,
    shownBytes: byteLength(shown),
    lines: shown.length ? shown.split("\n").length : 0,
  };
}

/** @brief Number rounded for geometry output, with -0 folded onto 0 so no path reads "M -0 5". */
function roundCoord(value) {
  const number = Number(value);
  if (!Number.isFinite(number)) {
    return 0;
  }
  const rounded = Number(number.toFixed(GEOMETRY_ROUND_DIGITS));
  return rounded === 0 ? 0 : rounded;
}

/** @brief Same job at viewBox precision: a zoomed box must not gain spurious digits. */
function roundView(value) {
  const rounded = Number(Number(value).toFixed(4));
  return rounded === 0 ? 0 : rounded;
}

/** @brief The four finite bounds of a geometry response, or null when the body carries none. */
function finiteBounds(bounds) {
  const box = bounds || [];
  if (box.length < 4) {
    return null;
  }
  for (let at = 0; at < 4; at += 1) {
    if (!Number.isFinite(Number(box[at]))) {
      return null;
    }
  }
  return [Number(box[0]), Number(box[1]), Number(box[2]), Number(box[3])];
}

/**
 * @brief Line to mirror a layer's y coordinates about.
 * @param {number[]} bounds min_x, min_y, max_x, max_y of one layer or of the whole job.
 * @returns {number} The bounds' mid-height, 0 when there are no usable bounds.
 *
 * A printer's y axis grows towards the far side of the bed while an SVG page grows downwards, so
 * the console draws every ring mirrored about the mid-height. Mirroring about that line maps
 * [min_y, max_y] onto itself, which is what lets the viewBox keep using the unmirrored bounds.
 */
function mirrorLine(bounds) {
  const box = finiteBounds(bounds);
  return box ? (box[1] + box[3]) / 2 : 0;
}

/** @brief Zoom factor clamped into [GEOMETRY_MIN_ZOOM, GEOMETRY_MAX_ZOOM]; junk means 1:1. */
function clampZoom(value) {
  const number = Number(value);
  if (!Number.isFinite(number)) {
    return 1;
  }
  return Math.min(GEOMETRY_MAX_ZOOM, Math.max(GEOMETRY_MIN_ZOOM, number));
}

/**
 * @brief SVG path data for one contour/infill/support ring.
 * @param {{point_count: number, closed: number, xy: number[]}} ring Ring as the API reports it.
 * @param {number} mirrorY Line to flip y about (see @ref mirrorLine).
 * @returns {string} "M x y L x y ..." plus " Z" for a closed ring, "" when there is nothing to draw.
 *
 * xy is a flat alternating array whose length the server does not send, so point_count is what
 * decides how many vertices to read: a ring it reports as empty stays empty here even if a stray
 * coordinate showed up, and a count claiming more pairs than the body carries is clamped down to
 * the pairs that are really there rather than drawing half a vertex.
 */
function ringPath(ring, mirrorY) {
  const xy = (ring && ring.xy) || [];
  const pairs = Math.floor(xy.length / 2);
  const declared = Number(ring && ring.point_count);
  const count = Number.isFinite(declared) ? Math.min(Math.max(Math.trunc(declared), 0), pairs) : pairs;
  if (count < 2) {
    return "";
  }
  const flip = Number.isFinite(Number(mirrorY)) ? Number(mirrorY) : 0;
  const parts = [];
  for (let at = 0; at < count; at += 1) {
    const x = roundCoord(xy[2 * at]);
    const y = roundCoord(2 * flip - Number(xy[2 * at + 1]));
    parts.push((at === 0 ? "M " : " L ") + x + " " + y);
  }
  return ring.closed ? parts.join("") + " Z" : parts.join("");
}

/**
 * @brief viewBox of a layer: its bounds padded, centered on itself.
 * @param {number[]} bounds min_x, min_y, max_x, max_y.
 * @returns {{x: number, y: number, w: number, h: number, cx: number, cy: number}}
 *
 * A layer that is a single point would otherwise give a zero-size viewBox, which no renderer can
 * show, so an empty extent falls back to one millimetre around the point.
 */
function geometryView(bounds) {
  const box = finiteBounds(bounds);
  if (!box) {
    return { x: 0, y: 0, w: 1, h: 1, cx: 0.5, cy: 0.5 };
  }
  const width = Math.max(0, box[2] - box[0]);
  const height = Math.max(0, box[3] - box[1]);
  const pad = GEOMETRY_VIEW_PAD * Math.max(width, height);
  const w = (width > 0 ? width : 1) + 2 * pad;
  const h = (height > 0 ? height : 1) + 2 * pad;
  const cx = (box[0] + box[2]) / 2;
  const cy = (box[1] + box[3]) / 2;
  return { x: roundView(cx - w / 2), y: roundView(cy - h / 2), w: roundView(w), h: roundView(h), cx: cx, cy: cy };
}

/**
 * @brief viewBox string actually applied to the SVG of one layer.
 * @param {object} base @ref geometryView of the layer being shown.
 * @param {number} zoom Zoom factor (1 = fit); clamped by @ref clampZoom.
 * @param {{x: number, y: number}} pan Offset of the view center in millimetres.
 * @returns {string} "min-x min-y width height".
 *
 * pan is expressed in the viewBox's own (already mirrored) space, so dragging the pointer right
 * moves the view left and the handler needs no second y flip.
 */
function currentViewBox(base, zoom, pan) {
  if (!base || !Number.isFinite(base.w) || !Number.isFinite(base.h)) {
    return GEOMETRY_EMPTY_VIEW;
  }
  const factor = clampZoom(zoom);
  const shift = (value) => (Number.isFinite(Number(value)) ? Number(value) : 0);
  const w = roundView(base.w / factor);
  const h = roundView(base.h / factor);
  const cx = roundView(base.cx + shift(pan && pan.x));
  const cy = roundView(base.cy + shift(pan && pan.y));
  return roundView(cx - w / 2) + " " + roundView(cy - h / 2) + " " + w + " " + h;
}

/**
 * @brief Path data of one layer, split by category and cut to the node budget.
 * @param {object} layer LayerGeometryView as the API reports it.
 * @param {number} mirrorY Line to flip y about.
 * @returns {{outlines: string[], fills: string[], supports: string[],
 *            counts: {outlines: number, fills: number, supports: number, dropped: number}}}
 *
 * counts are the rings the layer really holds and dropped how many were left undrawn; the budget
 * is handed out outlines then supports then fills, because the infill of a real part is the bulk
 * of a layer and the least informative part of it.
 */
function layerPaths(layer, mirrorY) {
  const drawn = { outlines: [], fills: [], supports: [] };
  const counts = { outlines: 0, fills: 0, supports: 0, dropped: 0 };
  let budget = GEOMETRY_MAX_PATHS;
  for (const key of ["outlines", "supports", "fills"]) {
    const rings = (layer && layer[key]) || [];
    for (const ring of rings) {
      const d = ringPath(ring, mirrorY);
      if (!d) {
        continue;
      }
      counts[key] += 1;
      if (budget > 0) {
        drawn[key].push(d);
        budget -= 1;
      } else {
        counts.dropped += 1;
      }
    }
  }
  return { outlines: drawn.outlines, fills: drawn.fills, supports: drawn.supports, counts: counts };
}

globalThis.HsBaHttp = {
  PREVIEW_LIMIT_BYTES: PREVIEW_LIMIT_BYTES,
  GEOMETRY_ROUND_DIGITS: GEOMETRY_ROUND_DIGITS,
  GEOMETRY_VIEW_PAD: GEOMETRY_VIEW_PAD,
  GEOMETRY_EMPTY_VIEW: GEOMETRY_EMPTY_VIEW,
  GEOMETRY_MAX_PATHS: GEOMETRY_MAX_PATHS,
  GEOMETRY_MIN_ZOOM: GEOMETRY_MIN_ZOOM,
  GEOMETRY_MAX_ZOOM: GEOMETRY_MAX_ZOOM,
  roundCoord: roundCoord,
  mirrorLine: mirrorLine,
  clampZoom: clampZoom,
  ringPath: ringPath,
  geometryView: geometryView,
  currentViewBox: currentViewBox,
  layerPaths: layerPaths,
  RESULT_EXTENSIONS: RESULT_EXTENSIONS,
  filenameFromDisposition: filenameFromDisposition,
  resultFilename: resultFilename,
  isPreviewable: isPreviewable,
  previewText: previewText,
};
