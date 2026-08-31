---
title: "5. net::udp_socket"
weight: 50
---

## 5. net::udp_socket

`net::udp_socket` is the userspace analogue of
[`udp::peer`](../srs-003-protocol-clients-servers/04-udp/) (Node's
`dgram.Socket`). It is created from a `net::stack`, holds no fd, and its
datagrams are built and parsed by this library — the kernel never sees a UDP
header on the bound interface.

### REQ-5.1 — create

`stack->udp_socket(family = "udp4")` returns a `std::shared_ptr<net::udp_socket>`.
`"udp4"` selects an IPv4 source address from the interface, `"udp6"` an IPv6
one. The socket is registered in the stack's UDP table lazily on the first
`bind()` or `send()`.

### REQ-5.2 — bind

`bind(port = 0, address = "")` claims `{address-or-first-interface-addr,
port}` in the stack's UDP table; `port` 0 allocates a free ephemeral port
(range 49152–65535, configurable). It emits `listening` with the chosen
`core::socket_address`. Binding a port already in the table fails with
`error`.

### REQ-5.3 — send

`send(core::buffer datagram, std::uint16_t port, std::string host)` —
`host` must be a numeric address in this SRS (there is no kernel resolver
path and `dns::client` would need a `net::udp_socket` itself; a
`dns::client` bound to a `net::stack` is future work). The stack picks the
route and source address, builds the IP + UDP headers, computes the UDP
checksum (mandatory for IPv6, on-by-default for IPv4), fragments if needed
(REQ-4.6), and transmits. Sending before `bind()` binds an ephemeral port
first so replies are deliverable. Returns `false` and emits `error` on a
routing / neighbor failure.

### REQ-5.4 — message

An inbound UDP datagram matched to this socket fires `message`
(`on_message()`), a `core::event<core::buffer, core::socket_address>` — the
reassembled payload and the sender. A zero-length datagram is delivered as
an empty buffer, not EOF (matching SRS-003 REQ-4.4).

### REQ-5.5 — address & options

`address()` returns the bound local `core::socket_address`. `set_ttl(int)` /
`set_hop_limit(int)` set the outbound IP TTL / hop limit. `set_broadcast(bool)`
permits a `255.255.255.255` or directed-broadcast destination. Multicast
group membership (`add_membership` / `drop_membership`) drives IGMPv2 /
MLDv1 reports from the stack and is an M5 item.

### REQ-5.6 — teardown

Destroying the last `shared_ptr` removes the socket from the stack's UDP
table on the loop thread; any queued outbound datagrams are dropped.
