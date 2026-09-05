---
title: "1. Purpose"
weight: 10
---

## 1. Purpose

[SRS-002](../srs-002-async-network-core/) built the reactor directly on
`poll(2)` and `eventfd(2)`, and [SRS-003](../srs-003-protocol-clients-servers/)
built `tcp::*` / `udp::peer` directly on the BSD socket calls. That was the
right first move — it is the smallest thing that works — but it leaves the
POSIX API threaded through every header:

- `event_loop.hpp` names `pollfd`, `POLLIN`, `POLLOUT`, `eventfd`, and calls
  `::poll` / `::read` / `::write` inline;
- `tcp/socket.hpp`, `tcp/server.hpp`, `udp/peer.hpp` each `#include <sys/socket.h>`
  and call `::socket` / `::connect` / `::accept` / `::recv` / `::sendto` / … by
  hand, and pass `short revents` masks around in their own callbacks;
- `core::socket_address` exposes `to_sockaddr` / `from_sockaddr`, so
  `<netinet/in.h>` types appear in a public core value type.

Three things need this to change:

1. **A test fabric with no kernel.** A `standalone` backend — an in-process
   loopback network and a deterministic readiness queue — lets the SRS-003
   test groups run with zero real sockets, no ephemeral-port races, and no
   `poll()` latency floor.
2. **Alternative production backends.** SRS-002 already lists an `io_uring`
   loop (M4) and a `kqueue` loop (M5). Each should be one file under
   `core/impl/`, selected at build time, with `event_loop` unchanged.
3. **A single, documented isolation pattern.** `core/descriptor.hpp` already
   prototypes it (a per-backend `namespace impl::posix` / `impl::standalone`,
   a `namespace current = …` switch, a forwarding façade). This SRS makes
   that pattern the rule and applies it uniformly.

This SRS specifies the pattern (§2) and the four facilities it is applied to:
`descriptor` (§3), `poller` (§4), `socket_ops` and `resolver` (§5).

### 1.1 Non-goals

- No runtime backend selection. The switch is a compile-time namespace alias;
  one backend per facility per binary (§2.4 covers the escape hatch).
- No change to the public `tcp::*` / `udp::peer` / `dns::*` event surface —
  SRS-003 is a consumer of this work, not a subject of it.
- No new link dependencies. The core stays header-only over the STL and
  pthreads (SRS-002 NFR-1).
