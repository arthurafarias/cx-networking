---
title: "Core Concepts"
weight: 20
---

The library is four layers, each in its own directory under
`include/lambdatech/networking/`.

## `core` — the runtime

**Async primitives** (`core/task.hpp`, `core/thread_pool.hpp`,
`core/signal.hpp`) are vendored from the
[cxflow](https://github.com/arthurafarias/cxflow) library and re-homed into
`namespace lambdatech::networking::core`:

- `task` — a dedicated-thread repeating-loop runner. The event loop is one of
  these driving a `poll()` cycle.
- `thread_pool` — a strict-priority worker pool for short, fire-and-forget
  work (name resolution, slow user callbacks).
- `signal<Args...>` — a connect/emit primitive; `emit()` is synchronous and
  ordered, `emit_async()` dispatches through a `thread_pool`.

**`core::event<Args...>`** (`core/event.hpp`) wraps a `signal` to give the
Node.js `EventEmitter` surface: `operator+=`, `once()`, `off_all()`, `emit()`.

**`core::event_loop`** (`core/event_loop.hpp`) is the reactor — libuv's role
behind Node. One `task` drives one `poll(2)` cycle that services, in order:

1. `watch(fd, events, cb)` / `modify` / `unwatch` — fd readiness callbacks.
2. `defer(fn)` — run `fn` on the loop thread ASAP (`process.nextTick`).
3. `set_timeout(delay, fn)` / `clear_timeout(id)` — timers (`setTimeout`).

`start()` runs the loop on its thread; `run()` blocks the caller until
`stop()`; `stop()` joins.

**`core::buffer`** is `std::vector<std::byte>` plus string interop, and
**`core::socket_address`** is `{ address, port, family }` with a blocking
`resolve()` (getaddrinfo) wrapper.

## `protocol::tcp`

`tcp::server` (`net.Server`) emits `listening`, `connection`
(`std::shared_ptr<tcp::client>`), `error`, `close`. `tcp::client`
(`net.Socket`) emits `connect`, `data`, `drain`, `end`, `error`, `close` and
has `connect()`, `write()` (returns `false` when it had to buffer — wait for
`drain`), `end()`, `destroy()`. Always held through `std::shared_ptr`.

## `protocol::udp`

`udp::peer` (`dgram.Socket`) emits `listening`, `message`
(`core::buffer`, `core::socket_address` sender), `error`, `close`, and has
`bind()`, `send(datagram, port, host)`, `close()`.

## `protocol::dns`

`dns::wire::reader` / `dns::wire::writer` are the bounds-checked, panic-free
RFC 1035 codec (see [Wire Format]({{< relref "wire-format.md" >}})).
`dns::message` = `dns::header` + `std::vector<dns::question>`, with total
`parse()` / `serialize()`. `dns::client` is a stub resolver over a
`udp::peer` (query → callback with `std::optional<message>`, matched by id,
timed out via a loop timer); `dns::server` binds a `udp::peer` and emits
`query` with a `responder` that sends the reply back.

## Testing

The self-hosted test facility lives in `testing/` subdirectories next to the
code it covers. Each header declares an `inline static` `test_group` whose
constructor self-registers it; CMake globs every `*_test.hpp` into one
generated translation unit and links a single `lambdatech-networking-tests`
binary.
