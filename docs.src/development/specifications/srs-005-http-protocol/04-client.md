---
title: "4. http::client"
weight: 40
---

## 4. http::client

Implemented in
[protocol/http/client.hpp](../../../../include/lambdatech/networking/protocol/http/client.hpp).
A thin layer over
[`tcp::client`](../../srs-003-protocol-clients-servers/03-tcp/) plus one
`http::parser` in
`response` mode.

### REQ-4.1 — create and request

```c++
static std::shared_ptr<client> create(core::event_loop &loop = core::event_loop::instance());

using response_handler = std::function<void(std::optional<message>)>;

void request(message req, response_handler on_response);
void request(std::string method, std::string url,
             message req, response_handler on_response);   // fills the request line + Host
```

`request` is fire-and-forget per call: it may be called before the loop
starts and from any thread (it defers onto the loop, like
`tcp::client::connect`). `on_response` is invoked exactly once — with the
parsed response, or `std::nullopt` on any failure (connect, write, parse,
timeout, early close) after `on_error()` has reported the reason.

### REQ-4.2 — target resolution

The request URL (from the 2-arg overload, or `req.url()` / the request-line
target plus `Host` for the 1-arg overload) is split into scheme, host, port,
and origin-form target:

- scheme `http` → default port 80. Scheme `https` (or any other) →
  `on_error("https requires the tls plugin")` and `std::nullopt`; no socket
  is opened.
- host is passed to `tcp::client::connect(port, host)`, so `getaddrinfo`
  runs on `core::thread_pool`, never the loop thread (SRS-003 REQ-3.3).
- the wire request line is rewritten to origin form (`GET /path?q HTTP/1.1`);
  `Host` is set from the URL authority if the caller did not supply it.

### REQ-4.3 — what the client fills in

Before serializing, and only when the caller has not already set them:
`Host`, `Content-Length` (from `body`), `Connection: close` (M1 — see
REQ-4.5), and `User-Agent: lambdatech-networking/0`. The caller's explicit
values always win, including an explicit empty `Host`.

### REQ-4.4 — lifecycle

1. `tcp::client::create`, wire `connect` / `data` / `error` / `close`.
2. on `connect`: `write(req.serialize())`.
3. `data` chunks are `feed()` into the response parser; if the request
   method was `HEAD`, `expect_no_body()` is set first.
4. parser `on_message` → `on_response(message)`; then M1 `destroy()`s the
   socket.
5. `tcp::client` `close` before a message completed → if the parser is in a
   "body to EOF" state, `finish()` (may still yield a message); otherwise
   `on_response(std::nullopt)`.
6. `set_timeout(std::chrono::milliseconds)` (default 30 s) arms one
   `event_loop` timer spanning connect + response; firing first →
   `on_error("request timed out")`, `on_response(std::nullopt)`, socket
   destroyed.

### REQ-4.5 — connection reuse is deferred

M1 opens one TCP connection per `request` and closes it when the response is
in (`Connection: close`). Keep-alive, a per-host idle-connection pool, and
request pipelining are M5 and are transparent to `response_handler` when
they land. Redirect following is never automatic — a `3xx` is delivered as
an ordinary response.

### REQ-4.6 — errors

`on_error()` (`core::event<const std::string &>`) fires for: unsupported
scheme, DNS failure, connect failure, a `tcp::client` `error`, any
`http::parser` error, and timeout. Every `on_error` for a given `request`
is followed by that request's `on_response(std::nullopt)`.
