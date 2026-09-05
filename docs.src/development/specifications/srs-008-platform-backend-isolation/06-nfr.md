---
title: "6. Non-Functional Requirements"
weight: 60
---

## 6. Non-Functional Requirements

### NFR-1 — no OS headers above `core/impl/`

After this SRS, a `grep` for `<poll.h>`, `<sys/socket.h>`, `<sys/eventfd.h>`,
`<netdb.h>`, `<netinet/in.h>`, `<arpa/inet.h>`, `<fcntl.h>` across
`include/lambdatech/networking/` returns hits only under
`core/impl/posix/`. `core/testing/*_test.hpp` may still include them to build
raw fixtures.

### NFR-2 — zero-cost

The façades are `inline` forwarding functions over namespace-aliased free
functions; there is no virtual dispatch, no type erasure, and no allocation
introduced. A release build is expected to be identical to hand-written
backend calls. The `concept` checks are compile-time only.

### NFR-3 — header-only, dependency-free

Unchanged from SRS-002 NFR-1: everything is headers under
`include/lambdatech/networking/core/`, link dependency `Threads::Threads`
only.

### NFR-4 — the public event surface is untouched

`tcp::server` / `tcp::socket` / `udp::peer` / `dns::*` keep the exact
`on_<name>()` events, argument types, and lifecycle SRS-003 specifies. The
only intentional break is internal: the `event_loop::io_callback` signature
(`short` → `poller::ready`), which no SRS-003 consumer names.

### NFR-5 — the standalone backend is selectable per test binary

Building the test facility with `-DLNW_NET_BACKEND_STANDALONE=1` compiles the
whole suite against the in-process backends and must link and run. Until M3
completes, groups that need a working fabric are allowed to be skipped under
that define; the `posix` build runs them all.

### NFR-6 — restartable & bounded shutdown preserved

SRS-002 NFR-4 / NFR-5 still hold: `poller::interrupt` unblocks a `wait` so
`event_loop::stop()` joins promptly, and a fresh `poller::state` after
`stop()`/`start()` gives a working loop.
