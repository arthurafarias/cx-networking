---
title: "7. Milestones"
weight: 70
---

## 7. Milestones

| Milestone | Covers | Status |
|---|---|---|
| **M1** | `tcp::client` / `tcp::server` — connect, accept, read, write with backpressure, half-close. Test group `tcp::client+server`. | Done |
| **M2** | `udp::peer` — bind, send, message, lazy socket. Test group `udp::peer`. | Done |
| **M3** | `dns::client` — query/response matching by id, per-query timeout. Test group `dns::client+server`. | Done |
| **M4** | `dns::server` — `query` event + `responder`. `lnw-example-dns-dig`, `lnw-example-tcp-echo`. | Done |
| **M5** | Richer `dns::client` (retry, `SERVFAIL` fallover, multiple resolvers) and answer-section decoding once SRS-001 M3 lands. | Not started |
| **M6** | `tcp::client` connection pooling + `dns::client` running over TCP for truncated (`TC`) responses. | Not started |
| **M7** | A `plugins/tls/` DoT transport (OpenSSL) behind the same `dns::client` API. | Not started |
