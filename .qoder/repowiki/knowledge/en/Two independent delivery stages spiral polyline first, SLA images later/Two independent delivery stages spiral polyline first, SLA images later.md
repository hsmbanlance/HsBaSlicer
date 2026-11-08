---
kind: design
name: 'Two independent delivery stages: spiral polyline first, SLA images later'
source: session
category: adr
---

# Two independent delivery stages: spiral polyline first, SLA images later

_Source: coding plans from commit period 68c42f0 → c513cb3 — records intent at planning time; the implementation may lag or differ._

## Context
SLA true images live inside the result zip and have no entry-list channel, while the spiral polyline can be served from the geometry snapshot. Both features target the same WebUI geometry panel but have different data sources and dependencies.

## Decision drivers
- deliver spiral visualization ASAP
- keep stage 2 self-contained and testable
- avoid coupling image extraction to geometry snapshot plumbing

## Considered options
- **Ship stage 1 (spiral polyline + four views) then stage 2 (SLA result-package entries + G-code embedded PNG previews)** — pros: each stage compiles/tests independently; webui Path works without SLA images
- **Build both together so the UI has all features at once**; cons: blocks spiral delivery behind SLA image parsing complexity

## Decision
Split into two commits: stage 1 adds the polyline snapshot, REST endpoint, and Layer/Stack/Side/Path views; stage 2 adds a result-package entry endpoint for SLA images and embeds Base64 PNG previews into G-code header comments.

## Consequences
Stage 2 does not depend on `?geometry=1` or the polyline snapshot; SLA images flow through the result package, spirals through the geometry snapshot. The two data paths remain separate.