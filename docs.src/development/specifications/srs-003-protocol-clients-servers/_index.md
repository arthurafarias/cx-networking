---
title: "SRS-003: Protocol Clients & Servers"
weight: 3
---

**Status:** Implemented (M1–M4) — `tcp::server`/`tcp::client`, `udp::peer`,
and `dns::client`/`dns::server` all build and are covered by the
`tcp::client+server`, `udp::peer`, and `dns::client+server` test groups, each
exercising a real loopback socket through the event loop. Backpressure
refinements and DNS resource-record support (M5) depend on
[SRS-001](../srs-001-dns-message-model/) M3.

**Author:** Arthur de Araújo Farias
**Date:** 2026-08-31

**Depends on:** [SRS-002](../srs-002-async-network-core/) (the `core`
runtime) and [SRS-001](../srs-001-dns-message-model/) (`dns::message`).

The Node.js-inspired transport surface: TCP, UDP, and DNS clients and
servers, each an `std::shared_ptr`-held object exposing its events as
`on_<name>()` accessors. Lives at
`include/lambdatech/networking/protocol/`.

## Sections

| # | Section |
|---|---|
| 1 | [Purpose](01-purpose) |
| 2 | [The Event Surface](02-event-surface) |
| 3 | [tcp::server & tcp::client](03-tcp) |
| 4 | [udp::peer](04-udp) |
| 5 | [dns::client & dns::server](05-dns) |
| 6 | [Non-Functional Requirements](06-nfr) |
| 7 | [Milestones](07-milestones) |
