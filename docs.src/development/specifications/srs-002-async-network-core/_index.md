---
title: "SRS-002: Asynchronous Network Core"
weight: 2
---

**Status:** Implemented (M1–M3) — the vendored async primitives,
`core::event`, and the `poll(2)` `event_loop` with fd watches, deferred work,
and timers are all in place and covered by the `core::event_loop` test group.
An `io_uring` backend (M4) is not started.

**Author:** Arthur de Araújo Farias
**Date:** 2026-08-31

**Depends on:** the [cxflow](https://github.com/arthurafarias/cxflow) library's
`threading::task` / `threading::thread_pool` / `threading::signal`, vendored
verbatim (namespace-swapped) into `lambdatech::networking::core`.

The `core` runtime every protocol runs on: one dedicated loop thread driving
a `poll()` cycle, with a Node.js `EventEmitter`-style event primitive layered
on top. Lives at `include/lambdatech/networking/core/`.

## Sections

| # | Section |
|---|---|
| 1 | [Purpose](01-purpose) |
| 2 | [Vendored Async Primitives](02-vendored-primitives) |
| 3 | [core::event](03-event) |
| 4 | [The Event Loop](04-event-loop) |
| 5 | [Non-Functional Requirements](05-nfr) |
| 6 | [Milestones](06-milestones) |
