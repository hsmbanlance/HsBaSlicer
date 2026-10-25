---
kind: design
name: Build optional REST API server as separate HsBaSlicerRestServer executable
source: session
category: adr
---

# Build optional REST API server as separate HsBaSlicerRestServer executable

_Source: coding plans from commit period ca9d258 → ae47e03 — records intent at planning time; the implementation may lag or differ._

**Status:** accepted

## Context
The project needed a network-facing interface to the existing C ABI of DllHsBaSlicer without coupling it to the desktop UI or changing the core slicer build. The restapi feature must not be pulled in by default and must fail fast when misconfigured.

## Decision drivers
- optional feature isolation
- no impact on default builds
- symmetric with existing HSBA_COPL detection pattern

## Considered options
- **Compile restapi into the main HsBaSlicer executable** — pros: single binary, no extra install target; cons: pulls cpp-httplib into every build; mixes UI and server concerns
- **Standalone executable gated by HSBA_BUILD_REST_API + vcpkg feature** — pros: default builds stay unchanged; dependency footprint is opt-in; mirrors COPL static-detection flow; cons: extra install target and config file (restapi.ini)

## Decision
Add top-level `restapi/` subdirectory producing `HsBaSlicerRestServer`, gated by `HSBA_BUILD_REST_API=ON` and the vcpkg feature `features.rest_api`; non-desktop platforms force it OFF at configure time.

## Consequences
Default builds remain free of cpp-httplib. Users must pass both `-DHSBA_BUILD_REST_API=ON` and `-DVCPKG_MANIFEST_FEATURES=rest_api`. A new install artifact and `restapi.ini` are shipped under `${CMAKE_INSTALL_SYSCONFDIR}/HsBaSlicer`.