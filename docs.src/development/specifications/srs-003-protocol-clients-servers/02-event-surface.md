---
title: "2. The Event Surface"
weight: 20
---

## 2. The Event Surface

### REQ-2.1 — shared_ptr ownership

Every socket type derives from `std::enable_shared_from_this` and is
constructed through a static `create(...)` factory returning
`std::shared_ptr`. The event loop stores a `std::weak_ptr` in each fd
callback and no-ops if it has expired, so tearing down a socket is always
safe.

### REQ-2.2 — `on_<name>()` accessors

Each named event is exposed as a method `on_<name>()` returning
`core::event<Args...> &`. Consumers add a listener with `+= fn` (it proxies
straight to `signal::connect`) or `.once(fn)`; the socket internals call
`.emit(...)`.

### REQ-2.3 — listeners run on the loop thread

Every `emit()` happens on the event-loop thread. A listener may freely call
back into the socket (`write`, `close`, …).

### REQ-2.4 — methods are thread-safe

`connect`, `write`, `send`, `end`, `close`, `destroy` may be called from any
thread: they take a short internal lock and, where needed, wake the loop.
The idiomatic pattern is still to drive everything from listeners.

### REQ-2.5 — error then close

A fatal error emits `error` (with a `std::string` message) and is then
always followed by `close`. `close` fires exactly once.
