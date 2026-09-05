---
title: "4. The Event Loop"
weight: 40
---

## 4. The Event Loop

Implemented in
[core/event_loop.hpp](../../../../include/lambdatech/networking/core/event_loop.hpp).

> **Superseded in part by [SRS-008](../srs-008-platform-backend-isolation/).**
> The `poll(2)` cycle and the wake `eventfd` now live behind `core::poller`;
> `event_loop` holds one `poller::state` and calls `poller::wait` /
> `poller::interrupt`. The watch API takes `poller::interest` instead of a
> `short` mask and the `io_callback` receives a `poller::ready` struct instead
> of `short revents`. The ordering, threading, and lifecycle guarantees below
> are unchanged.

### REQ-4.1 — one task, one poll cycle

`core::event_loop` owns one `core::task`. Each iteration builds a `pollfd`
array from the watch table plus a wake `eventfd`, calls `poll()` with the
next timer deadline as the timeout, then services results in this order:
drain the wake fd → run deferred functions (FIFO) → fire due timers → invoke
ready-fd callbacks.

### REQ-4.2 — fd watches

`watch(fd, events, cb)` registers or replaces an fd's `POLLIN`/`POLLOUT`
interest and callback; `modify(fd, events)` changes the mask; `unwatch(fd)`
drops it. All three are safe to call from any thread and take effect on the
next cycle (they write the wake fd).

### REQ-4.3 — deferred work

`defer(fn)` queues `fn` to run on the loop thread at the start of the next
cycle — the `process.nextTick` equivalent, used to marshal an off-loop
result (e.g. a completed `getaddrinfo`) back onto the loop.

### REQ-4.4 — timers

`set_timeout(delay, fn)` returns a `timer_id`; `clear_timeout(id)` cancels
it. Timers fire on the loop thread. `dns::client` uses one per outstanding
query for its timeout.

### REQ-4.5 — lifecycle

`start()` runs the loop on its own thread and is idempotent; `run()` blocks
the caller until another thread calls `stop()`; `stop()` requests shutdown,
wakes the `poll()`, and joins. A loop may be restarted after `stop()`.

### REQ-4.6 — callback isolation

If a callback removes an fd that appears later in the same cycle's ready
list, that fd is skipped (looked up fresh under the lock); a callback that
adds an fd is picked up next cycle. No callback ever runs while the internal
lock is held.
