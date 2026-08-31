---
title: "2. Vendored Async Primitives"
weight: 20
---

## 2. Vendored Async Primitives

### REQ-2.1 — verbatim, namespace-swapped

[`core/task.hpp`](../../../../include/lambdatech/networking/core/task.hpp),
[`core/thread_pool.hpp`](../../../../include/lambdatech/networking/core/thread_pool.hpp),
and
[`core/signal.hpp`](../../../../include/lambdatech/networking/core/signal.hpp)
are copies of cxflow's `threading::{task,thread_pool,signal}` with only the
namespace (`cxflow::threading` → `lambdatech::networking::core`) and include
paths changed. Each file's header comment records its origin.

### REQ-2.2 — `task`

A dedicated-thread repeating-loop runner: `start()` spawns a `std::jthread`
that calls the loop function until `stop()`. `pause()`/`resume()` gate it
before the next iteration. `core::event_loop` owns exactly one.

### REQ-2.3 — `thread_pool`

A fixed-size worker set with three strict-priority queues (`high` > `normal` >
`low`); a free worker always drains the highest non-empty queue. Used for
`getaddrinfo` in `tcp::client::connect`. A process-wide `instance()` is
provided.

### REQ-2.4 — `signal`

`connect(slot)` → `connection`; `emit(args...)` snapshots the slot list under
a lock, releases it, then invokes slots in connection order on the caller's
thread — so a slot may disconnect itself or re-emit. `emit_async(pool, ...)`
is disabled at instantiation for reference-typed args.

### REQ-2.5 — no divergence

Bug fixes and improvements to these three files should be contributed back to
cxflow and re-vendored, not forked here.
