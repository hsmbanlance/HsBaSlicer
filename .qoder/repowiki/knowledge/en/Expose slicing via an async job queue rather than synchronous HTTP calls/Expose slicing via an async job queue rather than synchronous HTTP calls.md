---
kind: design
name: Expose slicing via an async job queue rather than synchronous HTTP calls
source: session
category: adr
---

# Expose slicing via an async job queue rather than synchronous HTTP calls

_Source: coding plans from commit period ca9d258 → ae47e03 — records intent at planning time; the implementation may lag or differ._

**Status:** accepted

## Context
Slice pipelines can run for minutes; serving them synchronously would tie up HTTP workers and make progress reporting awkward. Clients need submit → poll → download semantics.

## Decision drivers
- non-blocking HTTP layer
- progress feedback
- bounded concurrency
- simple client protocol

## Considered options
- **Synchronous /api/jobs/{kind} that blocks until completion** — pros: simplest endpoint; cons: holds HTTP connections open; no progress; hard to cancel
- **Async job queue with queued/running/success/failed states** — pros: HTTP stays responsive; clients poll for progress; bounded by `max_concurrent_jobs`; cons: client must handle polling and result TTL cleanup

## Decision
Implement `job_manager` backed by `base/thread_pool.hpp` with `max_concurrent_jobs` workers. `POST /api/jobs/{kind}` returns 202 with a `boost::uuid` id; status is polled via `GET /api/jobs/{id}`; results are served from `/api/jobs/{id}/result` and evicted after `result_ttl_seconds` by a background cleaner.

## Consequences
Clients follow a three-step workflow. In-memory result storage means large G-code outputs consume process memory until TTL expiry; disk-backed persistence (`result_dir`) is left as a future extension point. Queue saturation returns 429.