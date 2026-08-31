---
title: "6. quic::client & quic::server"
weight: 60
---

## 6. quic::client & quic::server

Both are convenience wrappers over a `quic::endpoint` that give QUIC the same
shape as `tcp::client` / `tcp::server`.

### REQ-6.1 — tls_context

`quic::tls_context` wraps the TLS backend's configuration:

- `tls_context::client(opts)` — trust store (system default or an explicit
  CA bundle / pinned key), ALPN list (required — QUIC mandates ALPN),
  optional client certificate, `verify` mode, optional session cache for
  0-RTT.
- `tls_context::server(cert, key, opts)` — certificate chain and private
  key (PEM paths or in-memory), ALPN list, optional SNI callback selecting
  a chain per server name, 0-RTT enable, `require_client_cert`.

A context is immutable after construction and may back many endpoints.

### REQ-6.2 — client

`quic::client::create(loop, tls_context)`. `connect(port, host, opts)` binds
an ephemeral endpoint if not already bound and starts one connection;
`opts.server_name` overrides the SNI/verification name (default: `host`).
The client re-emits its single connection's `connect` / `error` / `close`
and exposes `connection()` for the full surface (§4). `open_stream()` and
`send_datagram()` are forwarded for the common one-connection case.

### REQ-6.3 — server

`quic::server::create(loop, tls_context)`. `listen(port = 0, address =
"0.0.0.0")` binds the endpoint and puts it in accept mode; each completed
handshake emits `connection` (`std::shared_ptr<quic::connection>`). The
server retains each connection until it emits `close` (SRS-003 REQ-3.2). It
re-emits the endpoint's `listening` / `error` / `close`.

### REQ-6.4 — Retry and address validation

`server` defaults to sending a Retry packet (stateless address validation)
above a configurable in-flight-handshake threshold, and validates the token
on the follow-up Initial. `listen(..., {retry: always | adaptive | never})`
overrides.

### REQ-6.5 — Node parity note

There is no Node.js `dgram`/`net` equivalent for QUIC in stable Node, so the
naming follows this project's own `tcp::`/`udp::` precedent rather than a
Node module. The `experimental node:quic` API (`QuicEndpoint`,
`QuicSession`, `QuicStream`) is the closest reference and the event names are
kept compatible with it where they overlap (`connect`, `stream`, `close`).
