---
title: "1. Purpose"
weight: 10
---

## 1. Purpose

QUIC is the transport the rest of the modern stack now assumes: HTTP/3,
DNS-over-QUIC (RFC 9250), and any low-latency service that wants TCP's
reliability without its head-of-line blocking or its one-RTT (often
three-RTT, with TLS) connection setup. LambdaTech Networking already has the
two things QUIC is built from — a `poll(2)` loop with timers (SRS-002) and a
datagram socket (`udp::peer`, SRS-003) — but not the connection, stream,
loss-recovery, and TLS-integration machinery that turns datagrams into a
QUIC transport.

This SRS specifies that layer: a `quic::endpoint` bound to one `udp::peer`,
the `quic::connection` it multiplexes, and the `quic::stream`s a connection
carries, all exposed through the project's standard `std::shared_ptr` +
`on_<name>()` event surface (SRS-003 §2). It deliberately does **not**
specify HTTP/3 or DoQ — those are consumers of this transport and belong to
later SRSs.

The design constraint is that a caller who has used `tcp::client` should be
able to open a `quic::stream` with almost the same code: `connect`, wait for
`connect`, `write`, receive `data`, observe `end` / `close`. QUIC's extra
surface — multiple streams, per-stream and connection flow control, 0-RTT,
connection migration, the datagram extension — is additive and opt-in.
