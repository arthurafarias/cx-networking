---
title: "3. quic::endpoint"
weight: 30
---

## 3. quic::endpoint

The `quic::endpoint` owns one `udp::peer` and demultiplexes every datagram
on it to a `quic::connection` by Destination Connection ID (DCID). Both
`quic::client` and `quic::server` are thin wrappers over an endpoint.

### REQ-3.1 — create and bind

`endpoint::create(loop, tls_context)` returns a `std::shared_ptr`. `bind(port
= 0, address = "0.0.0.0", family = "udp4")` creates the underlying
`udp::peer`, binds it, and emits `listening`; `port` 0 yields a discoverable
ephemeral port via `peer::address()`. A client endpoint may skip `bind()` —
the first `connect()` opens an ephemeral `udp::peer` (SRS-003 REQ-4.3).

### REQ-3.2 — inbound demux

Each `udp::peer` `message` event carries `(core::buffer datagram,
core::socket_address from)`. The endpoint parses the QUIC header, extracts
the DCID, and routes:

- known DCID → that connection's receive path;
- unknown DCID with a long-header Initial packet, on a server endpoint →
  new-connection path (REQ-3.4);
- otherwise → dropped, optionally answered with a Version Negotiation or
  Stateless Reset packet as the backend directs.

Connection IDs issued by this endpoint are tracked so a peer that migrates
or retires a CID still resolves.

### REQ-3.3 — outbound

The backend hands the endpoint fully-formed datagrams plus a destination
`core::socket_address`; the endpoint calls `peer::send(...)`. All sends
happen on the loop thread. Pacing timers requested by the backend are armed
with `core::event_loop::set_timeout`.

### REQ-3.4 — server acceptance

A server endpoint is put in listening mode with `listen(on_connection)`. On
a valid Initial it runs Retry / token validation as configured, creates a
`quic::connection` in state `handshaking`, and emits it via `connection`
once the handshake completes (state `open`). A handshake that fails emits
`connection` never — only an `error` on the endpoint.

### REQ-3.5 — timers

The endpoint keeps at most one `core::event_loop` timer armed per
connection (the nearest of the backend's loss-detection, idle, and pacing
deadlines) plus its own. When a timer fires it calls the backend's expiry
handler and then flushes any resulting packets.

### REQ-3.6 — events

`listening`, `connection` (`std::shared_ptr<quic::connection>`, server
only), `error` (`std::string`), `close`. `close()` closes every live
connection with an application error code, then closes the `udp::peer`;
`close` fires once.
