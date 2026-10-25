---
kind: design
name: Support both JSON and XML response/request formats via content negotiation
source: session
category: adr
---

# Support both JSON and XML response/request formats via content negotiation

_Source: coding plans from commit period ca9d258 → ae47e03 — records intent at planning time; the implementation may lag or differ._

**Status:** accepted

## Context
Existing consumers use either JSON or protobuf/XML interchange; the REST layer must serve both without duplicating serialization logic.

## Decision drivers
- backward compatibility with existing consumers
- single serialization path
- standard HTTP negotiation

## Considered options
- **JSON-only API** — pros: simpler implementation; cons: breaks existing XML/protobuf consumers
- **Dual-format with `?format=` query taking precedence over `Accept` header, defaulting to JSON** — pros: compatible with both consumers; uses shared `to_response(Json|Xml)` helpers; cons: must maintain two serializers

## Decision
All responses go through `content_negotiate::{to_response(Json|Xml)}`; request body format is resolved by `Content-Type: application/json | application/xml | text/xml`, with `?format=` overriding `Accept`. Errors are uniformly `{code,message}` JSON or `<Error><Code/><Message/></Error>` XML.

## Consequences
Every endpoint must serialize via `write_pretty_json` / `write_xml` — hand-painted strings are forbidden. Consumers can pick their preferred wire format per request.