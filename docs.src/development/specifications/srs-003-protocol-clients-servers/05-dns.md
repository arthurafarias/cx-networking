---
title: "5. dns::client & dns::server"
weight: 50
---

## 5. dns::client & dns::server

Implemented in
[protocol/dns/client.hpp](../../../../include/lambdatech/networking/protocol/dns/client.hpp)
and
[protocol/dns/server.hpp](../../../../include/lambdatech/networking/protocol/dns/server.hpp).
Both are thin layers over `udp::peer` + `dns::message` (SRS-001).

### REQ-5.1 — client: query

`client::create(resolver_host = "127.0.0.1", resolver_port = 53)`.
`query(name, type, on_answer)` builds a `dns::message` with a random id and
`RD` set, serializes it, and `send()`s it via a `udp::peer`. The handler is
`std::function<void(std::optional<dns::message>)>`.

### REQ-5.2 — client: matching and timeout

Outstanding queries are keyed by their 16-bit id. An incoming datagram is
parsed and, on an id match, delivers the message to that query's handler.
`set_timeout(duration)` (default 5 s) arms a `core::event_loop` timer per
query; if it fires first the handler is called with `std::nullopt` and the
entry is dropped. Exactly one of the two paths ever fires per query.

### REQ-5.3 — client: errors

`on_error()` reports socket failures and malformed responses. A malformed
response does not resolve any pending query (it may be spoofed / late).

### REQ-5.4 — server: listen

`server::create()`; `listen(port = 53, address = "0.0.0.0")` binds a
`udp::peer` and re-emits its `listening` / `error`. Each datagram is parsed;
a parse failure emits `error` and is dropped.

### REQ-5.5 — server: query & responder

A parsed request is emitted as `query` with `(const dns::message &,
const responder &)`. `responder::send(reply)` forces `reply.header.id` to the
request id and `reply.header.response = true`, serializes, and sends it back
to the querier's address. `responder::peer_address()` / `request_id()` are
available for logging.
