---
kind: design
name: Route each pipeline kind through a single X-macro dispatch table
source: session
category: adr
---

# Route each pipeline kind through a single X-macro dispatch table

_Source: coding plans from commit period ca9d258 → ae47e03 — records intent at planning time; the implementation may lag or differ._

**Status:** accepted

## Context
There are nine `HsBa*PipelineKind` variants; adding one requires touching many places (run, free, default-config, JSON parse). A centralized table avoids scattered switch/if chains.

## Decision drivers
- add-a-kind cost
- compile-time consistency
- avoiding duplicated boilerplate

## Considered options
- **Per-kind route files or if/else chain in job_routes.cpp** — pros: explicit per-kind code; cons: easy to forget a handler when adding a new kind
- **`HSBA_PIPELINE_LIST` X-macro expanding `{kind → run_fn, free_fn, default_fn, from_json_fn}` entries** — pros: one line change per new kind; all four functions generated together; cons: macro-heavy style

## Decision
Define `HSBA_PIPELINE_LIST` in `routes/job_routes.cpp` and expand it to generate the kind→function static table used by the job dispatcher and default-config endpoints.

## Consequences
Adding a new pipeline kind now requires editing only the macro list plus implementing the four associated functions. Reduces risk of missing a handler and keeps the dispatch logic in one place.