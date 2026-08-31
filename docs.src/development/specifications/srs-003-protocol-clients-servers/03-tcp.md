---
title: "3. tcp::server & tcp::client"
weight: 30
---

## 3. tcp::server & tcp::client

Implemented in
[protocol/tcp/server.hpp](../../../../include/lambdatech/networking/protocol/tcp/server.hpp)
and
[protocol/tcp/client.hpp](../../../../include/lambdatech/networking/protocol/tcp/client.hpp).

### REQ-3.1 — server: listen and accept

`server::listen(port, host = "0.0.0.0")` resolves `host`, creates a
non-blocking listening socket with `SO_REUSEADDR`, binds, listens, reads back
the bound address (so `port` 0 yields a discoverable ephemeral port), and
emits `listening`. Each `accept()` produces a `tcp::client` (adopted fd,
state `open`), emitted as `connect` (`server::on_connect()`).

### REQ-3.2 — server: accepted-client lifetime

The server retains a `std::shared_ptr` to each accepted client until that
client emits `close`, then drops it. A listener that keeps its own copy
extends the lifetime as expected.

### REQ-3.3 — client: connect

`client::connect(port, host)` runs `getaddrinfo` on `core::thread_pool`,
then `defer()`s the socket creation and non-blocking `connect()` onto the
loop. `POLLOUT` readiness with `SO_ERROR == 0` transitions to `open` and
emits `connect`.

### REQ-3.4 — client: write and backpressure

`write(bytes)` appends to an internal buffer and sends what the kernel will
take immediately. It returns `false` if any bytes remain buffered — the
caller should wait for `drain`, which fires when the buffer empties.

### REQ-3.5 — client: read and half-close

`POLLIN` drains with `recv()` in a loop, emitting `data` per chunk. A
zero-length `recv` emits `end` (peer FIN); with the default
`allowHalfOpen == false` the client then closes its side too. `end()`
half-closes after the write buffer flushes; `destroy()` closes immediately.

### REQ-3.6 — peer address

`client::remote_address()` returns the `core::socket_address` of the peer,
populated on connect or accept.
