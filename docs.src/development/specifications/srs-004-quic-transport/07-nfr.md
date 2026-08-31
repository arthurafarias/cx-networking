---
title: "7. Non-Functional Requirements"
weight: 70
---

## 7. Non-Functional Requirements

### NFR-1 — plugin, not core

Nothing in `include/lambdatech/networking/` gains a QUIC dependency. The core
and all three protocols keep building with the STL and pthreads alone; a
checkout without a QUIC library or a TLS backend builds and tests everything
else unchanged (REQ-2.2).

### NFR-2 — no blocking on the loop thread

Every ngtcp2 / TLS call runs on the event-loop thread and must be non-
blocking. Name resolution for `connect` is the only blocking call and runs on
`core::thread_pool` (SRS-002), as with `tcp::client`. Certificate and key
loading happens once, at `tls_context` construction, off the hot path.

### NFR-3 — one datagram socket per endpoint

An endpoint multiplexes all its connections and streams over a single
`udp::peer`. Thousands of connections and streams must not scale the fd
count; they scale the connection table and the per-connection backend state
only.

### NFR-4 — safe teardown

Dropping the last `shared_ptr` to an endpoint, connection, or stream, from
any thread, at any handshake or data-transfer stage, must not use-after-free.
Backend callbacks resolve the owning object through a `std::weak_ptr` and
no-op if it has expired (SRS-003 REQ-2.1 / NFR-3).

### NFR-5 — bounded, offline tests

The `handshake` and `stream` test groups run a `quic::server` and
`quic::client` against `127.0.0.1` ephemeral ports with a throwaway
self-signed `tls_context`, drive them through one `core::event_loop`, and
wrap every wait in `testing::await(future, timeout)` (SRS-003 NFR-4). No test
touches the network or a real CA.

### NFR-6 — backend-agnostic public headers

The headers under `plugins/quic/include/` expose no ngtcp2 or OpenSSL type.
Replacing the backend (REQ-2.3) changes only the `.hpp` implementation
bodies and `CMakeLists.txt`, never a signature a consumer compiled against.

### NFR-7 — spec conformance is delegated

RFC 9000/9001/9002 wire conformance, loss recovery, and congestion control
correctness are the backend's responsibility and are not re-tested here; the
plugin's tests cover only the glue (demux, event surface, lifetime, flow-
control backpressure signalling).
