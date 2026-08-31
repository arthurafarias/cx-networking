# LambdaTech Networking

LambdaTech Networking is a header-only, zero-dependency asynchronous networking
toolkit for C++23. It layers cleanly:

- **`lambdatech::networking::core`** — the runtime: a `poll(2)` **event loop**
  (fd readiness, deferred work, timers), a Node.js `EventEmitter`-style
  `event<>` primitive, address resolution, and byte buffers. The async
  machinery — `task`, `thread_pool`, `signal` — is vendored from the
  [cxflow](https://github.com/arthurafarias/cxflow) library.
- **`lambdatech::networking::protocol::tcp`** — `tcp::server` / `tcp::client`
  (Node's `net.Server` / `net.Socket`).
- **`lambdatech::networking::protocol::udp`** — `udp::peer` (Node's
  `dgram.Socket`).
- **`lambdatech::networking::protocol::dns`** — a bounds-checked wire
  reader/writer, a `message` type, and `dns::client` (stub resolver) /
  `dns::server`, built on `udp::peer`.

Everything runs on one event-loop thread, so listeners never race; blocking
work (name resolution) is off-loaded to `core::thread_pool` and handed back to
the loop with `event_loop::defer()`.

Early conceptual stage — the core loop, all three transports, and the DNS
request/response round trip are proven end-to-end by the self-hosted test
suite. DNS resource-record parsing, name compression on write, DoT, and an
`io_uring` backend are specified but not implemented (see
[Architecture & Roadmap](#architecture--roadmap)).

# Table of Contents

- [Getting Started](#getting-started)
- [Layout](#layout)
- [Examples & Tools](#examples--tools)
- [Benchmarks](#benchmarks)
- [Architecture & Roadmap](#architecture--roadmap)
- [Testing & Coverage](#testing--coverage)
- [Documentation](#documentation)
- [Contributing](#contributing)
- [License](#license)

# Getting Started

Header-only — point your compiler at `include/` and link pthreads.

```c++
#include <future>
#include <memory>
#include <string>

#include <lambdatech/networking/core/buffer.hpp>
#include <lambdatech/networking/core/event_loop.hpp>
#include <lambdatech/networking/protocol/tcp/client.hpp>
#include <lambdatech/networking/protocol/tcp/server.hpp>

namespace core = lambdatech::networking::core;
namespace tcp = lambdatech::networking::protocol::tcp;

int main() {
  core::event_loop loop;
  auto server = tcp::server::create(loop);
  auto client = tcp::client::create(loop);

  std::promise<std::string> echoed;
  auto future = echoed.get_future();

  server->on_connect() += [](const std::shared_ptr<tcp::client> &conn) {
    conn->on_data() += [conn](const core::buffer &chunk) { conn->write(chunk); };  // echo
  };
  server->on_listening() += [&] { client->connect(server->address().port, "127.0.0.1"); };

  client->on_connect() += [&] { client->write(core::make_buffer("hello over tcp\n")); };
  client->on_data() += [&](const core::buffer &chunk) { echoed.set_value(core::to_string(chunk)); };

  loop.start();
  server->listen(0, "127.0.0.1");
  std::string line = future.get();   // "hello over tcp\n"
  client->destroy();
  server->close();
  loop.stop();
}
```

```bash
g++ -std=c++23 -Iinclude -pthread examples/lnw-example-tcp-echo.cpp -o tcp-echo
```

# Layout

```
include/lambdatech/networking/
  core/          task.hpp thread_pool.hpp signal.hpp   (vendored from cxflow)
                 event.hpp event_loop.hpp address.hpp buffer.hpp native.hpp
  protocol/
    tcp/         client.hpp  server.hpp
    udp/         peer.hpp
    dns/         header.hpp  message.hpp  client.hpp  server.hpp
                 wire/reader.hpp  wire/writer.hpp
  testing/       the self-hosted test facility
```

# Examples & Tools

| Path | What it does |
|---|---|
| `examples/lnw-example-tcp-echo.cpp` | echo server + client on one loop |
| `examples/lnw-example-dns-dig.cpp` | `./dig <name> <resolver>` — one A query over UDP |
| `tools/lnw-dns-inspect/` | `lnw-dns-inspect < msg.bin` — dump a DNS message header/question |
| `benchmarks/` | cross-stack echo benchmark vs Boost.Asio, Node.js, `asyncio` |

# Benchmarks

[`benchmarks/`](benchmarks) runs the same closed-loop TCP/UDP echo workload
against LambdaTech Networking, Boost.Asio, Node.js and Python `asyncio` —
each stack with its own server and load generator — and prints comparable
req/s, throughput, and latency-percentile tables.

```bash
cmake -S . -B build -DLAMBDATECH_NETWORKING_BUILD_BENCHMARKS=ON
cmake --build build --target lnw-bench-echo-server lnw-bench-echo-client \
                             asio-bench-echo-server asio-bench-echo-client
python benchmarks/run.py --build-dir build
```

It is a loopback, single-reactor comparison — see
[`benchmarks/README.md`](benchmarks/README.md) for the methodology and caveats.

# Architecture & Roadmap

Design specifications under
[`docs.src/content/specifications/`](docs.src/content/specifications):

- **SRS-001: DNS Message Model** — the wire codec, header, and message type.
  M1–M2 done; resource records (M3) and write-side compression (M4) not started.
- **SRS-002: Asynchronous Network Core** — the cxflow-derived primitives,
  `core::event`, and the `poll(2)` event loop. M1–M3 done; `io_uring` (M4) not
  started.
- **SRS-003: Protocol Clients & Servers** — the Node-inspired TCP/UDP/DNS
  surface. M1–M4 done.
- **SRS-004: QUIC Transport** — IETF QUIC v1 as the optional `plugins/quic/`
  component (`quic::endpoint` / `quic::connection` / `quic::stream` over
  `udp::peer`, backed by ngtcp2 + a TLS 1.3 stack). Specified, not started.
- **SRS-005: HTTP Messages, Client, Server & Router** — HTTP/1.1 over
  `tcp::*`: a key/value `http::message` container, an incremental
  parser/serializer, `http::client`, `http::server`, and `http::router`.
  Specified, not started.

# Testing & Coverage

```bash
cmake -S . -B build
cmake --build build --target lambdatech-networking-tests
build/lambdatech-networking-tests
```

The socket test groups stand real listeners up on `127.0.0.1` ephemeral ports
and drive them through the loop, each guarded by a hard timeout.

An HTML line-coverage report (via [gcovr](https://gcovr.com/), GCC/Clang only)
is published at [`docs/coverage/`](docs/coverage/index.html):

```bash
cmake -S . -B build -DLAMBDATECH_NETWORKING_ENABLE_COVERAGE=ON
cmake --build build --target coverage
```

# Documentation

The full documentation site lives under [`docs.src/`](docs.src) (Hugo source)
and builds to [`docs/`](docs):

```bash
cd docs.src && hugo server
```

# Contributing

Issues and pull requests welcome. Fixes to the vendored `core/task.hpp`,
`core/thread_pool.hpp`, `core/signal.hpp` should go back to
[cxflow](https://github.com/arthurafarias/cxflow) and be re-vendored.

# License

Proprietary — see the [license file](license.md).
