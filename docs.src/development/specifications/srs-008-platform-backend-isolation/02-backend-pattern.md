---
title: "2. The Backend Pattern"
weight: 20
---

## 2. The Backend Pattern

Implemented across
[core/config.hpp](../../../../include/lambdatech/networking/core/config.hpp),
each `core/<facility>.hpp` façade, and the
[core/impl/](../../../../include/lambdatech/networking/core/impl/) backend
trees.

### REQ-2.1 — a facility is a façade plus N backends

Each isolated facility `F` is:

```
core/F.hpp                 the façade: vocabulary types + free functions
core/impl/posix/F.hpp      namespace core::F::impl::posix
core/impl/standalone/F.hpp namespace core::F::impl::standalone
```

A backend defines, inside `namespace core::F::impl::<backend>`:

- a `state` type — the per-instance handle the facility operates on;
- one free function per façade operation, taking `state&` (or `const state&`)
  and the façade's vocabulary types, never OS types.

The façade, in `namespace core::F`, does exactly three things:

1. `#include` the selected backend header and write
   `namespace backend = impl::<selected>;`
2. re-export the vocabulary: `using state = backend::state;` plus the enums
   and result structs the facility defines (these are backend-independent and
   live in the façade);
3. define one `inline` forwarding function per operation:
   `inline R op(state& s, Args… a) { return backend::op(s, a…); }`

No façade contains an `#ifdef` other than the backend selection block. No
backend header is included except through its façade.

### REQ-2.2 — the switch is `core/config.hpp`

`config.hpp` defines exactly one of `LNW_NET_BACKEND_POSIX` /
`LNW_NET_BACKEND_STANDALONE` to `1` and the other to `0`:

- honour a `-D` override if the caller set one;
- else `POSIX` when `__unix__ || __APPLE__ || __linux__`;
- else `STANDALONE`.

Each façade's selection block is the same four lines:

```cpp
#if LNW_NET_BACKEND_POSIX
#  include <lambdatech/networking/core/impl/posix/F.hpp>
namespace backend = impl::posix;
#elif LNW_NET_BACKEND_STANDALONE
#  include <lambdatech/networking/core/impl/standalone/F.hpp>
namespace backend = impl::standalone;
#endif
```

### REQ-2.3 — the contract is a `concept`, checked once

Each façade declares a `concept F_backend<class B>` enumerating the required
`B::state` shape and `B::op(...)` signatures, and ends with

```cpp
static_assert(F_backend<backend>, "selected <F> backend is incomplete");
```

so a half-written backend fails at the façade with a named message, not at
the first call site. No virtual dispatch is introduced — the check is
compile-time only and the forwarding calls devirtualise to the backend
functions directly.

### REQ-2.4 — one backend per facility per binary

Namespace-alias selection means a binary cannot hold two backends of the same
facility. This is accepted. `config.hpp` selects each facility independently
(`descriptor`, `poller`, `socket_ops`, `resolver` are separate switches
defaulting together), so a test binary may mix — e.g. real `socket_ops` with
a `standalone` `resolver` backed by a static host table. If a single process
ever needs two live backends of one facility, that facility (only) moves to a
runtime v-table; nothing in this SRS requires it.

### REQ-2.5 — vocabulary types replace OS types

The façades trade in:

| Concept | Façade type | never |
|---|---|---|
| a descriptor | `descriptor::native_handle` (opaque), `descriptor::state` | `int fd` |
| I/O outcome | `descriptor::io_result { std::ptrdiff_t count; std::errc error; }` | `-1` + `errno` |
| readiness interest | `poller::interest` (bitset enum: `read`, `write`) | `short` / `POLLIN` |
| readiness result | `poller::ready { bool readable, writable, error, hangup; }` | `short revents` |
| an endpoint | `core::socket_address` (already neutral: text address, port, family) | `sockaddr_storage` |
| a failure string | `socket_ops::describe(std::errc, prefix)` | `native::last_error` |

`std::errc{}` (value-initialised) means success throughout.
