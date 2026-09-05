---
title: "5. Facilities: socket_ops & resolver"
weight: 50
---

## 5. Facilities: socket_ops & resolver

### 5.1 socket_ops

Façade
[core/socket_ops.hpp](../../../../include/lambdatech/networking/core/socket_ops.hpp),
posix backend
[core/impl/posix/socket_ops.hpp](../../../../include/lambdatech/networking/core/impl/posix/socket_ops.hpp).
Replaces `core/native.hpp` and every inline BSD-socket call in `tcp/socket.hpp`,
`tcp/server.hpp`, `udp/peer.hpp`.

#### REQ-5.1 — vocabulary

```cpp
enum class domain    { inet, inet6 };
enum class transport { stream, datagram };
enum class shut      { read, write, both };

struct opened   { descriptor::state handle; std::errc error; };   // error == {} ⇒ ok
struct accepted { descriptor::state handle; socket_address peer; std::errc error; };
struct transfer { std::ptrdiff_t count; std::errc error; };       // count == 0 ⇒ stream EOF
struct received { std::ptrdiff_t count; socket_address from; std::errc error; };
```

#### REQ-5.2 — operations

| Function | Notes |
|---|---|
| `opened open(domain, transport)` | non-blocking-by-default is **not** assumed — caller follows with `set_nonblocking` |
| `std::errc set_nonblocking(descriptor::state&)` | |
| `std::errc set_reuse_addr(descriptor::state&)` | `SO_REUSEADDR` |
| `std::errc connect(descriptor::state&, const socket_address&)` | returns `std::errc::operation_in_progress` for the normal async case |
| `std::errc bind(descriptor::state&, const socket_address&)` | |
| `std::errc listen(descriptor::state&, int backlog)` | |
| `accepted accept(descriptor::state&)` | `error == operation_would_block` when the queue drains |
| `transfer send(descriptor::state&, std::span<const std::byte>)` | `MSG_NOSIGNAL` applied by the backend |
| `transfer recv(descriptor::state&, std::span<std::byte>)` | |
| `transfer send_to(descriptor::state&, std::span<const std::byte>, const socket_address&)` | |
| `received recv_from(descriptor::state&, std::span<std::byte>)` | |
| `std::errc shutdown(descriptor::state&, shut)` | |
| `socket_address local_endpoint(descriptor::state&)` | `getsockname` |
| `std::errc pending_error(descriptor::state&)` | `SO_ERROR`, for connect completion |
| `std::string describe(std::errc, const char* prefix)` | `"prefix: <message>"` |

#### REQ-5.3 — the sockaddr boundary

`socket_address ⇄ sockaddr_storage` conversion lives **only** in
`core/impl/posix/endpoint.hpp` (included only by the posix backends).
`core/address.hpp` keeps just the neutral `socket_address` value (text
address, port, `family` as `AF_INET`/`AF_INET6`) and its `operator==`. The
`family` field staying an `int` with `AF_*` values is a pragmatic exception —
it is an opaque tag to every consumer and never dereferenced.

Protocol code obtains peer/local addresses from `socket_ops` return values
(`accepted::peer`, `received::from`, `local_endpoint`), never by calling a
conversion itself.

### 5.2 resolver

Façade
[core/resolver.hpp](../../../../include/lambdatech/networking/core/resolver.hpp),
posix backend `core/impl/posix/resolver.hpp` (thin wrapper over
`getaddrinfo`, moved verbatim from `address.hpp`).

#### REQ-5.4 — operations

```cpp
std::vector<socket_address> resolve(const std::string& host, std::uint16_t port,
                                    bool datagram = false);              // blocking
std::optional<socket_address> resolve_one(const std::string& host, std::uint16_t port,
                                          bool datagram = false);
```

Blocking semantics are unchanged from SRS-002 — callers still run `resolve`
on `core::thread_pool` and marshal the result back with `event_loop::defer`.
The `standalone` backend resolves numeric literals directly and looks other
names up in a process-global host table the tests populate.
