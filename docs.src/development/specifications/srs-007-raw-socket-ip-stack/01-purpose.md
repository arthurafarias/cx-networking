---
title: "1. Purpose"
weight: 10
---

## 1. Purpose

SRS-003 builds `tcp::client`, `udp::peer`, and the rest directly on the
kernel's `AF_INET` / `AF_INET6` sockets: the kernel owns ARP, routing,
fragmentation, the TCP state machine, retransmission timers, and congestion
control, and the library only moves payload bytes across the `read()` /
`write()` boundary. This SRS specifies the opposite arrangement — **the
library owns the whole stack** — and connects to the network through a
single raw layer-2 socket.

### 1.1 Why bypass the kernel stack

- **Determinism and control.** The retransmission timeout, the congestion
  controller, delayed-ACK behaviour, the initial window, TIME-WAIT
  duration, and the reassembly policy are all code in this repository,
  tunable per `net::stack` instance, not `sysctl` global state shared with
  every other process on the host.
- **Isolation.** Traffic on the bound interface never touches netfilter,
  conntrack, the kernel routing table, or the host's listening sockets. A
  `net::stack` can run its own addresses and ports that the kernel has no
  knowledge of, on an interface the kernel is not otherwise using.
- **Testability.** The entire TCP/IP path — handshake, loss, reordering,
  window scaling, RTO backoff — runs in one address space over a TAP pair
  and is exercised by ordinary `testing/*_test.hpp` groups with no
  privileged host configuration and no flakiness from the host's own
  traffic.
- **Reuse of the async core.** The stack is one more `fd` in the same
  `poll(2)` cycle (SRS-002 §4): the `net::device` socket is `watch()`ed for
  `POLLIN`/`POLLOUT`, every protocol timer is a `core::event_loop`
  `set_timeout`, and every user callback runs on the one loop thread —
  identical to how `tcp::*` and `udp::peer` already behave.

### 1.2 Node.js parity

The user-facing transports keep the SRS-003 surface so that code moving from
the kernel path to the userspace path changes only construction:

| SRS-003 (kernel) | SRS-007 (userspace) |
|---|---|
| `udp::peer::create("udp4")` | `stack->udp_socket()` |
| `tcp::client::create()` + `connect(port,host)` | `stack->tcp_connect(port, host)` |
| `tcp::server::create()` + `listen(port)` | `stack->tcp_listen(port)` |
| `sock.on_data() += fn` | `sock.on_data() += fn` (unchanged) |
| `sock.on_message() += fn` | `sock.on_message() += fn` (unchanged) |
| implicit host IP / routing | explicit `net::interface` (address, prefix, gateway) |

The one non-negotiable difference: there is no ambient host configuration.
A caller builds a `net::device` for an interface, attaches a `net::interface`
with at least one address and a default route, and gets the transports from
the resulting `net::stack`.

### 1.3 Out of scope

IP forwarding between two interfaces, a netfilter-equivalent hook chain,
tunnelling encapsulations (GRE, VXLAN, WireGuard), a full BSD-sockets
`AF_INET` shim / `LD_PRELOAD` interposer, TLS (still `plugins/tls/`),
multicast routing and IGMP/MLD snooping, traffic shaping / QoS, and hardware
checksum or segmentation offload. IPv4 fragmentation/reassembly and honouring
ICMP "fragmentation needed" are in scope; full RFC 8899 packetization-layer
PMTUD is not.
