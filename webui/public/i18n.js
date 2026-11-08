"use strict";

/**
 * @file i18n.js
 * @brief Browser-side localization for the REST console: dictionaries, static-DOM translation and
 * the language switcher. Loaded only by the page (never by the node:vm tests), so the browser-only
 * APIs it uses (TreeWalker, localStorage, documentElement) never reach a test sandbox.
 *
 * Translation is keyed on the English source text, and English is the built-in default: the
 * registry and lookup live in /httphelpers.js (HsBaHttp.tr), which returns any string unchanged
 * while the active language is English. This file adds the non-English tables, walks the static
 * markup so every text node and label attribute can be reworded, and offers a selector that
 * remembers the choice. Picking another language reloads the page, so the dynamic panels app.js
 * renders come up fully in the new language instead of half-swapped.
 *
 * @author HsBa
 */

(function () {
  const http = globalThis.HsBaHttp;
  if (!http || typeof http.registerLocale !== "function") {
    // The helpers are the translation engine; without them this file has nothing to register into,
    // so the page keeps its built-in English rather than throwing during startup.
    return;
  }

  /** @brief Where the operator's language choice is remembered between visits. */
  const LANG_KEY = "hsba.console.lang";

  /** @brief Human-facing name for each language code, shown on the switcher. */
  const LOCALE_NAMES = {
    en: "English",
    zh: "简体中文",
  };

  /**
   * @brief Chinese table, keyed on the exact English text the console uses.
   *
   * Keys are the whole trimmed text node, a single label attribute, or one fixed fragment app.js
   * assembles around dynamic values; a message that mixes in server text (an error's own sentence,
   * a file path) is kept as the surrounding literal so only the console's own words get translated.
   */
  const ZH = {
    // ---- static chrome (headings, labels, buttons, table headers) ----
    Credentials: "凭据",
    "API token": "API 令牌",
    Save: "保存",
    Clear: "清除",
    Slicing: "切片",
    Pipeline: "流水线",
    "Load default config": "加载默认配置",
    "Model file": "模型文件",
    "capture geometry": "捕获几何",
    "embed preview images (fdm)": "嵌入预览图像 (fdm)",
    "Submit job": "提交任务",
    Jobs: "任务",
    Refresh: "刷新",
    auto: "自动",
    Job: "任务",
    State: "状态",
    Progress: "进度",
    Layers: "层数",
    Elapsed: "耗时",
    Output: "输出",
    Actions: "操作",
    Uploads: "上传",
    Upload: "上传",
    File: "文件",
    "Stored path": "存储路径",
    Size: "大小",
    Expires: "过期",
    "Layer geometry": "层几何",
    "Reload layer": "重新加载层",
    Layer: "层",
    Stack: "堆叠",
    Side: "侧视",
    Path: "路径",
    outline: "轮廓",
    infill: "填充",
    support: "支撑",
    order: "顺序",
    Fit: "适应",
    "Result preview": "结果预览",
    Images: "图像",
    Prev: "上一张",
    Next: "下一张",
    Log: "日志",

    // ---- label / input attributes ----
    "only needed when the REST server requires one": "仅当 REST 服务器要求时才填写",
    "previous layer": "上一层",
    "next layer": "下一层",
    "previous image": "上一张图像",
    "next image": "下一张图像",
    "package image": "包内图像",

    // ---- status badges and empty states ----
    checking: "检查中",
    ok: "正常",
    Language: "语言",
    "token required": "需要令牌",
    unreachable: "无法访问",
    "no jobs yet": "暂无任务",
    "no uploaded models": "没有已上传的模型",
    "upload pool not queried yet": "尚未查询上传池",
    "no layer geometry previewed yet": "尚未预览层几何",
    "no result previewed yet": "尚未预览结果",

    // ---- job and upload state words reported by the API ----
    queued: "排队中",
    running: "运行中",
    success: "成功",
    failed: "失败",
    cancelled: "已取消",
    available: "可用",
    in_use: "使用中",
    expired: "已过期",

    // ---- row actions ----
    Preview: "预览",
    Result: "结果",
    Geometry: "几何",
    Cancel: "取消",
    Delete: "删除",

    // ---- dynamic log and panel messages ----
    copied: "已复制",
    submitted: "已提交",
    deleted: "已删除",
    previewed: "已预览",
    downloaded: "已下载",
    "loaded the default config of": "已加载以下流水线的默认配置：",
    "opened the geometry of": "已打开以下任务的几何：",
    "package of": "该任务的包",
    "holds no images": "不含图像",
    layers: "层",
    "token saved for this browser": "令牌已为本浏览器保存",
    "token cleared": "令牌已清除",
    "jobs auto-refresh resumed after token change": "令牌变更后已恢复任务自动刷新",
    "jobs auto-refresh paused: unauthorized (401); save an API key to resume":
      "任务自动刷新已暂停：未授权 (401)；保存 API 密钥以恢复",
    "config is not JSON - gcode_thumbnails not applied": "配置不是 JSON - 未应用 gcode_thumbnails",
    "preview needs a download instead:": "预览需要改为下载：",
    is: "是",
    binary: "二进制",
    "preview a result first - Images works on the job whose result is on screen":
      "请先预览一个结果 - “图像”仅作用于当前显示结果的任务",
    "this result is a single file, not a package": "此结果是单个文件，而非打包结果",
    "no package to read:": "没有可读取的包：",
    "package has no images - SLA exports PNG/JPG/SVG layers": "包内没有图像 - SLA 导出 PNG/JPG/SVG 层",
    "{size} of {type}, not text - use Result to download it":
      "{size}，类型为 {type}，非文本 - 请使用“结果”下载",
    "unknown type": "未知类型",
    lines: "行",
    "showing the first": "仅显示开头",
    "use Result for the whole file": "使用“结果”获取完整文件",
    "no embedded previews, set gcode_thumbnails to 1": "无内嵌预览，请将 gcode_thumbnails 设为 1",
    preview: "预览",
    "was not embedded:": "未被嵌入：",
    "model preview": "模型预览",
    "(click to copy)": "（点击复制）",
    "the snapshot pool keeps only the last few jobs": "快照池仅保留最近几个任务",
    "spiral runs only": "仅限螺旋运行",
    "this run deposited no continuous path - spiral_mode=1 belongs to fdm, tdp and waam":
      "本次运行未沉积连续路径 - spiral_mode=1 适用于 fdm、tdp 与 waam",
    "this job holds no layer geometry - submit it with capture geometry enabled":
      "该任务不含层几何 - 请在启用捕获几何的情况下提交",
    "cancel requested for": "已请求取消任务",
    accepted: "已接受",
    "the job stops at its next safe point": "任务将在下一个安全点停止",
    "the REST server reported no pipeline kinds": "REST 服务器未报告任何流水线类型",
    "no pipeline selected": "未选择流水线",
    "load a default config (or paste one) before submitting": "提交前请加载（或粘贴）一份默认配置",
    "clipboard unavailable: open the console on localhost or over https":
      "剪贴板不可用：请在 localhost 或 https 下打开控制台",

    // ---- geometry panel meta fragments ----
    "paths left undrawn,": "条路径未绘制，",
    "is the budget": "为预算上限",
    "left undrawn,": "条未绘制，",
    "per layer": "每层上限",
    layer: "层",
    outlines: "轮廓",
    fills: "填充",
    supports: "支撑",
    path: "路径",
    points: "个点",
    "one uninterrupted line, the arrow marks where the run was heading":
      "一条不间断的线，箭头标示运行的前进方向",
    "layers of": "层，共",
    paths: "条路径",
    "lines between rings are inferred travel": "环之间的连线为推断的空走路径",
  };

  http.registerLocale("zh", ZH);

  /** @brief The language to start with: a saved choice, else the browser, else English. */
  function detectLocale() {
    let stored = "";
    try {
      stored = localStorage.getItem(LANG_KEY) || "";
    } catch (blocked) {
      stored = "";
    }
    if (stored && http.availableLocales().indexOf(stored) !== -1) {
      return stored;
    }
    const browser = String((globalThis.navigator && globalThis.navigator.language) || "").toLowerCase();
    if (browser.indexOf("zh") === 0) {
      return "zh";
    }
    return "en";
  }

  const locale = http.setLocale(detectLocale());

  /**
   * @brief Rewrite one text node when its trimmed form is a dictionary key.
   * @param {Text} node A text node in the static markup.
   *
   * Surrounding whitespace is preserved so the layout of an inline label (" capture geometry") does
   * not shift, and a node with no key is left exactly as the server sent it.
   */
  function translateTextNode(node) {
    const raw = node.nodeValue || "";
    const trimmed = raw.trim();
    if (!trimmed) {
      return;
    }
    const translated = http.tr(trimmed);
    if (translated === trimmed) {
      return;
    }
    const leading = raw.match(/^\s*/)[0];
    const trailing = raw.match(/\s*$/)[0];
    node.nodeValue = leading + translated + trailing;
  }

  /** @brief Translate every text node under @p root via a live TreeWalker. */
  function translateText(root) {
    if (typeof document.createTreeWalker !== "function") {
      return;
    }
    const walker = document.createTreeWalker(root, globalThis.NodeFilter.SHOW_TEXT);
    let node = walker.nextNode();
    while (node) {
      translateTextNode(node);
      node = walker.nextNode();
    }
  }

  /** @brief Translate the label-bearing attributes the console's markup uses. */
  function translateAttributes(root) {
    const attributes = ["placeholder", "title", "aria-label"];
    const all = [root].concat(Array.prototype.slice.call(root.querySelectorAll("*")));
    for (const element of all) {
      for (const name of attributes) {
        const value = element.getAttribute ? element.getAttribute(name) : null;
        if (value && value.trim()) {
          const translated = http.tr(value.trim());
          if (translated !== value.trim()) {
            element.setAttribute(name, translated);
          }
        }
      }
    }
  }

  /** @brief Reflect the active language on the document so screen readers and CSS can key on it. */
  function applyDocumentLanguage(code) {
    if (document.documentElement) {
      document.documentElement.setAttribute("lang", code === "zh" ? "zh-CN" : code);
    }
  }

  /**
   * @brief Build the language selector into the status area and wire the switch.
   * @param {string} containerId The element that should hold the selector.
   *
   * Changing the language reloads the page: that is the simplest way to make the panels app.js
   * fills dynamically (job rows, geometry and preview meta) come up in the new language rather than
   * leaving a mix of two languages on screen until the next unrelated refresh.
   */
  function buildSwitcher(containerId) {
    const host = document.getElementById(containerId);
    if (!host || !host.appendChild) {
      return;
    }
    const select = document.createElement("select");
    select.id = "lang";
    select.title = http.tr("Language");
    for (const code of http.availableLocales()) {
      const option = document.createElement("option");
      option.value = code;
      option.textContent = LOCALE_NAMES[code] || code;
      if (code === locale) {
        option.selected = true;
      }
      select.appendChild(option);
    }
    select.addEventListener("change", () => {
      const next = select.value;
      try {
        localStorage.setItem(LANG_KEY, next);
      } catch (blocked) {
        // A storage error must not strand the page: the reload below simply falls back to detection.
      }
      if (next !== locale) {
        location.reload();
      }
    });
    host.appendChild(select);
  }

  document.addEventListener("DOMContentLoaded", () => {
    applyDocumentLanguage(locale);
    translateText(document.body);
    translateAttributes(document.body);
    buildSwitcher("lang-slot");
  });
})();
