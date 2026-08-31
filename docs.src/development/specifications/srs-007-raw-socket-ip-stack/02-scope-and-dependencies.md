---
title: "2. Scope, Privileges & Dependencies"
weight: 20
---

## 2. Scope, Privileges & Dependencies

### REQ-2.1 — placement: header-only core

The stack needs no third-party library: `AF_PACKET`/`SOCK_RAW`, `sockaddr_ll`,
`SIOCGIFINDEX` / `SIOCGIFHWADDR` / `SIOCGIFMTU`, and `/dev/net/tun` are all
kernel interfaces. Per the
[Plugin Folder Convention](../guidelines/plugin-folder-convention/) it is
therefore **not** a plugin. All of `netstack` is headers under
`include/lambdatech/networking/netstack/`, link dependency pthreads only,
inherited from `core`.

```
include/lambdatech/networking/netstack/
  device.hpp          # AF_PACKET / TAP layer-2 frame I/O
  ethernet.hpp arp.hpp ndp.hpp
  ipv4.hpp ipv6.hpp icmpv4.hpp icmpv6.hpp
  route_table.hpp neighbor_cache.hpp reassembly.hpp
  interface.hpp       # address/prefix/gateway/route config bound to a device
  stack.hpp           # the engine: demux, routing, transport control blocks
  udp_socket.hpp
  tcp_socket.hpp tcp_listener.hpp tcp_state.hpp
  testing/
    device_test.hpp arp_test.hpp ipv4_test.hpp reassembly_test.hpp
    udp_test.hpp tcp_handshake_test.hpp tcp_transfer_test.hpp
```

### REQ-2.2 — link layer backends

`net::device` supports two backends, selected at `create()`:

1. **`AF_PACKET`** (`socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL))`) bound
   to one interface index via `sockaddr_ll` — the production path. Reads and
   writes full Ethernet frames. `PACKET_MMAP` `TPACKET_V3` RX/TX rings are
   an allowed internal optimization (M6), not part of the API.
2. **TAP** (`open("/dev/net/tun")`, `IFF_TAP | IFF_NO_PI`) — the test and
   embedded path. Same frame-in/frame-out contract; the peer is whatever is
   attached to the other end of the tap (a second `net::device` in tests,
   or a bridge in deployment).

A raw `AF_INET`/`SOCK_RAW` (IP-level) backend is explicitly **not** offered:
that would leave IP framing and routing to the kernel, defeating the SRS.

### REQ-2.3 — privileges

Opening an `AF_PACKET` socket requires `CAP_NET_RAW`; binding it and setting
promiscuous mode requires `CAP_NET_ADMIN`. The library performs no privilege
management: it opens the socket, and if the process lacks the capability the
`net::device` emits `error` with the `errno` string. The recommended
deployments are documented, not enforced:

- grant the binary `cap_net_raw,cap_net_admin=ep` via file capabilities; or
- run inside a user + network namespace (`unshare -Urn`) with a `veth` or
  `tap` pair, where `CAP_NET_RAW` is held over the namespace's own devices
  and no host privilege is needed — this is the CI configuration.

### REQ-2.4 — in scope

Ethernet II framing; ARP (RFC 826) with a timed neighbor cache; IPv4
(RFC 791) including options pass-through, TX fragmentation and RX
reassembly with a reassembly budget; ICMPv4 (RFC 792) echo and the
error messages the stack must originate and consume (dest-unreachable,
time-exceeded, frag-needed); IPv6 (RFC 8200) with hop-by-hop / routing /
fragment extension-header parsing; ICMPv6 + NDP (RFC 4861) neighbor and
router discovery, optional SLAAC (RFC 4862); a longest-prefix-match routing
table with a default route; UDP (RFC 768); and TCP (RFC 9293) — see §6 for
the TCP feature milestones.

### REQ-2.5 — out of scope

Everything in §1.3, plus: 802.1Q VLAN tags, 802.3 LLC/SNAP framing, jumbo
frames beyond a configurable cap, IP source routing origination, IPsec, and
any second `net::device` acting as a router between subnets.

### REQ-2.6 — future plugin: AF_XDP

A zero-copy `AF_XDP` data path (`plugins/xdp/`, `libxdp` + `libbpf`) may
later replace the `AF_PACKET` backend behind the unchanged `net::device`
API. It is not specified here and its absence must not affect this SRS.
