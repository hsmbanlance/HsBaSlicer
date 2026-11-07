---
kind: design
name: Register `/api/jobs/{id}/geometry/path` before the generic layer-number route
source: session
category: adr
---

# Register `/api/jobs/{id}/geometry/path` before the generic layer-number route

_Source: coding plans from commit period 68c42f0 → c513cb3 — records intent at planning time; the implementation may lag or differ._

## Context
httplib matches routes in registration order; the existing pattern `/api/jobs/([^/]+)/geometry/([^/]+)` would swallow the literal segment `path` and treat it as a layer index, returning 400 instead of the new endpoint.

## Decision drivers
- correct URL dispatch without changing httplib
- keep layer-number routing generic

## Considered options
- **Register the exact `/geometry/path` route immediately before the generic `/geometry/([^/]+)` route** — pros: one-line fix, no regex change, explicit ordering comment
- **Rewrite the generic route to require digits only** — pros: prevents future accidental collisions; cons: changes an existing public pattern; more invasive

## Decision
Register the handler for `/api/jobs/([^/]+)/geometry/path` right after cancel and before the layer-number route, and document that `geometry/path` is a reserved sub-path.

## Consequences
Any future sub-path under `/geometry/` must be registered before the catch-all layer route; otherwise it will be interpreted as a numeric layer index.