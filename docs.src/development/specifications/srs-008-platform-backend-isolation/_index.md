---
title: "SRS-008: Platform Backend Isolation"
weight: 8
---

**Status:** In progress (M1–M2) — the `posix` backend of `core::descriptor`,
`core::poller`, `core::socket_ops`, and `core::resolver` is implemented and
`event_loop` / `tcp::*` / `udp::peer` are ported onto it. The `standalone`
backend (M3) is scaffolded but not functional.

**Author:** Arthur de Araújo Farias
**Date:** 2026-09-05

**Depends on:** [SRS-002](../srs-002-async-network-core/) (the `event_loop`
this refactors) and [SRS-003](../srs-003-protocol-clients-servers/) (the
`tcp::*` / `udp::peer` consumers that are ported).

Every operating-system dependency the networking core has — raw descriptor
I/O, readiness multiplexing, the BSD socket calls, and name resolution — is
pushed behind a small set of **facility façades** in
`lambdatech::networking::core`, each selecting one **backend** at compile
time by namespace alias. `event_loop` and the protocol layer name only the
façades; no header above `core/impl/` includes `<poll.h>`, `<sys/socket.h>`,
`<netdb.h>`, or `<netinet/in.h>`.

The pattern is the one prototyped in `core/descriptor.hpp`, generalised and
made consistent across all four facilities.

## Sections

| # | Section |
|---|---|
| 1 | [Purpose](01-purpose) |
| 2 | [The Backend Pattern](02-backend-pattern) |
| 3 | [Facility: descriptor](03-descriptor) |
| 4 | [Facility: poller](04-poller) |
| 5 | [Facilities: socket_ops & resolver](05-socket-and-resolver) |
| 6 | [Non-Functional Requirements](06-nfr) |
| 7 | [Milestones](07-milestones) |
