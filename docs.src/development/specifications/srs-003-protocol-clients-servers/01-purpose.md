---
title: "1. Purpose"
weight: 10
---

## 1. Purpose

Given the `core` runtime (SRS-002), this SRS defines the surface application
code actually uses. The design goal is that someone who knows Node.js's
`net`, `dgram`, and `dns` modules can read and write LambdaTech Networking
code with almost no translation:

| Node.js | LambdaTech Networking |
|---|---|
| `net.Server` | `protocol::tcp::server` |
| `net.Socket` | `protocol::tcp::client` |
| `dgram.Socket` | `protocol::udp::peer` |
| `dns` (resolver) | `protocol::dns::client` |
| — (custom UDP server) | `protocol::dns::server` |
| `emitter.on('data', fn)` | `sock.on_data() += fn` |
| `emitter.once('connect', fn)` | `sock.on_connect().once(fn)` |

The differences from Node are the ones C++ forces: objects are held through
`std::shared_ptr` (the event loop keeps a `weak_ptr`), events are typed
`core::event<Args...>` accessors rather than stringly-typed, and payloads are
`core::buffer` (`std::vector<std::byte>`).
