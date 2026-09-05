---
title: "Specifications"
weight: 2
---

Software Requirements Specifications (SRS) for LambdaTech Networking. Each
document is split into numbered section pages — use each document's index
page for a full table of contents.

## Guidelines

Pinned, cross-cutting rules that apply regardless of any individual SRS's
Active/Archived status:

- [Testing Convention](guidelines/testing-convention/) — every module ships
  its unit tests as a colocated `testing/*_test.hpp` header that
  self-registers a `test_group`; no external test framework is added.
- [Plugin Folder Convention](guidelines/plugin-folder-convention/) — anything
  that needs a third-party library lives under `plugins/<name>/` as its own
  optional, dependency-detected CMake target, never in the header-only core.

## Active

- [SRS-001: DNS Message Model](srs-001-dns-message-model/) — the
  `protocol::dns` wire reader/writer, the 12-byte header, and the `message`
  type with its question section.
- [SRS-002: Asynchronous Network Core](srs-002-async-network-core/) — the
  `core` runtime: the cxflow-derived async primitives, `core::event`, and the
  `poll(2)` `event_loop` with fd watches, deferred work, and timers.
- [SRS-003: Protocol Clients & Servers](srs-003-protocol-clients-servers/) —
  the Node.js-inspired surface: `tcp::server`/`tcp::client`, `udp::peer`, and
  `dns::client`/`dns::server`.
- [SRS-004: QUIC Transport](srs-004-quic-transport/) — the IETF QUIC v1
  transport as the optional `plugins/quic/` component: a `quic::endpoint`
  multiplexing `quic::connection`s and `quic::stream`s over one `udp::peer`.
  Specified, not started.
- [SRS-005: HTTP Messages, Client, Server & Router](srs-005-http-protocol/) —
  HTTP/1.1 on top of `tcp::*`: the key/value `http::message` container, an
  incremental parser/serializer, `http::client`, `http::server`, and
  `http::router`. Specified, not started.
- [SRS-006: Server-Side Rendering — Mustache Templates](srs-006-mustache-renderer/) —
  a self-hosted, header-only Mustache engine matched to
  [samskivert/jmustache](https://github.com/samskivert/jmustache) (variant
  data context, compound paths, `-index`/`-first`/`-last`, lambdas,
  filesystem partials) plus `render::view`, a one-call server-side renderer
  over `http::server`. Specified, not started.
- [SRS-007: Userspace IP Stack over Raw Sockets](srs-007-raw-socket-ip-stack/) —
  a kernel-bypassing TCP/IP stack in `lambdatech::networking::netstack`: an
  `AF_PACKET`/TAP `net::device`, `net::stack` (ARP/NDP, IPv4/IPv6, ICMP,
  routing, reassembly), and `net::udp_socket` / `net::tcp_socket` /
  `net::tcp_listener` mirroring the SRS-003 surface. Specified, not started.
- [SRS-008: Platform Backend Isolation](srs-008-platform-backend-isolation/) —
  every OS dependency of the core (descriptor I/O, readiness polling, the BSD
  socket calls, name resolution) pushed behind compile-time-selected backend
  façades in `core::` (`descriptor`, `poller`, `socket_ops`, `resolver`), so
  `event_loop` and the protocol layer name no `<poll.h>` / `<sys/socket.h>`.
  posix backend done; `standalone` in-process fabric in progress.

## Archived (implemented)

_None yet._
