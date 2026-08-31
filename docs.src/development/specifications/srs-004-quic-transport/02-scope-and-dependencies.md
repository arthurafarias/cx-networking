---
title: "2. Scope & Dependencies"
weight: 20
---

## 2. Scope & Dependencies

### REQ-2.1 — plugin placement

QUIC requires an external TLS 1.3 implementation and an external QUIC
protocol state machine. Per the
[Plugin Folder Convention](../guidelines/plugin-folder-convention/) it must
not appear under `include/lambdatech/networking/protocol/`. It lives at:

```
plugins/quic/
  CMakeLists.txt
  include/lambdatech/networking/plugins/quic/
    endpoint.hpp  connection.hpp  stream.hpp  client.hpp  server.hpp
    tls_context.hpp
    testing/handshake_test.hpp  testing/stream_test.hpp
```

Namespace `lambdatech::networking::plugins::quic`, conventionally aliased
`namespace quic = lambdatech::networking::plugins::quic;`.

### REQ-2.2 — dependency detection

`plugins/quic/CMakeLists.txt` does its own `pkg_check_modules()` for the QUIC
library and the TLS backend and defines
`option(LAMBDATECH_NETWORKING_PLUGIN_QUIC ... ${deps_FOUND})`. If either
dependency is missing it produces **no targets** and the top-level build is
unaffected. When present it builds target
`lambdatech-networking-plugin-quic` (links `lambdatech-networking` + the
deps) and a gated `lambdatech-networking-plugin-quic-tests` binary.

### REQ-2.3 — reference dependency set

The M1 implementation targets **ngtcp2** for the transport state machine
(packetization, loss recovery, congestion control, ACK handling, flow
control, connection-ID management) and **quictls / OpenSSL ≥ 3.5 / BoringSSL**
for TLS 1.3 with the QUIC key export. The plugin owns only the glue: driving
ngtcp2's callbacks from `udp::peer` events and `core::event_loop` timers, and
surfacing the result as `core::event<...>` accessors. Swapping in another
backend (msquic, lsquic, picoquic) must not change the public headers.

### REQ-2.4 — in scope

QUIC v1 (RFC 9000) long/short header packets, version negotiation, the
TLS 1.3 handshake (RFC 9001), loss detection and congestion control
(RFC 9002) as provided by the backend, bidirectional and unidirectional
streams with flow control, connection close (idle timeout, explicit,
stateless reset), keep-alive (`PING`), 0-RTT resumption, and the
unreliable-datagram extension (RFC 9221).

### REQ-2.5 — out of scope

HTTP/3 and QPACK, DNS-over-QUIC, connection migration across local
interfaces (path validation is handled by the backend but the endpoint binds
one local address), the `preferred_address` transport parameter, and any
QUIC version other than v1. These are future SRSs or explicitly declined.
