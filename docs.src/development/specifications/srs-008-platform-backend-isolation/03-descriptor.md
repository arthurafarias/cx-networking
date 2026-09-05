---
title: "3. Facility: descriptor"
weight: 30
---

## 3. Facility: descriptor

Façade
[core/descriptor.hpp](../../../../include/lambdatech/networking/core/descriptor.hpp),
backends
[core/impl/posix/descriptor.hpp](../../../../include/lambdatech/networking/core/impl/posix/descriptor.hpp)
and `core/impl/standalone/descriptor.hpp`.

### REQ-3.1 — the handle

`descriptor::native_handle` is the backend's raw handle type, exposed so
`event_loop` can key its watch table and so interop code can obtain it
deliberately via `descriptor::native(state)`. On `posix` it is `int`; on
`standalone` it is a `std::uint64_t` token. It is copyable and totally
ordered.

`descriptor::state` owns one handle. It is default-constructible to an
invalid state, movable, non-copyable, and closes its handle on destruction
(RAII) unless it was released. `descriptor::state` does **not** implicitly
convert to `native_handle` — raw use is always spelled `descriptor::native(s)`.

### REQ-3.2 — operations

| Function | Contract |
|---|---|
| `state adopt(native_handle h)` | wrap an already-open handle |
| `io_result read(state&, std::span<std::byte>)` | `count` bytes read; `count == 0` is EOF; `error` set and `count < 0` on failure |
| `io_result write(state&, std::span<const std::byte>)` | `count` bytes written; `error` set and `count < 0` on failure |
| `void close(state&)` | idempotent; leaves the state invalid |
| `bool valid(const state&)` | true iff the state holds an open handle |
| `native_handle native(const state&)` | the raw handle, or the backend's invalid sentinel |
| `native_handle release(state&)` | hand ownership out; the state becomes invalid without closing |

`would_block(std::errc)` is a façade free function true for
`operation_would_block` / `resource_unavailable_try_again`, so callers test
one predicate.

### REQ-3.3 — posix backend

`state` wraps `int fd = -1`; `read` / `write` call `::read` / `::write` and
translate `-1` to `{ -1, std::errc(errno) }`; `close` calls `::close` when
`fd >= 0` then sets `fd = -1`; the destructor calls `close`. `EINTR` is
returned to the caller, not retried, matching the existing loop code that
already re-enters on `EINTR`.

### REQ-3.4 — standalone backend

`state` wraps a token into a process-global table of in-memory pipe
endpoints (shared with the `standalone` `socket_ops` and `poller` backends).
Scaffolded under M3.
