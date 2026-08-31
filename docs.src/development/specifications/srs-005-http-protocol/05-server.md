---
title: "5. http::server"
weight: 50
---

## 5. http::server

Implemented in
[protocol/http/server.hpp](../../../../include/lambdatech/networking/protocol/http/server.hpp).
A layer over
[`tcp::server`](../../srs-003-protocol-clients-servers/03-tcp/): one
`http::parser` in `request`
mode per accepted connection.

### REQ-5.1 — listen

```c++
static std::shared_ptr<server> create(core::event_loop &loop = core::event_loop::instance());
void listen(std::uint16_t port, std::string host = "0.0.0.0");
const core::socket_address &address() const;   // bound address, port 0 → ephemeral
```

`listen` delegates to `tcp::server::listen` and re-emits its `listening` and
`error` events unchanged (`on_listening()`, `on_error()`).

### REQ-5.2 — per-connection state

On each `tcp::server` `connection` the server creates a `parser` and holds
it alongside the accepted `tcp::client` (whose lifetime `tcp::server`
already manages until its `close`, SRS-003 REQ-3.2). Socket `data` →
`parser.feed`; socket `end` / `close` → `parser.finish`. A `parser`
`on_error` sends `400 Bad Request` with `Connection: close` and destroys the
connection.

### REQ-5.3 — the `request` event

```c++
core::event<const message &, const response_writer &> &on_request();
```

Emitted once per fully parsed request. The `response_writer` is bound to
that request's connection and request id (for pipelining order).

### REQ-5.4 — response_writer

```c++
class response_writer {
public:
  void send(message reply) const;        // headers + body, once
  const core::socket_address &peer_address() const;
  std::string request_line() const;      // the caller's start line, for logging
  bool keep_alive() const;               // whether the connection will survive send()
};
```

`send`:

1. if `reply` has no start line, sets `HTTP/1.1 200 OK`; otherwise forces the
   version to match the request (HTTP/1.0 stays 1.0).
2. sets `Content-Length` from `body` unless `Transfer-Encoding` is set.
3. sets `Date` and `Server: lambdatech-networking/0` if absent.
4. decides persistence: HTTP/1.1 defaults keep-alive unless the request or
   the reply says `Connection: close`; HTTP/1.0 defaults close unless both
   say `Connection: keep-alive`. Adds the matching `Connection` header.
5. `write`s `reply.serialize()` to the connection's `tcp::client`. If not
   persistent, `end()`s the socket after the write flushes.

Calling `send` twice on one `response_writer`, or never calling it, is a
programming error; the server logs via `on_error` and closes the connection
after a configurable header-to-response timeout (default 30 s).

### REQ-5.5 — pipelining and ordering

M1 processes one request at a time per connection: the parser holds
subsequent pipelined requests (bounded by `limits.max_pipelined`) and the
server emits the next `request` only after the current `response_writer`
has `send()`. This guarantees responses leave in request order without a
reordering buffer. Concurrent handling with an ordered write queue is M5.

### REQ-5.6 — no routing here

`http::server` only turns a connection into a stream of `(message,
response_writer)` pairs. Matching a request to a handler is `http::router`
(§6); an application may also just subscribe to `on_request()` directly and
branch on `msg.method()` / `msg.target()`.
