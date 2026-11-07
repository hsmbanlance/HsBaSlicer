---
kind: design
name: Render stack/side views as projected SVG polylines rather than using a WebGL canvas
source: session
category: adr
---

# Render stack/side views as projected SVG polylines rather than using a WebGL canvas

_Source: coding plans from commit period 68c42f0 → c513cb3 — records intent at planning time; the implementation may lag or differ._

## Context
The WebUI already uses SVG for layer geometry; adding a full 3D renderer would introduce a second graphics stack, state synchronization, and CSP concerns for the Stack/Side views.

## Decision drivers
- reuse existing SVG pipeline
- DOM-free projection helpers
- simple rotation/pan interaction

## Considered options
- **Project each ring/path to 2D with `projectPoint(yaw,pitch)` and draw SVG `<path>` elements** — pros: no new library, CSS styling works, easy to add arrows and sequence colors
- **Integrate a WebGL/Three.js viewer for 3D navigation**; cons: adds a heavy dependency, requires asset loading, breaks the existing DOM-only contract

## Decision
Implement `projectPoint`, `projectedViewBox`, `projectedRingPaths`, `projectedPathPolyline` as pure JS functions in `httphelpers.js`; Stack samples up to 12 layers with `stackBudget`, Side toggles between yaw 0 and π/2, and Path renders the real polyline. Interaction uses mouse drag to adjust yaw/pitch and wheel to zoom via viewBox.

## Consequences
Rendering scales linearly with point count; very large spirals may need the `limit` budget. All drawing stays in SVG, so CSS classes like `.geometry-ordered` and `.geometry-arrow` compose naturally with existing styles.