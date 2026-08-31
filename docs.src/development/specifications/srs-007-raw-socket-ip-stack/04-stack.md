---
title: "4. net::interface & net::stack"
weight: 40
---

## 4. net::interface & net::stack — ARP, IP, ICMP, routing

### 4.1 net::interface — the L3 configuration

`net::interface` is plain configuration attached to a `net::device`: it does
not touch an fd. It carries what the kernel would otherwise hold in its
per-interface state.

#### REQ-4.1 — addresses and routes

- `add_address(net::ip_address, unsigned prefix_len)` — one or more IPv4
  and/or IPv6 addresses. The stack answers ARP/NDP for each and accepts
  inbound packets destined to any of them (plus the matching broadcast and
  solicited-node multicast).
- `set_gateway(net::ip_address)` — installs a default route
  (`0.0.0.0/0` or `::/0`) via that next hop.
- `add_route(net::ip_prefix dest, std::optional<net::ip_address> via)` —
  a static route; `via` absent means on-link.
- `set_mtu(unsigned)` — defaults to the device MTU; the IP layer fragments
  (v4) or emits `Packet Too Big` (v6) against this value.

#### REQ-4.2 — SLAAC (optional)

If an interface has an IPv6 configuration but no static IPv6 address,
`enable_slaac()` makes the stack send Router Solicitations and configure an
address + default route from the received Router Advertisement (RFC 4862),
honouring the advertised prefix and lifetimes.

### 4.2 net::stack — the engine

`net::stack::create(std::shared_ptr<net::device>, net::interface)` builds the
engine. It attaches to the device's `on_frame`, and it is the factory for
every transport (§5, §6). One `net::stack` = one host identity on the wire.

#### REQ-4.3 — inbound demux

Each received frame is dispatched by ethertype: `0x0806` → ARP, `0x0800` →
IPv4, `0x86DD` → IPv6. Frames whose destination MAC is neither `device.mac()`,
broadcast, nor a joined multicast group are dropped. IP packets whose
destination address is not one of the interface's addresses (and not a
broadcast/multicast the stack listens to) are dropped — the stack does
**not** forward.

#### REQ-4.4 — ARP (IPv4)

The stack maintains a `neighbor_cache` keyed by IPv4 address:
`{ mac, state, expiry }` with states `incomplete` / `reachable` / `stale`.
It answers ARP requests for its own addresses, learns from replies and from
gratuitous ARP, and on a cache miss for an outbound packet it queues the
packet (bounded, REQ-7.4), broadcasts an ARP request, and retransmits up to
3 times before failing the packet with a local `EHOSTUNREACH` to the
originating transport. Reachable entries expire to `stale` after 60 s and
are re-verified on next use.

#### REQ-4.5 — NDP (IPv6)

The IPv6 equivalent: Neighbor Solicitation / Advertisement over ICMPv6
(RFC 4861) with Duplicate Address Detection for configured addresses, the
same cache-state machine as REQ-4.4, and Router Discovery feeding REQ-4.2.

#### REQ-4.6 — IPv4

Parse and validate the header (version, IHL, total length, header
checksum); drop malformed or non-local packets. Reassemble fragments by
`{src, dst, id, proto}` with a per-tuple timer (REQ-7.4). On transmit,
select the route (longest-prefix match, then default), resolve the next-hop
MAC via REQ-4.4, set a monotonically increasing IP id, compute the header
checksum, and fragment to the outgoing MTU when `DF` is clear — or, when
`DF` is set and the packet exceeds the MTU, fail it and (for forwarded-style
paths) originate an ICMP frag-needed to the local transport so TCP can lower
its MSS.

#### REQ-4.7 — IPv6

Parse the fixed header and the extension-header chain (hop-by-hop, routing,
fragment, destination-options); reassemble via the Fragment header. IPv6
routers/hosts do not fragment in transit, so an oversized transmit yields a
local `Packet Too Big` to the transport. Compute the transport pseudo-header
checksum (mandatory for v6).

#### REQ-4.8 — ICMP

- **ICMPv4/ICMPv6 echo:** the stack answers `echo request` to its own
  addresses with an `echo reply` automatically.
- **Errors consumed:** `destination unreachable` (incl. frag-needed /
  packet-too-big), `time exceeded`, `parameter problem` are matched to the
  originating socket by the quoted packet's tuple and surfaced as that
  socket's `error` (and, for frag-needed, an MSS clamp).
- **Errors originated:** `port unreachable` when a UDP datagram or TCP SYN
  hits no listener (TCP prefers `RST`; see §6), `time exceeded` is not
  originated because the stack never forwards.
- `net::stack::ping(dest, opts)` is a convenience that sends echo requests
  and reports round-trip times — the building block for an example
  `lnw-example-net-ping`.

#### REQ-4.9 — transport control blocks

`net::stack` owns the demultiplex tables: a UDP table keyed by local
`{addr, port}` and a TCP table keyed by the 4-tuple
`{local addr, local port, remote addr, remote port}` plus a listener table
keyed by `{local addr|any, local port}`. Inbound IP payloads with protocol
17 / 6 are matched here and handed to the owning `net::udp_socket` /
`net::tcp_socket` / `net::tcp_listener`; an unmatched datagram triggers
REQ-4.8, an unmatched TCP segment triggers a `RST` (RFC 9293 §3.10).

#### REQ-4.10 — timers on the loop

Every stack timer — ARP/NDP retransmit and expiry, reassembly timeout, TCP
RTO / TIME-WAIT / keepalive / delayed-ACK — is a `core::event_loop`
`set_timeout` on the one loop thread. The stack starts no threads of its
own.
