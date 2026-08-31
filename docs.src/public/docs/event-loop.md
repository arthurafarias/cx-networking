---
title: "The Event Loop"
weight: 30
---

`core::event_loop` ([core/event_loop.hpp](../../../../include/lambdatech/networking/core/event_loop.hpp))
is the single-threaded reactor every socket runs on. It is one `core::task`
(a dedicated thread) driving one `poll(2)` cycle.

## One cycle

```
poll(watched fds + wake eventfd, next timer deadline)
  │
  ├─ drain the wake eventfd
  ├─ run every defer()'d function            (FIFO)
  ├─ fire every timer whose deadline passed
  └─ for each ready fd: invoke its callback  (revents)
```

The wake `eventfd` is written by `watch/modify/unwatch/defer/set_timeout` from
any thread, so a change made off the loop takes effect on the very next cycle
instead of waiting out the current `poll()`.

## API

| Method | Purpose | Node analogue |
|---|---|---|
| `watch(fd, events, cb)` | register/replace an fd's interest + callback | — |
| `modify(fd, events)` | change an fd's `POLLIN`/`POLLOUT` interest | — |
| `unwatch(fd)` | stop watching an fd | — |
| `defer(fn)` | run `fn` on the loop thread ASAP | `process.nextTick` |
| `set_timeout(delay, fn)` → `timer_id` | run `fn` after `delay` | `setTimeout` |
| `clear_timeout(id)` | cancel a pending timer | `clearTimeout` |
| `start()` | run the loop on its own thread (idempotent) | — |
| `run()` | block the caller until `stop()` | `process` staying alive |
| `stop()` | stop the loop and join its thread | — |

## Threading contract

Callbacks and event listeners **only ever run on the loop thread**. Socket
methods (`connect`, `write`, `send`, `close`, …) are safe to call from other
threads — they take a short internal lock and wake the loop — but the cleanest
pattern mirrors Node: do everything from inside listeners, and marshal work in
from other threads with `defer()`.

Blocking calls (`getaddrinfo`, file I/O) must not run on the loop thread.
`tcp::client::connect` and `udp::peer::send` already push name resolution onto
`core::thread_pool` and hand the result back via `defer()`.
