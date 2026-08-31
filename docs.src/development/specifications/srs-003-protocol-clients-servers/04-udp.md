---
title: "4. udp::peer"
weight: 40
---

## 4. udp::peer

Implemented in
[protocol/udp/peer.hpp](../../../../include/lambdatech/networking/protocol/udp/peer.hpp).

### REQ-4.1 — create

`udp::peer::create(family = "udp4")` — `"udp4"` → `AF_INET`, `"udp6"` →
`AF_INET6`. The socket is created lazily on the first `bind()` or `send()`.

### REQ-4.2 — bind

`bind(port = 0, address = "0.0.0.0")` binds a non-blocking datagram socket
(`SO_REUSEADDR`), reads back the local address, starts reading, and emits
`listening`. `"udp6"` maps the default `address` to `"::"`.

### REQ-4.3 — send

`send(datagram, port, host)` resolves `host` (numeric hosts — the DNS case —
resolve instantly), then `sendto()`. Calling `send()` before `bind()` opens
an ephemeral socket and still starts reading, so replies are delivered.
Returns `false` on failure (and emits `error`).

### REQ-4.4 — message

`POLLIN` drains with `recvfrom()` in a loop, emitting `message` with the
datagram (`core::buffer`) and the sender `core::socket_address`. A
zero-length datagram is delivered as an empty buffer, not treated as EOF.

### REQ-4.5 — address

`peer::address()` returns the bound local `core::socket_address`.
