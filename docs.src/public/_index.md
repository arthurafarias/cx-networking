---
title: "LambdaTech Networking"
---

LambdaTech Networking is a header-only, zero-dependency asynchronous networking
toolkit for C++23. It layers cleanly:

- **`core`** — the runtime: a `poll(2)` **event loop** (fd readiness, deferred
  work, timers), a Node.js `EventEmitter`-style `event<>` primitive, address
  resolution, and byte buffers. The async machinery under `core` — `task`,
  `thread_pool`, `signal` — is vendored from the
  [cxflow](https://github.com/arthurafarias/cxflow) library, the same design
  that drives its dataflow pipelines.
- **`protocol::tcp`** — `tcp::server` and `tcp::client`, modeled on Node's
  `net.Server` / `net.Socket`.
- **`protocol::udp`** — `udp::peer`, modeled on Node's `dgram.Socket`.
- **`protocol::dns`** — a bounds-checked wire reader/writer, a `message` type,
  and a `dns::client` (stub resolver) / `dns::server` pair built on `udp::peer`.

Everything runs on one event-loop thread, so listeners never race each other;
blocking work (name resolution) is off-loaded to `core::thread_pool` and its
result handed back with `event_loop::defer()`.

The project is in an early conceptual stage — the core loop, all three
transports, and the DNS request/response round trip are proven end-to-end by
the self-hosted test suite. DNS resource-record parsing, name compression on
write, TLS/DoT, a QUIC transport (the optional `plugins/quic/` component), and
an `io_uring` backend are specified or planned but not yet implemented.
