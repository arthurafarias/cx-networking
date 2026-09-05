---
title: "5. Non-Functional Requirements"
weight: 50
---

## 5. Non-Functional Requirements

### NFR-1 — header-only

The entire `core` runtime is headers under
`include/lambdatech/networking/core/`. The only link dependency is
`Threads::Threads` (pthreads).

### NFR-2 — Linux/BSD

The `posix` `core::poller` backend uses `poll(2)` and `eventfd(2)`. A
`kqueue` fallback for the BSDs and an `io_uring` / `epoll` backend are future
work ([SRS-008](../srs-008-platform-backend-isolation/) M4–M5); since SRS-008
`event_loop` names `core::poller` only and leaks no `poll` specifics.

### NFR-3 — no busy-waiting

The loop blocks in `poll()` with the next timer deadline as its timeout, or
indefinitely when there are no timers. Every operation that changes loop
state writes the wake fd so `poll()` returns promptly.

### NFR-4 — bounded shutdown

`stop()` returns only after the loop thread has been joined; it must not
hang if the loop is currently blocked in `poll()`.

### NFR-5 — restartable

`start()` after `stop()` produces a working loop again (a fresh loop thread),
so the type is usable as a test fixture.
