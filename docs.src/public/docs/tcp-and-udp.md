---
title: "TCP & UDP"
weight: 40
---

Both transports mirror the Node.js `net` and `dgram` surfaces, adapted to
C++: events are `on_<name>()` accessors returning a `core::event<...>`, and
objects are always held through `std::shared_ptr` (use the `create()` factory
or take one from an event).

## `protocol::tcp::server`  — `net.Server`

```c++
auto server = tcp::server::create(loop);
server->on_connect() += [](const std::shared_ptr<tcp::client> conn) { /* ... */ };
server->on_listening() += [&] { /* server->address().port is now known */ };
server->listen(0, "127.0.0.1");   // port 0 -> ephemeral
```

| Event | Args |
|---|---|
| `listening` | — |
| `connect` | `const std::shared_ptr<tcp::client> &` |
| `error` | `const std::string &` |
| `close` | — |

The server keeps each accepted `client` alive until it emits `close`.

## `protocol::tcp::client`  — `net.Socket`

```c++
auto client = tcp::client::create(loop);
client->on_connect() += [&] { client->write(core::make_buffer("GET / HTTP/1.0\r\n\r\n")); };
client->on_data() += [](const core::buffer &chunk) { /* ... */ };
client->on_end() += [] { /* peer sent FIN */ };
client->connect(80, "example.com");
```

| Method | Notes |
|---|---|
| `connect(port, host)` | resolves `host` off the loop, then connects |
| `write(bytes)` | returns `false` if the data had to be buffered — wait for `drain` |
| `end()` | half-close once the write buffer flushes |
| `destroy()` | close immediately |

Events: `connect`, `data` (`core::buffer`), `drain`, `end`, `error`
(`std::string`), `close`.

## `protocol::udp::peer`  — `dgram.Socket`

```c++
auto peer = udp::peer::create("udp4", loop);
peer->on_message() += [](const core::buffer &datagram, const core::socket_address &from) { /* ... */ };
peer->on_listening() += [&] { peer->send(core::make_buffer("ping"), 9, "127.0.0.1"); };
peer->bind(0, "127.0.0.1");
```

| Method | Notes |
|---|---|
| `bind(port, address)` | `port` 0 binds an ephemeral port |
| `send(datagram, port, host)` | one datagram; `send()` before `bind()` still receives replies |
| `close()` | — |

Events: `listening`, `message` (`core::buffer`, `core::socket_address`),
`error` (`std::string`), `close`.
