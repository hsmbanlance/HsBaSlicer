"use strict";

/**
 * @file httphelpers.js
 * @brief DOM-free helpers behind the REST console's result download and preview.
 *
 * They turn HTTP metadata into a file name and a bounded text view, so they can be unit tested
 * without a browser: webui/test/server.test.js evaluates this file in a node:vm sandbox and calls
 * the functions directly. The page loads it as a plain script before /app.js and reads the same
 * global object.
 *
 * @author HsBa
 */

/** @brief Longest result text the console renders before pointing at the download. */
const PREVIEW_LIMIT_BYTES = 64 * 1024;

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

globalThis.HsBaHttp = {
  PREVIEW_LIMIT_BYTES: PREVIEW_LIMIT_BYTES,
  RESULT_EXTENSIONS: RESULT_EXTENSIONS,
  filenameFromDisposition: filenameFromDisposition,
  resultFilename: resultFilename,
  isPreviewable: isPreviewable,
  previewText: previewText,
};
