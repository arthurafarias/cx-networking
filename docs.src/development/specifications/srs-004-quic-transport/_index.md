---
title: "SRS-004: QUIC Transport"
weight: 4
---

**Status:** Specified — not started. QUIC needs a TLS 1.3 stack and a
protocol state machine, both third-party, so per the
[Plugin Folder Convention](../guidelines/plugin-folder-convention/) it is
**not** part of the header-only core: it lives at `plugins/quic/` as the
optional, dependency-detected target `lambdatech-networking-plugin-quic`.

**Author:** Arthur de Araújo Farias
**Date:** 2026-08-31

**Depends on:** [SRS-002](../srs-002-async-network-core/) (the `core`
runtime — event loop, timers, `core::event`), [SRS-003](../srs-003-protocol-clients-servers/)
(`udp::peer`, which owns the datagram socket), and an external QUIC library
(ngtcp2 as the reference choice) plus a TLS 1.3 backend (quictls / OpenSSL
≥ 3.5 / BoringSSL).

The IETF QUIC v1 transport (RFC 9000 / 9001 / 9002), wrapped in the same
`std::shared_ptr` + `on_<name>()` surface as the other protocols. A
`quic::endpoint` multiplexes many connections over one `udp::peer`; each
`quic::connection` carries ordered, flow-controlled `quic::stream`s.
Namespace `lambdatech::networking::plugins::quic`.

## Sections

| # | Section |
|---|---|
| 1 | [Purpose](01-purpose) |
| 2 | [Scope & Dependencies](02-scope-and-dependencies) |
| 3 | [quic::endpoint](03-endpoint) |
| 4 | [quic::connection](04-connection) |
| 5 | [quic::stream](05-stream) |
| 6 | [quic::client & quic::server](06-client-server) |
| 7 | [Non-Functional Requirements](07-nfr) |
| 8 | [Milestones](08-milestones) |
