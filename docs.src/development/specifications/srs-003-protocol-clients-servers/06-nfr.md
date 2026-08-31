---
title: "6. Non-Functional Requirements"
weight: 60
---

## 6. Non-Functional Requirements

### NFR-1 — header-only

All protocol types are headers under
`include/lambdatech/networking/protocol/`. Link dependency: pthreads only.

### NFR-2 — no blocking on the loop thread

The only blocking call any protocol makes is `getaddrinfo`, and that runs on
`core::thread_pool`, never the loop thread. Numeric-host `send()` (the DNS
path) resolves without blocking.

### NFR-3 — safe teardown

Destroying a socket's last `shared_ptr`, from any thread, at any time, must
not use-after-free: fd callbacks hold a `weak_ptr` and the destructor
`unwatch`es and closes the fd.

### NFR-4 — bounded tests

Every socket test group runs against `127.0.0.1` ephemeral ports and is
wrapped in `testing::await(future, timeout)` so a regression fails a case
rather than hanging CI.

### NFR-5 — Node parity is documented, not enforced

Where behaviour intentionally differs from Node (typed events, `shared_ptr`,
`core::buffer`, `allowHalfOpen` defaulting closed), the difference is called
out in this SRS and the docs.
