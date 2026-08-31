---
title: "Getting Started"
weight: 10
---

LambdaTech Networking is header-only, so there's nothing to build or link
against beyond pthreads — just point your compiler at `include/`.

## An echo server and client

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
    conn->on_data() += [conn](const core::buffer &chunk) { conn->write(chunk); };
  };
  server->on_listening() += [&] { client->connect(server->address().port, "127.0.0.1"); };

  client->on_connect() += [&] { client->write(core::make_buffer("hello over tcp\n")); };
  client->on_data() += [&](const core::buffer &chunk) { echoed.set_value(core::to_string(chunk)); };

  loop.start();
  server->listen(0, "127.0.0.1"); // port 0 -> an ephemeral port

  std::string line = future.get();
  client->destroy();
  server->close();
  loop.stop();
}
```

Compile it:

```bash
g++ -std=c++23 -Iinclude -pthread examples/lnw-example-tcp-echo.cpp -o tcp-echo
```

## The event model

Every socket type exposes its events as `on_<name>()` accessors that return a
`core::event<Args...>` — the analogue of a Node `EventEmitter` channel.
`operator+=` adds a listener; `.once()` returns a `subscription` you can
disconnect:

```c++
client.on_data() += [](const core::buffer &chunk) { /* ... */ };
auto sub = client.on_close().once([] { /* fires at most once */ });
sub.disconnect();                                   // like emitter.off(...)
```

Listeners run **synchronously on the one event-loop thread**, in subscription
order. Anything that would block the loop (DNS lookups, disk) belongs on
`core::thread_pool`; hand the result back with `loop.defer(...)`.

Continue to [Core Concepts]({{< relref "core-concepts.md" >}}).
