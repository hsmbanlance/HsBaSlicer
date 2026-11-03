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

/* ---------------------------------------------------------------------------
 * Orthographic projection helpers (stack / side / path views)
 *
 * The console has no 3D library and no intent to grow one, so a layer stack is drawn as two
 * fixed orthographic projections. projectPoint rotates a model point about the printer z axis
 * by yaw, then tilts the view down by pitch and keeps the horizontal screen axis plus the
 * raised one. The result stays in model space (v grows upwards); the path builders below flip
 * v because an SVG page grows downwards.
 * ------------------------------------------------------------------------- */

/** @brief The four ways the geometry panel can show one and the same snapshot. */
const GEOMETRY_VIEWS = ["layer", "stack", "side", "path"];

/** @brief Layers the stack and side views sample at most, however many a job has. */
const GEOMETRY_STACK_LAYERS = 12;

/** @brief Isometric-ish looking angle of the stack view, chosen so a short cylinder reads as one. */
const GEOMETRY_STACK_ANGLES = { yaw: -0.6155, pitch: 0.5586 };

/** @brief Front and side yaw of the side view; dragging swaps between the two. */
const GEOMETRY_SIDE_YAWS = [0, Math.PI / 2];

/** @brief Half-length of a direction arrow as a fraction of the visible box width. */
const GEOMETRY_ARROW_HALF = 0.006;

/**
 * @brief Project one model point onto a viewing plane.
 * @param {number} x Model x (mm).
 * @param {number} y Model y (mm).
 * @param {number} z Model z (mm).
 * @param {number} yaw Rotation about the z axis (radians).
 * @param {number} pitch Tilt of the camera towards the plane (radians, 0 = level, pi/2 = top down).
 * @returns {{u: number, v: number}} Screen axes in model space: v grows upwards and away.
 *
 * With pitch pi/2 the camera looks straight down and the plane is the build plate
 * ({u: x, v: -y}); with pitch 0 it looks along y and the plane is the XZ elevation ({u: x, v: z}).
 */
function projectPoint(x, y, z, yaw, pitch) {
  const ca = Math.cos(Number(yaw) || 0);
  const sa = Math.sin(Number(yaw) || 0);
  const cp = Math.cos(Number(pitch) || 0);
  const sp = Math.sin(Number(pitch) || 0);
  const modelX = Number(x) || 0;
  const modelY = Number(y) || 0;
  const modelZ = Number(z) || 0;
  const hx = modelX * ca + modelY * sa;
  const hy = -modelX * sa + modelY * ca;
  return { u: roundCoord(hx), v: roundCoord(-hy * sp + modelZ * cp) };
}

/** @brief How many points of one layer the stack view may draw before it starts dropping. */
function stackBudget(layerCount) {
  const layers = Math.max(1, Math.trunc(Number(layerCount)) || 1);
  return Math.max(1, Math.floor(GEOMETRY_MAX_PATHS / layers));
}

/**
 * @brief Bounding box of projected points, as an SVG viewBox.
 * @param {{u: number, v: number}[]} points Projected points of everything that will be drawn.
 * @param {number} padRatio Fraction of the longer side to add (defaults to GEOMETRY_VIEW_PAD).
 * @returns {{d: string, x: number, y: number, w: number, h: number, cx: number, cy: number}}
 *
 * The y half of the box is built from negated v, because the paths this box frames are emitted
 * in SVG space; a degenerate extent falls back to one millimetre so a single-point view still
 * renders, and no points at all give the same empty view the layer view uses.
 */
function projectedViewBox(points, padRatio) {
  const list = points || [];
  if (!list.length) {
    return { d: GEOMETRY_EMPTY_VIEW, x: 0, y: 0, w: 1, h: 1, cx: 0.5, cy: 0.5 };
  }
  let minU = Infinity;
  let maxU = -Infinity;
  let minV = Infinity;
  let maxV = -Infinity;
  for (const point of list) {
    const u = Number(point && point.u);
    const v = Number(point && point.v);
    if (!Number.isFinite(u) || !Number.isFinite(v)) {
      continue;
    }
    minU = Math.min(minU, u);
    maxU = Math.max(maxU, u);
    minV = Math.min(minV, v);
    maxV = Math.max(maxV, v);
  }
  if (!Number.isFinite(minU) || !Number.isFinite(minV)) {
    return { d: GEOMETRY_EMPTY_VIEW, x: 0, y: 0, w: 1, h: 1, cx: 0.5, cy: 0.5 };
  }
  const width = Math.max(0, maxU - minU);
  const height = Math.max(0, maxV - minV);
  const ratio = Number.isFinite(Number(padRatio)) ? Number(padRatio) : GEOMETRY_VIEW_PAD;
  const pad = ratio * Math.max(width, height);
  const w = (width > 0 ? width : 1) + 2 * pad;
  const h = (height > 0 ? height : 1) + 2 * pad;
  const cx = (minU + maxU) / 2;
  const cy = (minV + maxV) / 2;
  const x = roundView(cx - w / 2);
  const y = roundView(-cy - h / 2);
  return { d: x + " " + y + " " + roundView(w) + " " + roundView(h), x: x, y: y, w: roundView(w), h: roundView(h), cx: cx, cy: -cy };
}

/** @brief Projected vertices of one ring, or [] when it carries fewer than two usable points. */
function projectedRingPoints(ring, z, angles) {
  const xy = (ring && ring.xy) || [];
  const pairs = Math.floor(xy.length / 2);
  const declared = Number(ring && ring.point_count);
  const count = Number.isFinite(declared) ? Math.min(Math.max(Math.trunc(declared), 0), pairs) : pairs;
  if (count < 2) {
    return [];
  }
  const height = Number(z) || 0;
  const yaw = angles && angles.yaw;
  const pitch = angles && angles.pitch;
  const points = [];
  for (let at = 0; at < count; at += 1) {
    points.push(projectPoint(xy[2 * at], xy[2 * at + 1], height, yaw, pitch));
  }
  return points;
}

/** @brief "M u v L u v ..." of already projected points, in SVG space, " Z" when closed. */
function projectedPolyline(points, closed) {
  if (!points || points.length < 2) {
    return "";
  }
  const parts = [];
  for (let at = 0; at < points.length; at += 1) {
    const u = roundCoord(points[at].u);
    const v = roundCoord(-points[at].v);
    parts.push((at === 0 ? "M " : " L ") + u + " " + v);
  }
  return closed ? parts.join("") + " Z" : parts.join("");
}

/**
 * @brief Path data of one layer seen from a fixed camera, cut to a per-layer budget.
 * @param {object[]} rings Contour/infill/support rings as the API reports them.
 * @param {number} z Platform height every point of those rings sits on.
 * @param {{yaw: number, pitch: number}} angles Camera angles.
 * @param {number} limit How many paths this layer may contribute.
 * @returns {{paths: string[], points: object[][], used: number, dropped: number}}
 *
 * points is aligned with paths so a direction arrow can be drawn per path. Unlike the layer view
 * nothing is mirrored about the printer y axis: the projection already places the model, and
 * dropping the mirror is what keeps the stack's layers in the same relative order.
 */
function projectedRingPaths(rings, z, angles, limit) {
  const budget = Math.max(0, Math.trunc(Number(limit)) || 0);
  const paths = [];
  const points = [];
  let used = 0;
  let dropped = 0;
  for (const ring of rings || []) {
    const vertices = projectedRingPoints(ring, z, angles);
    if (vertices.length < 2) {
      continue;
    }
    used += 1;
    if (used > budget) {
      dropped += 1;
      continue;
    }
    paths.push(projectedPolyline(vertices, ring.closed));
    points.push(vertices);
  }
  return { paths: paths, points: points, used: used, dropped: dropped };
}

/**
 * @brief Path data of a continuous toolpath (GET .../geometry/path).
 * @param {number[]} xyz Flat x, y, z triples as the API reports them.
 * @param {{yaw: number, pitch: number}} angles Camera angles.
 * @returns {{d: string, points: object[]}} Single polyline plus its projected vertices.
 *
 * A trailing remainder of one or two numbers cannot form a point, so it is dropped instead of
 * drawing half a vertex.
 */
function projectedPathPolyline(xyz, angles) {
  const flat = xyz || [];
  const total = Math.floor(flat.length / 3);
  const points = [];
  for (let at = 0; at < total; at += 1) {
    points.push(projectPoint(flat[3 * at], flat[3 * at + 1], flat[3 * at + 2], angles && angles.yaw, angles && angles.pitch));
  }
  return { d: projectedPolyline(points, false), points: points };
}

/**
 * @brief Layer indices to draw when a stack only has room for a handful.
 * @param {number} count Total layers of the job.
 * @param {number} want How many layers may be shown.
 * @returns {{indices: number[], focus: number}} Sampled indices and the one to highlight.
 *
 * The first and the last layer are always taken, because a stack that hides the bottom or the
 * top of a part misrepresents its height; focus is the middle sample, which is where the eye of
 * a viewer reading bottom-up settles.
 */
function sampleLayers(count, want) {
  const total = Math.max(0, Math.trunc(Number(count)) || 0);
  const take = Math.max(1, Math.trunc(Number(want)) || 1);
  if (total === 0) {
    return { indices: [], focus: -1 };
  }
  if (total <= take) {
    const all = [];
    for (let at = 0; at < total; at += 1) {
      all.push(at);
    }
    return { indices: all, focus: all[Math.floor((all.length - 1) / 2)] };
  }
  const indices = [];
  for (let at = 0; at < take; at += 1) {
    const value = at === take - 1 ? total - 1 : Math.round((at * (total - 1)) / (take - 1));
    if (!indices.length || value > indices[indices.length - 1]) {
      indices.push(value);
    }
  }
  return { indices: indices, focus: indices[Math.floor((indices.length - 1) / 2)] };
}

/** @brief Hue range the order ramp walks: cool at the start of the run, warm at its end. */
const GEOMETRY_HUE_START = 200;
const GEOMETRY_HUE_END = 340;

/** @brief HSL to #rrggbb, with the saturation and lightness the order ramp uses. */
function hslToHex(hue, saturation, lightness) {
  const chroma = (1 - Math.abs(2 * lightness - 1)) * saturation;
  const sector = (((hue % 360) + 360) % 360) / 60;
  const second = chroma * (1 - Math.abs((sector % 2) - 1));
  const legs =
    sector < 1
      ? [chroma, second, 0]
      : sector < 2
        ? [second, chroma, 0]
        : sector < 3
          ? [0, chroma, second]
          : sector < 4
            ? [0, second, chroma]
            : sector < 5
              ? [second, 0, chroma]
              : [chroma, 0, second];
  const shift = lightness - chroma / 2;
  return (
    "#" +
    legs
      .map((leg) => Math.round((leg + shift) * 255).toString(16).padStart(2, "0"))
      .join("")
  );
}

/**
 * @brief Color of one deposited element, so the order of a run is readable without animating it.
 * @param {number} index Position inside the sequence.
 * @param {number} total Size of the sequence.
 * @returns {string} "#rrggbb" between GEOMETRY_HUE_START and GEOMETRY_HUE_END.
 */
function sequenceColor(index, total) {
  const size = Math.trunc(Number(total)) || 0;
  const at = Math.trunc(Number(index)) || 0;
  const hue = size <= 1 ? (GEOMETRY_HUE_START + GEOMETRY_HUE_END) / 2 : GEOMETRY_HUE_START + ((GEOMETRY_HUE_END - GEOMETRY_HUE_START) * at) / (size - 1);
  return hslToHex(hue, 0.75, 0.6);
}

/**
 * @brief Small triangle marking where a deposited sequence is heading.
 * @param {{u: number, v: number}[]} points Projected vertices of one polyline.
 * @param {{w: number, h: number}} view Visible box, so the mark keeps its size at any zoom fit.
 * @returns {string[]} One "M ... Z" path, or [] when the polyline has no direction to show.
 *
 * The arrow sits on the longest segment of the polyline: a ring's seam is where the run turned
 * around, and that is the one place a viewer needs the heading spelled out.
 */
function arrowMark(points, view) {
  const list = points || [];
  if (list.length < 2) {
    return [];
  }
  const span = Math.max(Number(view && view.w), Number(view && view.h));
  const half = Number.isFinite(span) && span > 0 ? span * GEOMETRY_ARROW_HALF : 0;
  if (!(half > 0)) {
    return [];
  }
  let best = -1;
  let length = 0;
  for (let at = 1; at < list.length; at += 1) {
    const dx = Number(list[at].u) - Number(list[at - 1].u);
    const dy = Number(list[at].v) - Number(list[at - 1].v);
    const segment = Math.hypot(dx, dy);
    if (segment > length) {
      length = segment;
      best = at;
    }
  }
  if (best < 1 || !(length > 0)) {
    return [];
  }
  const ux = (list[best].u - list[best - 1].u) / length;
  const vy = (list[best].v - list[best - 1].v) / length;
  const mx = (list[best].u + list[best - 1].u) / 2;
  const my = (list[best].v + list[best - 1].v) / 2;
  const tip = { u: mx + ux * half, v: my + vy * half };
  const left = { u: mx - vy * half, v: my + ux * half };
  const right = { u: mx + vy * half, v: my - ux * half };
  return [projectedPolyline([tip, left, right], true)];
}

/* ---------------------------------------------------------------------------
 * Result package entries and G-code preview images
 *
 * Both features read what the engine already puts in a response: a packed export keeps its layer
 * images inside the result archive, and an FDM run can embed projections of the model it sliced as
 * comment blocks at the head of its G-code. Neither needs a decoder here - a picture is handed to an
 * <img> element and the browser does the rest - so everything below only names, orders and bounds.
 * ------------------------------------------------------------------------- */

/** @brief Preview blocks one G-code header may contribute, however many it carries. */
const GCODE_THUMBNAIL_MAX_BLOCKS = 8;

/** @brief Base64 characters one block may contribute before the reader stops collecting. */
const GCODE_THUMBNAIL_MAX_B64 = 4 * 1024 * 1024;

/** @brief Package media types the console can put into an image element. */
const PACKAGE_IMAGE_MEDIA_TYPES = { "image/png": 1, "image/jpeg": 1, "image/svg+xml": 1 };

/** @brief Image formats a thumbnail block may declare, and the data URI type each one needs. */
const THUMBNAIL_FORMAT_TYPES = { png: "image/png", jpg: "image/jpeg", jpeg: "image/jpeg", svg: "image/svg+xml" };

/** @brief Space-separated k=v text of a thumbnail header line, as an object of strings. */
function thumbnailFields(text) {
  const fields = {};
  for (const token of String(text || "").trim().split(/\s+/)) {
    const at = token.indexOf("=");
    if (at > 0) {
      fields[token.slice(0, at)] = token.slice(at + 1);
    }
  }
  return fields;
}

/**
 * @brief Preview image blocks written into the comments at the head of a G-code result.
 * @param {string} text Result text (the whole file, or just its head - the blocks are always first).
 * @returns {Array<{name: string, width: number, height: number, format: string, data: string,
 *            skipped: boolean, reason: string}>} In file order, empty when the run embedded none.
 *
 * Reading stops at the first line that is not a comment, because a block never appears below the
 * header: that is what keeps this cheap on a several-megabyte file. A block whose end line was never
 * reached (a truncated preview, say) is dropped rather than shown as a half picture, and a skip
 * notice is returned as an entry of its own so the panel can state why a view is missing.
 */
function parseGcodeThumbnails(text) {
  const blocks = [];
  let current = null;
  for (const raw of String(text || "").split("\n")) {
    const line = raw.replace(/\r$/, "");
    if (!line.startsWith(";")) {
      break;
    }
    if (blocks.length >= GCODE_THUMBNAIL_MAX_BLOCKS) {
      break;
    }
    const skipped = /^;\s*thumbnail skipped\s+(.*)$/.exec(line);
    if (skipped) {
      const fields = thumbnailFields(skipped[1]);
      blocks.push({
        name: fields.name || "",
        width: 0,
        height: 0,
        format: "",
        data: "",
        skipped: true,
        reason: fields.reason || "render_failed",
      });
      continue;
    }
    const begin = /^;\s*thumbnail begin\s+(.*)$/.exec(line);
    if (begin) {
      const fields = thumbnailFields(begin[1]);
      current = {
        name: fields.name || "",
        width: Number(fields.width) || 0,
        height: Number(fields.height) || 0,
        format: (fields.format || "png").toLowerCase(),
        data: "",
        skipped: false,
        reason: "",
      };
      continue;
    }
    if (/^;\s*thumbnail end\b/.test(line)) {
      if (current) {
        blocks.push(current);
      }
      current = null;
      continue;
    }
    if (current) {
      const chunk = /^;\s*([A-Za-z0-9+/=]*)$/.exec(line);
      if (chunk && current.data.length < GCODE_THUMBNAIL_MAX_B64) {
        current.data += chunk[1];
      }
    }
  }
  return blocks;
}

/**
 * @brief Data URI of one parsed preview block.
 * @param {object} thumb Entry as @ref parseGcodeThumbnails reports it.
 * @returns {string} "data:image/...;base64,...", "" when there is nothing to show.
 *
 * The console's content security policy already allows `img-src 'self' data:`, so an embedded image
 * needs no relaxation and no second request; an unknown format is refused rather than guessed at.
 */
function thumbnailDataUrl(thumb) {
  if (!thumb || thumb.skipped || !thumb.data) {
    return "";
  }
  const type = THUMBNAIL_FORMAT_TYPES[String(thumb.format || "").toLowerCase()];
  return type ? "data:" + type + ";base64," + thumb.data : "";
}

/**
 * @brief Same-origin URL of one entry inside a job's result package.
 * @param {string} id Job id.
 * @param {string} entry Entry path as the package listing reported it ("layers/layer_0.png").
 * @returns {string} Path a browser can put straight into an <img> src.
 *
 * Every component is encoded on its own, which is what lets a folder inside an archive survive the
 * route's greedy tail capture without turning a slash into a separator the server would re-read.
 */
function packageEntryUrl(id, entry) {
  const segments = String(entry || "").split("/").map((part) => encodeURIComponent(part)).join("/");
  return "/api/jobs/" + encodeURIComponent(id) + "/package/" + segments;
}

/** @brief First run of digits in a name - the layer number an export file is ordered by. */
function entryNumber(name) {
  const digits = /(\d+)/.exec(String(name || ""));
  return digits ? Number(digits[1]) : null;
}

/**
 * @brief Images a result package holds, in the order a layer browser should walk them.
 * @param {Array<{path: string, media_type: string}>} entries Entries of a package listing.
 * @returns {object[]} The image entries, layer number ascending, unnamed ones kept last in server order.
 *
 * "layer_10" sorting after "layer_2" is the whole point: a package that is walked in the archive's own
 * order would flip between the tenth and the second layer, which reads as a broken viewer.
 */
function packageImages(entries) {
  const listed = (entries || [])
    .map((entry, at) => ({ entry: entry, at: at, number: entryNumber(entry && entry.path) }))
    .filter((item) => item.entry && PACKAGE_IMAGE_MEDIA_TYPES[item.entry.media_type]);
  const numbered = listed
    .filter((item) => item.number !== null)
    .sort((a, b) => a.number - b.number || a.at - b.at);
  const plain = listed.filter((item) => item.number === null);
  return numbered.concat(plain).map((item) => item.entry);
}

/**
 * @brief Patch submission flags into a config the operator is about to send.
 * @param {string} text Config text exactly as it sits in the editor.
 * @param {object} flags Keys to set on the configuration (numbers, as the ABI reads them).
 * @returns {string} Re-serialized JSON, or @p text untouched when it is not a JSON object.
 *
 * The console submits whatever the editor holds, and a server may answer a default-config request as
 * TOML or INI; rewriting such a config would break the submission, so a non-JSON body is passed
 * through and the caller tells the operator its flags did not land.
 */
function applyConfigFlags(text, flags) {
  const body = text || "";
  const settings = flags || {};
  let parsed;
  try {
    parsed = JSON.parse(body);
  } catch (notJson) {
    return body;
  }
  if (!parsed || typeof parsed !== "object" || Array.isArray(parsed)) {
    return body;
  }
  for (const key of Object.keys(settings)) {
    parsed[key] = settings[key];
  }
  return JSON.stringify(parsed, null, 2);
}

globalThis.HsBaHttp = {
  PREVIEW_LIMIT_BYTES: PREVIEW_LIMIT_BYTES,
  GEOMETRY_ROUND_DIGITS: GEOMETRY_ROUND_DIGITS,
  GEOMETRY_VIEW_PAD: GEOMETRY_VIEW_PAD,
  GEOMETRY_EMPTY_VIEW: GEOMETRY_EMPTY_VIEW,
  GEOMETRY_MAX_PATHS: GEOMETRY_MAX_PATHS,
  GEOMETRY_MIN_ZOOM: GEOMETRY_MIN_ZOOM,
  GEOMETRY_MAX_ZOOM: GEOMETRY_MAX_ZOOM,
  GEOMETRY_VIEWS: GEOMETRY_VIEWS,
  GEOMETRY_STACK_LAYERS: GEOMETRY_STACK_LAYERS,
  GEOMETRY_STACK_ANGLES: GEOMETRY_STACK_ANGLES,
  GEOMETRY_SIDE_YAWS: GEOMETRY_SIDE_YAWS,
  GEOMETRY_ARROW_HALF: GEOMETRY_ARROW_HALF,
  roundCoord: roundCoord,
  mirrorLine: mirrorLine,
  clampZoom: clampZoom,
  ringPath: ringPath,
  geometryView: geometryView,
  currentViewBox: currentViewBox,
  layerPaths: layerPaths,
  projectPoint: projectPoint,
  projectedViewBox: projectedViewBox,
  projectedRingPaths: projectedRingPaths,
  projectedPathPolyline: projectedPathPolyline,
  stackBudget: stackBudget,
  sampleLayers: sampleLayers,
  sequenceColor: sequenceColor,
  arrowMark: arrowMark,
  GCODE_THUMBNAIL_MAX_BLOCKS: GCODE_THUMBNAIL_MAX_BLOCKS,
  GCODE_THUMBNAIL_MAX_B64: GCODE_THUMBNAIL_MAX_B64,
  PACKAGE_IMAGE_MEDIA_TYPES: PACKAGE_IMAGE_MEDIA_TYPES,
  parseGcodeThumbnails: parseGcodeThumbnails,
  thumbnailDataUrl: thumbnailDataUrl,
  packageEntryUrl: packageEntryUrl,
  packageImages: packageImages,
  applyConfigFlags: applyConfigFlags,
  RESULT_EXTENSIONS: RESULT_EXTENSIONS,
  filenameFromDisposition: filenameFromDisposition,
  resultFilename: resultFilename,
  isPreviewable: isPreviewable,
  previewText: previewText,
};
