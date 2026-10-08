---
kind: business_term
name: Business Glossary
category: business_term
scope:
    - '**'
---

### HsBaSlicer
- Definition：The project's own name: a high-performance, modular, cross-platform C++ framework providing slicing-core capabilities for 3D-printing pipelines (FDM/SLA), intended for both research and production use.

### core modules
- Definition：The set of directories whose coverage is held to the higher 80% gate: base, 2D, utils, cipher, meshmodel, paths, preprocess, support, and LibHsBaSlicer. All other source falls into the 'other' bucket with a 60% threshold.
- Aliases：CORE

### other modules
- Definition：Source files outside the core-module list, subject to the lower 60% coverage gate.
- Aliases：OTHER

### static_tests
- Definition：Compile-only template sanity checks that do not link/run at runtime; they intentionally do not participate in coverage or the build gate because they cannot produce `.gcda` and trigger a GCC 13 duplicate-symbol bug when instrumented.

### AnyObject Mock
- Definition：A mock/stub approach built on top of the project's `AnyObject::Invoke` dispatch and a Mockit-style stubbing facility, used to drive otherwise hard-to-reach Lua binding branches (error propagation, nil return, type conversion, unregistered method) without needing real Lua script failures.
- Aliases：Mockit-style stubs、ScopedStubs
