---
title: "4. Facility: poller"
weight: 40
---

## 4. Facility: poller

Façade
[core/poller.hpp](../../../../include/lambdatech/networking/core/poller.hpp),
backends
[core/impl/posix/poller.hpp](../../../../include/lambdatech/networking/core/impl/posix/poller.hpp)
and `core/impl/standalone/poller.hpp`. This facility absorbs the readiness
multiplexing **and the cross-thread wake** that `event_loop` did inline.

### REQ-4.1 — vocabulary

```cpp
enum class interest : unsigned { none = 0, read = 1, write = 2 };   // bitset
struct ready { bool readable, writable, error, hangup; };
struct event { descriptor::native_handle handle; ready what; };
```

`operator|`, and `has(interest set, interest bit)`, are provided.

### REQ-4.2 — the poller owns the wake

`poller::state` default-constructs to a usable poll set that already contains
one internal wake channel — on `posix` an `eventfd`, created and closed by
`state`. `event_loop` no longer creates, polls, drains, or closes a wake fd;
it calls `poller::interrupt`.

`poller::state` is non-copyable and non-movable (it owns OS resources and an
internal mutex); `event_loop` holds it as a direct data member.

### REQ-4.3 — operations

| Function | Contract |
|---|---|
| `void add(state&, native_handle, interest)` | register or replace a handle's interest |
| `void update(state&, native_handle, interest)` | change an existing handle's interest; no-op if absent |
| `void drop(state&, native_handle)` | deregister a handle |
| `void interrupt(state&)` | make a concurrent / next `wait` return promptly; any thread |
| `int wait(state&, std::vector<event>& out, std::optional<std::chrono::milliseconds> timeout)` | block until readiness or `timeout` (`nullopt` = forever); fill `out` with the ready handles (never the internal wake channel); return the count, or `-1` on a hard error |

`add` / `update` / `drop` are safe to call from any thread and take effect on
the next `wait`; they call `interrupt` internally so a blocked `wait` re-polls.

### REQ-4.4 — posix backend

`state` holds the `eventfd`, a `std::mutex`, and a `std::map<int, short>` of
fd → `POLLIN`/`POLLOUT` mask. `wait` snapshots the map under the lock into a
`pollfd` vector (index 0 = the eventfd, `POLLIN`), calls `::poll` with
`timeout.value_or(-1)`, drains the eventfd if it fired, and translates the
rest: `POLLIN → readable`, `POLLOUT → writable`, `POLLERR → error`,
`POLLHUP → hangup`. `::poll` returning `-1` with `EINTR` yields `0` events
(the loop re-enters), any other `-1` yields `-1`.

An `epoll` variant (M4) is a drop-in replacement for this one file.

### REQ-4.5 — event_loop after the refactor

`event_loop` keeps its watch table (`native_handle → { interest, callback }`),
its deferred queue, and its timers exactly as SRS-002 §4 specifies. `iterate`
becomes: `poller::wait(poller_, scratch, next_timeout())` → drain deferred →
fire due timers → for each returned `event`, look the callback up fresh under
the lock (SRS-002 REQ-4.6 unchanged) and invoke it with the `ready` struct.
The `io_callback` signature changes from `void(short revents)` to
`void(poller::ready)`. `event_loop.hpp` includes no `<poll.h>` /
`<sys/eventfd.h>` / `<unistd.h>`.
