---
title: "8. Milestones"
weight: 80
---

## 8. Milestones

| Milestone | Covers | Status |
|---|---|---|
| **M1** | `plugins/quic/` scaffold per the Plugin Folder Convention: `pkg_check_modules` for ngtcp2 + a TLS backend, `option(LAMBDATECH_NETWORKING_PLUGIN_QUIC)`, gated targets. `tls_context` (client + server, self-signed helper for tests). | Not started |
| **M2** | `quic::endpoint` over `udp::peer`: bind, inbound DCID demux, outbound send, per-connection loss/idle/pacing timers on `core::event_loop`. | Not started |
| **M3** | `quic::connection` client path + `quic::stream` bidi: `connect`, `open_stream`, ordered `data`, `write` with `drain` backpressure, `end`, clean `close`. Test group `quic::handshake`. | Not started |
| **M4** | `quic::server` + `quic::client` wrappers, incoming `stream` / `connection` events, Retry / address validation, connection-level flow control (`data_capacity`). Test group `quic::stream`. | Not started |
| **M5** | Unidirectional streams, `RESET_STREAM` / `STOP_SENDING`, `CONNECTION_CLOSE` error propagation, stateless reset, keep-alive. | Not started |
| **M6** | 0-RTT / session resumption (session cache in `tls_context`), the RFC 9221 datagram extension (`send_datagram` / `datagram`). | Not started |
| **M7** | `plugins/doq/` — DNS-over-QUIC (RFC 9250) exposing the SRS-003 `dns::client` / `dns::server` API over a `quic::connection`, and an `lnw-example-doq-dig` example. | Not started |
| **M8** | Alternative backend behind the same headers (msquic or picoquic) selected at configure time, proving NFR-6. | Not started |
