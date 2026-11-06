"use strict";

/**
 * @file view.js
 * @brief MVC View of the REST console: everything that turns data and state into DOM.
 *
 * The View owns the table cells, the log panel, the preview/package strips and the SVG geometry
 * drawing. It reads session state off the Model and hands user gestures back to the Controller, but
 * it never fetches and never decides what a click means. It is loaded after model.js (it reads
 * HsBaApp.Model) and before app.js; the two callbacks it needs from the Controller (run, copyText)
 * and the action tables (JOB_ACTIONS, UPLOAD_ACTIONS) are dereferenced only at call time, by which
 * point the controller has loaded. Its surface is published on globalThis.HsBaApp.View.
 */

(function () {
  const Model = globalThis.HsBaApp.Model;
  const geometry = Model.geometry;
  const pack = Model.pack;
  const geometryLayerCount = Model.geometryLayerCount;
  const hasContinuousPath = Model.hasContinuousPath;
  const geometryAngles = Model.geometryAngles;

  const LOG_LIMIT = 200;
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

  // Shorthand for the English-keyed translation lookup that lives in /httphelpers.js. It stays the
  // identity while the active language is English (the built-in default the node:vm tests run against),
  // so every message below keeps its exact English wording unless a dictionary from /i18n.js rewords it.
  const tr = (text, params) => HsBaHttp.tr(text, params);

  // Forward user gestures to the Controller without naming any of its functions here: the click of a
  // cell or a row action is the Controller's concern, but wiring it is the View's, exactly as classic
  // MVC lets a View reference its Controller for event handling.
  const run = (fn) => globalThis.HsBaApp.Controller.run(fn);
  const copyText = (value) => globalThis.HsBaApp.Controller.copyText(value);

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
        button.textContent = tr(action.label);
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
      title: path ? path + "\n" + tr("(click to copy)") : fallback || "",
    });
    bindOnce(cell, () => copyText(read(row)));
    return cell;
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

  /** One job row: the values of the current listing, kept in step with the row already on screen. */
  function buildJobRow(row, job) {
    row.job = job;
    cellAt(row, 0, { className: "mono", text: job.id });
    cellAt(row, 1, { text: job.kind });
    cellAt(row, 2, { className: "state-" + job.state, text: tr(job.state), title: job.error_message || "" });
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
    actionsCell(row, 7, globalThis.HsBaApp.Controller.JOB_ACTIONS);
  }

  /** One upload row: the stored server-side path is a column of its own, not a tooltip. */
  function buildUploadRow(row, upload) {
    row.upload = upload;
    cellAt(row, 0, { className: "mono", text: upload.id });
    cellAt(row, 1, { className: "mono", text: upload.job_id || "-" });
    cellAt(row, 2, { text: upload.original_name || upload.file_name });
    pathCell(row, 3, upload.path, "-", (r) => r.upload.path);
    cellAt(row, 4, { text: formatBytes(upload.size_bytes) });
    cellAt(row, 5, { className: "state-" + upload.state, text: tr(upload.state) });
    cellAt(row, 6, { className: "muted", text: formatRemaining(upload.remaining_seconds) });
    actionsCell(row, 7, globalThis.HsBaApp.Controller.UPLOAD_ACTIONS);
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
        note.textContent =
          (block.name || tr("preview")) + " " + tr("was not embedded:") + " " + block.reason;
        stage.appendChild(note);
        continue;
      }
      if (!url) {
        continue;
      }
      const image = document.createElement("img");
      image.setAttribute("src", url);
      image.setAttribute("alt", block.name || tr("model preview"));
      stage.appendChild(image);
      const label = document.createElement("span");
      label.className = "thumbnail-label";
      label.textContent =
        (block.name || tr("preview")) + " (" + block.width + "x" + block.height + ")";
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

  /** Show a sentence in the panel instead of a drawing, and drop whatever was drawn before. */
  function setGeometryMessage(text) {
    byId("geometry-stage").hidden = true;
    byId("geometry-svg").replaceChildren();
    byId("geometry-meta").textContent = text;
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

  function stageWidth() {
    return Number(byId("geometry-stage").clientWidth) || FALLBACK_STAGE_WIDTH;
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
      ? " (" + counts.dropped + " " + tr("paths left undrawn,") + " " + HsBaHttp.GEOMETRY_MAX_PATHS +
        " " + tr("is the budget") + ")"
      : "";
    byId("geometry-meta").textContent =
      tr("layer") + " " + (geometry.layer + 1) + " / " + geometryLayerCount() +
      ", z " + geometry.z + " mm, " + tr("outlines") + " " + geometry.paths.outlines.length +
      " / " + tr("fills") + " " + geometry.paths.fills.length +
      " / " + tr("supports") + " " + geometry.paths.supports.length + undrawn;
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

    const undrawn = dropped
      ? " (" + dropped + " " + tr("left undrawn,") + " " + budget + " " + tr("per layer") + ")"
      : "";
    byId("geometry-meta").textContent =
      geometry.view + " " + planes.length + " " + tr("layers of") + " " + geometryLayerCount() +
      ", " + total + " " + tr("paths") + undrawn + " - " + tr("lines between rings are inferred travel");
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
      tr("path") + " " + geometry.path.count + " " + tr("points") + ", z " + (range[0] || 0) + " - " +
      (range[1] || 0) + " mm - " + tr("one uninterrupted line, the arrow marks where the run was heading");
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

  globalThis.HsBaApp = globalThis.HsBaApp || {};
  globalThis.HsBaApp.View = {
    byId: byId,
    tr: tr,
    GEOMETRY_GROUPS: GEOMETRY_GROUPS,
    writeLog: writeLog,
    formatSeconds: formatSeconds,
    formatBytes: formatBytes,
    formatRemaining: formatRemaining,
    buildJobRow: buildJobRow,
    buildUploadRow: buildUploadRow,
    tableFor: tableFor,
    renderGcodeThumbnails: renderGcodeThumbnails,
    clearThumbnails: clearThumbnails,
    setPackageMessage: setPackageMessage,
    clearPackage: clearPackage,
    showPackageImage: showPackageImage,
    setGeometryMessage: setGeometryMessage,
    setGeometryControlsEnabled: setGeometryControlsEnabled,
    markGeometryView: markGeometryView,
    stageWidth: stageWidth,
    renderGeometry: renderGeometry,
  };
})();
