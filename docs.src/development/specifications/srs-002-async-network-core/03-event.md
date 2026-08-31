---
title: "3. core::event"
weight: 30
---

## 3. core::event

Implemented in
[core/event.hpp](../../../../include/lambdatech/networking/core/event.hpp).

### REQ-3.1 — one event, one signal

`core::event<Args...>` owns one `core::signal<Args...>`. It is the unit a
socket exposes per named event (`on_data()`, `on_close()`, …).

### REQ-3.2 — EventEmitter surface

| Method | Node analogue |
|---|---|
| `operator+=(listener)` | `emitter.on(name, fn)` — proxies straight to `signal::connect` |
| `once(listener)` → `subscription` | `emitter.once(name, fn)` — self-removes after the first emission |
| `off_all()` | `emitter.removeAllListeners(name)` |
| `listener_count()` | `emitter.listenerCount(name)` |
| `emit(args...)` | internal — the socket, not the consumer, calls this |

### REQ-3.3 — subscription handle

`subscription` is `core::signal::connection`: `disconnect()` removes exactly
that listener and is safe to call after the owning event is destroyed.

### REQ-3.4 — synchronous, ordered

Listeners run synchronously in subscription order on whatever thread calls
`emit()` — which, for every socket type, is always the event-loop thread.
