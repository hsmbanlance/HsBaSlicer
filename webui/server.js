#!/usr/bin/env node
/**
 * @file server.js
 * @brief HsBaSlicer REST Console: a standalone, dependency-free Web UI for HsBaSlicerRestServer.
 *
 * The console is a plain HTTP server that does exactly two things:
 *
 *   1. serve the static bundle under public/ (HTML / CSS / JS / icon), and
 *   2. reverse-proxy /api/* to the REST server named by --rest-url.
 *
 * Nothing here links against, loads, or imports any part of the C++ project: the console is
 * an ordinary HTTP client of the REST API, so it can be built, tested and released on its own
 * schedule (see .github/workflows/rest-webui.yml).
 *
 * Proxying instead of calling the REST server directly from the browser keeps the page
 * same-origin, so no CORS support is needed on the C++ side, and it lets an operator pass the
 * shared token on the command line (--token) so the secret never reaches the browser.
 *
 * Usage: node server.js [--rest-url <url>] [--host <addr>] [--port <n>] [--token <t>]
 *
 * @author HsBa
 */
import { createReadStream } from "node:fs";
import { stat } from "node:fs/promises";
import http from "node:http";
import https from "node:https";
import path from "node:path";
import process from "node:process";
import { fileURLToPath } from "node:url";

const ASSET_ROOT = path.join(path.dirname(fileURLToPath(import.meta.url)), "public");

// An explicit allowlist, never a path join with client input: there is no directory to
// traverse, and an unknown path is answered with 404 instead of a file-system lookup.
const ASSETS = new Map([
  ["/", ["index.html", "text/html; charset=utf-8"]],
  ["/index.html", ["index.html", "text/html; charset=utf-8"]],
  ["/app.css", ["app.css", "text/css; charset=utf-8"]],
  ["/httphelpers.js", ["httphelpers.js", "text/javascript; charset=utf-8"]],
  ["/i18n.js", ["i18n.js", "text/javascript; charset=utf-8"]],
  ["/app.js", ["app.js", "text/javascript; charset=utf-8"]],
  ["/favicon.svg", ["favicon.svg", "image/svg+xml"]],
]);

// The bundle has no inline <script>/<style> and no external URL, so 'self' is enough; the
// policy makes that guarantee enforced instead of merely documented.
const HTML_CSP =
  "default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' data:; " +
  "connect-src 'self'; form-action 'none'; base-uri 'none'; frame-ancestors 'none'";

const PROXY_PREFIX = "/api/";
const PROXY_METHODS = new Set(["GET", "HEAD", "POST", "PUT", "DELETE"]);

// Hop-by-hop headers must not be forwarded (RFC 9110 section 7.6.1). Cookies, Origin and Referer
// are dropped on purpose: the console shares no session state with the REST server, and leaking
// the console's browser context upstream buys nothing.
const DROPPED_REQUEST_HEADERS = new Set([
  "host",
  "connection",
  "keep-alive",
  "proxy-authenticate",
  "proxy-authorization",
  "proxy-connection",
  "te",
  "trailer",
  "transfer-encoding",
  "upgrade",
  "cookie",
  "origin",
  "referer",
]);

const DROPPED_RESPONSE_HEADERS = new Set([
  "connection",
  "keep-alive",
  "proxy-authenticate",
  "te",
  "trailer",
  "transfer-encoding",
  "upgrade",
]);

/** @brief Built-in defaults, each overridable by a flag or an environment variable. */
export const DEFAULTS = {
  restUrl: "http://127.0.0.1:8080",
  host: "127.0.0.1",
  port: 5080,
  token: "",
};

/** @brief Raised for invalid command-line or environment input (exit code 2). */
export class UsageError extends Error {
  constructor(message) {
    super(message);
    this.name = "UsageError";
  }
}

/** @brief --help text (also printed after a UsageError). */
export const USAGE = `Usage: node server.js [options]

  --rest-url <url>   HsBaSlicerRestServer base URL, optional path prefix allowed
                     (default: ${DEFAULTS.restUrl})
  --host <addr>      Address this console binds to (default: ${DEFAULTS.host})
  --port <n>         Port of this console (default: ${DEFAULTS.port}, 0 = ephemeral)
  --token <t>        Shared REST token; the proxy injects it as X-API-Key
  --help, -h         Show this message

Environment fallbacks (a flag always wins):
  HSBA_REST_URL, HSBA_WEBUI_HOST, HSBA_WEBUI_PORT, HSBA_REST_AUTH_TOKEN`;

/**
 * @brief Validate and normalize a REST server base URL.
 * @param {string} value Candidate URL; https and an optional path prefix are supported.
 * @returns {URL} Parsed URL.
 * @throws {UsageError} When the value is not an absolute http(s) URL.
 */
export function normalizeRestUrl(value) {
  let url;
  try {
    url = new URL(String(value));
  } catch {
    throw new UsageError(`invalid REST URL: ${value}`);
  }
  if (url.protocol !== "http:" && url.protocol !== "https:") {
    throw new UsageError(`REST URL must be http or https: ${value}`);
  }
  return url;
}

function requireValue(argv, index, flag) {
  const value = argv[index];
  if (value === undefined || value.startsWith("--")) {
    throw new UsageError(`${flag} needs a value`);
  }
  return value;
}

/**
 * @brief Parse command-line arguments with environment fallbacks.
 * @param {string[]} argv Arguments without the script name.
 * @param {Record<string, string | undefined>} env Environment source (injectable for tests).
 * @returns {{restUrl: string, host: string, port: number, token: string, help: boolean}}
 * @throws {UsageError} On unknown flags, missing values or out-of-range settings.
 */
export function parseCliArgs(argv = [], env = process.env) {
  const options = {
    restUrl: env.HSBA_REST_URL || DEFAULTS.restUrl,
    host: env.HSBA_WEBUI_HOST || DEFAULTS.host,
    port: Number(env.HSBA_WEBUI_PORT ?? DEFAULTS.port),
    token: env.HSBA_REST_AUTH_TOKEN || DEFAULTS.token,
    help: false,
  };

  for (let index = 0; index < argv.length; index += 1) {
    const arg = argv[index];
    switch (arg) {
      case "--rest-url":
        options.restUrl = requireValue(argv, index + 1, arg);
        index += 1;
        break;
      case "--host":
        options.host = requireValue(argv, index + 1, arg);
        index += 1;
        break;
      case "--port":
        options.port = Number(requireValue(argv, index + 1, arg));
        index += 1;
        break;
      case "--token":
        options.token = requireValue(argv, index + 1, arg);
        index += 1;
        break;
      case "--help":
      case "-h":
        options.help = true;
        break;
      default:
        throw new UsageError(`unrecognized option: ${arg}`);
    }
  }

  normalizeRestUrl(options.restUrl);
  if (!Number.isInteger(options.port) || options.port < 0 || options.port > 65535) {
    throw new UsageError(`invalid port: ${options.port}`);
  }
  return options;
}

function sendText(res, status, body) {
  res.writeHead(status, {
    "Content-Type": "text/plain; charset=utf-8",
    "Content-Length": Buffer.byteLength(body),
    "Cache-Control": "no-store",
  });
  res.end(body);
}

/** @brief Answer with the same {code, message, detail} shape the REST server uses. */
function sendJson(res, status, payload) {
  const body = JSON.stringify(payload);
  res.writeHead(status, {
    "Content-Type": "application/json; charset=utf-8",
    "Content-Length": Buffer.byteLength(body),
    "Cache-Control": "no-store",
  });
  res.end(body);
}

async function serveAsset(res, pathname, headOnly) {
  const route = ASSETS.get(pathname);
  if (!route) {
    sendText(res, 404, "not found\n");
    return;
  }
  const [file, contentType] = route;
  const fullPath = path.join(ASSET_ROOT, file);
  let info;
  try {
    info = await stat(fullPath);
  } catch {
    sendText(res, 500, `console asset missing: ${file}\n`);
    return;
  }
  const headers = {
    "Content-Type": contentType,
    "Content-Length": info.size,
    // No hashed asset names exist, so a rebuilt bundle must always be revalidated.
    "Cache-Control": "no-cache",
    "X-Content-Type-Options": "nosniff",
  };
  if (contentType.startsWith("text/html")) {
    headers["Content-Security-Policy"] = HTML_CSP;
  }
  res.writeHead(200, headers);
  if (headOnly) {
    res.end();
    return;
  }
  createReadStream(fullPath).pipe(res);
}

function buildTargetUrl(upstream, requestUrl) {
  const prefix = upstream.pathname.replace(/\/+$/, "");
  return new URL(`${prefix}${requestUrl.pathname}${requestUrl.search}`, upstream.origin);
}

function proxyRequest(req, res, requestUrl, upstream, token) {
  if (!PROXY_METHODS.has(req.method)) {
    // Drain the body so the rejection does not surface as a socket reset on the client.
    req.resume();
    sendJson(res, 405, {
      code: 405,
      message: "method not allowed",
      detail: `${req.method} is not proxied to the REST server`,
    });
    return;
  }

  const target = buildTargetUrl(upstream, requestUrl);
  const client = target.protocol === "https:" ? https : http;

  const headers = {};
  for (const [name, value] of Object.entries(req.headers)) {
    if (!DROPPED_REQUEST_HEADERS.has(name)) {
      headers[name] = value;
    }
  }
  // A launch-time token always wins: it is the operator's decision, and it keeps the secret
  // out of the browser entirely.
  if (token) {
    headers["x-api-key"] = token;
  }

  const upstreamRequest = client.request(target, { method: req.method, headers }, (upstreamResponse) => {
    const responseHeaders = {};
    for (const [name, value] of Object.entries(upstreamResponse.headers)) {
      if (!DROPPED_RESPONSE_HEADERS.has(name)) {
        responseHeaders[name] = value;
      }
    }
    res.writeHead(upstreamResponse.statusCode || 502, responseHeaders);
    upstreamResponse.pipe(res);
  });

  upstreamRequest.on("error", (error) => {
    if (res.headersSent) {
      res.destroy(error);
      return;
    }
    sendJson(res, 502, {
      code: 502,
      message: "REST server unreachable",
      detail: `${target.origin}: ${error.message}`,
    });
  });

  req.pipe(upstreamRequest);
}

async function handleRequest(req, res, upstream, token) {
  const requestUrl = new URL(req.url || "/", "http://console.invalid");

  if (requestUrl.pathname === "/healthz") {
    sendText(res, 200, "ok\n");
    return;
  }
  if (requestUrl.pathname.startsWith(PROXY_PREFIX)) {
    proxyRequest(req, res, requestUrl, upstream, token);
    return;
  }
  if (req.method !== "GET" && req.method !== "HEAD") {
    req.resume();
    sendJson(res, 405, {
      code: 405,
      message: "method not allowed",
      detail: "the console only serves GET/HEAD outside /api/*",
    });
    return;
  }
  await serveAsset(res, requestUrl.pathname, req.method === "HEAD");
}

/**
 * @brief Create the console server without listening.
 * @param {{restUrl?: string, token?: string}} options Upstream URL and optional shared token.
 * @returns {import("node:http").Server} Unbound HTTP server.
 */
export function createConsoleServer({ restUrl = DEFAULTS.restUrl, token = "" } = {}) {
  const upstream = normalizeRestUrl(restUrl);
  const server = http.createServer((req, res) => {
    handleRequest(req, res, upstream, token).catch((error) => {
      if (res.headersSent) {
        res.destroy(error);
        return;
      }
      sendJson(res, 500, { code: 500, message: "console internal error", detail: error.message });
    });
  });
  // Model uploads and result downloads may legitimately outlive Node's default 5 minute
  // request timeout; the REST server enforces its own body and payload caps.
  server.requestTimeout = 0;
  return server;
}

/**
 * @brief Start the console and resolve once it is listening.
 * @param {{restUrl?: string, host?: string, port?: number, token?: string}} options Settings.
 * @returns {Promise<import("node:http").Server>} Listening server (port 0 resolves ephemeral).
 */
export async function startConsole(options = {}) {
  const server = createConsoleServer(options);
  const host = options.host ?? DEFAULTS.host;
  const port = options.port ?? DEFAULTS.port;
  await new Promise((resolve, reject) => {
    server.once("error", reject);
    server.listen(port, host, () => {
      server.removeListener("error", reject);
      resolve();
    });
  });
  return server;
}

/**
 * @brief Browsable URL of a listening console.
 * @param {import("node:http").Server} server Listening server.
 * @returns {string} e.g. "http://127.0.0.1:5080/".
 */
export function consoleUrl(server) {
  const address = server.address();
  let host = address.address;
  if (host === "0.0.0.0" || host === "::") {
    host = "127.0.0.1";
  } else if (address.family === "IPv6") {
    host = `[${host}]`;
  }
  return `http://${host}:${address.port}/`;
}

async function main(argv) {
  const options = parseCliArgs(argv);
  if (options.help) {
    process.stdout.write(`${USAGE}\n`);
    return;
  }

  const server = await startConsole(options);
  process.stderr.write(`HsBaSlicer REST Console: ${consoleUrl(server)}\n`);
  process.stderr.write(
    `proxying /api/* -> ${options.restUrl} (token: ${options.token ? "injected by the console" : "forwarded from the browser"})\n`,
  );
  if (options.host !== "127.0.0.1" && options.host !== "localhost" && options.host !== "::1") {
    process.stderr.write(
      `warning: '${options.host}' exposes the console to other hosts without its own authentication\n`,
    );
  }

  const shutdown = () => {
    server.close(() => process.exit(0));
    server.closeAllConnections();
  };
  process.on("SIGINT", shutdown);
  process.on("SIGTERM", shutdown);
}

const invokedDirectly = Boolean(process.argv[1]) && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url);
if (invokedDirectly) {
  main(process.argv.slice(2)).catch((error) => {
    const usage = error instanceof UsageError;
    process.stderr.write(`${usage ? error.message : error.stack}\n`);
    if (usage) {
      process.stderr.write(`\n${USAGE}\n`);
    }
    process.exitCode = usage ? 2 : 1;
  });
}
