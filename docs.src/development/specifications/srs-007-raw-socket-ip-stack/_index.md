---
title: "SRS-007: Userspace IP Stack over Raw Sockets"
weight: 7
---

**Status:** Specified, not started — no `netstack` code exists yet. This
document defines a **userspace TCP/IP stack** that owns Ethernet framing,
ARP/NDP, IPv4/IPv6, ICMP, UDP, and TCP itself, driven entirely from the
`core` event loop and speaking to the wire through an `AF_PACKET`
(`SOCK_RAW`, layer-2) socket or a TAP file descriptor. The kernel IP stack
is bypassed end to end: the kernel sees only opaque Ethernet frames on the
bound interface, and every protocol decision — ARP resolution, IP routing,
fragmentation, the TCP state machine, retransmission, congestion control —
is made in this library.

**Author:** Arthur de Araújo Farias
**Date:** 2026-08-31

**Depends on:** [SRS-002](../srs-002-async-network-core/) (the `core`
runtime — `event_loop` fd watches and timers, `core::buffer`,
`core::thread_pool`, `core::event`). It reuses the **event-surface
conventions** of [SRS-003](../srs-003-protocol-clients-servers/)
(`std::shared_ptr` + `create()` + `on_<name>()`) but not its code: the
`netstack` transports are a parallel, kernel-independent implementation of
the same shapes as `tcp::*` and `udp::peer`.

No third-party library is required — `AF_PACKET` and TAP are kernel
interfaces — so per the
[Plugin Folder Convention](../guidelines/plugin-folder-convention/) the
stack is **header-only core** at
`include/lambdatech/networking/netstack/`, namespace
`lambdatech::networking::netstack`, conventionally aliased
`namespace net = lambdatech::networking::netstack;`. The zero-copy `AF_XDP`
data path (which needs `libxdp`/`libbpf`) is deferred to a future
`plugins/xdp/`.

## Sections

| # | Section |
|---|---|
| 1 | [Purpose](01-purpose) |
| 2 | [Scope, Privileges & Dependencies](02-scope-and-dependencies) |
| 3 | [net::device — layer-2 frame I/O](03-device) |
| 4 | [net::interface & net::stack — ARP, IP, ICMP, routing](04-stack) |
| 5 | [net::udp_socket](05-udp) |
| 6 | [net::tcp_socket & net::tcp_listener](06-tcp) |
| 7 | [Non-Functional Requirements](07-nfr) |
| 8 | [Milestones](08-milestones) |
