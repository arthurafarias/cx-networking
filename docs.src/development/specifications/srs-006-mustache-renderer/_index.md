---
title: "SRS-006: Server-Side Rendering — Mustache Templates"
weight: 6
---

**Status:** Specified, not started — no `render` code exists yet. This
document defines a **self-hosted Mustache template engine** (header-only,
zero third-party dependencies) whose behaviour is matched, tag for tag and
option for option, to [samskivert/jmustache](https://github.com/samskivert/jmustache),
and its integration as a **server-side renderer** that turns a compiled
template plus a data context into an `http::message` for
[`http::server`](../srs-005-http-protocol/05-server/).

**Author:** Arthur de Araújo Farias
**Date:** 2026-08-31

**Depends on:** [SRS-005](../srs-005-http-protocol/) (the `http::message`
container and `response_writer` the renderer writes into), and the vendored
`core::variant` / `core::object` / `core::array` containers SRS-005 §2
introduces (from [cxflow](https://github.com/arthurafarias/cxflow)'s
`containers::`) — the render **data context** is a `core::variant`, the same
container the HTTP layer already produces. No dependency on a network socket:
the engine is pure in-memory string work and is usable standalone.

The engine follows the [Mustache specification](https://github.com/mustache/spec)
(`mustache(5)`) for syntax and whitespace, and follows **jmustache** for
every place the spec leaves a choice open: missing-variable strictness,
compound paths, parent-context fallback, the `-first` / `-last` / `-index`
section variables, default values, and the compiler option surface. Because
C++ has no reflection, jmustache's "any Java object" data model is realised
as a `core::variant` tree plus a `custom_context` escape hatch. Lives at
`include/lambdatech/networking/render/`, namespace
`lambdatech::networking::render`, conventionally aliased
`namespace mustache = lambdatech::networking::render::mustache;`.

## Sections

| # | Section |
|---|---|
| 1 | [Purpose](01-purpose) |
| 2 | [Scope & Data Model](02-scope-and-data-model) |
| 3 | [The Compiler & Configuration](03-compiler) |
| 4 | [Template Syntax & Semantics](04-syntax) |
| 5 | [Values, Sections & Lambdas](05-values-sections-lambdas) |
| 6 | [Partials & Loaders](06-partials) |
| 7 | [HTTP Integration](07-http-integration) |
| 8 | [Non-Functional Requirements](08-nfr) |
| 9 | [Milestones](09-milestones) |
