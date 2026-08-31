---
title: "8. Milestones"
weight: 80
---

## 8. Milestones

| Milestone | Covers | Status |
|---|---|---|
| **M1** | `net::device`: `AF_PACKET` + TAP backends, `on_frame` / `send` / `on_drain`, link-param ioctls, TX queue + `POLLOUT`, `stats()`, safe teardown. `net::mac_address` / `net::ip_address` / `net::ip_prefix` value types. Test group `net::device` (tap↔tap frame echo). | Not started |
| **M2** | Ethernet demux + ARP: `neighbor_cache`, request/reply/gratuitous, pending-packet queue, retransmit + fail. IPv4 RX/TX parse + header checksum, route table (longest-prefix + default), no fragmentation yet. `net::interface` config. Test groups `net::arp`, `net::ipv4`. | Not started |
| **M3** | IPv4 TX fragmentation + RX reassembly with budget/timers; ICMPv4 echo + error consume/originate; `net::stack::ping`. `net::udp_socket` (§5): bind, send, message, ephemeral ports, checksum. Test groups `net::reassembly`, `net::udp`. `examples/lnw-example-net-ping.cpp`. | Not started |
| **M4** | `net::tcp_socket` / `net::tcp_listener` (§6): handshake, MSS, cumulative ACK, RTO with Jacobson/Karn, delayed ACK, Nagle + `set_no_delay`, FIN/TIME-WAIT, `RST` handling, connect timeout. Test groups `net::tcp_handshake`, `net::tcp_transfer`. `examples/lnw-example-net-tcp-echo.cpp` (userspace echo server on a tap). | Not started |
| **M5** | TCP window scaling + PAWS timestamps (RFC 7323), fast retransmit / fast recovery, Reno congestion control (RFC 5681), SACK (RFC 2018), keepalive + user timeout. Loss/reorder test group `net::tcp_congestion`. | Not started |
| **M6** | IPv6 + ICMPv6 + NDP (RFC 8200 / 4861): extension-header chain, fragment header, neighbor discovery + DAD, `"udp6"` / IPv6 TCP. Optional SLAAC (RFC 4862). `PACKET_MMAP` `TPACKET_V3` RX/TX rings behind the unchanged `net::device` API. Test groups `net::ipv6`, `net::ndp`. | Not started |
| **M7** | CUBIC congestion control (RFC 9438) + ECN (RFC 3168); IGMPv2 / MLDv1 for UDP multicast; SYN-cookies. `plugins/xdp/` exploration — zero-copy `AF_XDP` data path behind `net::device`. A `dns::client` / `http::client` bound to a `net::stack` instead of the kernel (cross-SRS integration example). | Not started |

### File map when M1–M4 land

```
include/lambdatech/networking/netstack/
  device.hpp
  ethernet.hpp   arp.hpp   neighbor_cache.hpp
  ipv4.hpp   icmpv4.hpp   reassembly.hpp   route_table.hpp
  interface.hpp   stack.hpp
  udp_socket.hpp
  tcp_socket.hpp   tcp_listener.hpp   tcp_state.hpp
  testing/
    device_test.hpp   arp_test.hpp   ipv4_test.hpp   reassembly_test.hpp
    udp_test.hpp   tcp_handshake_test.hpp   tcp_transfer_test.hpp
examples/
  lnw-example-net-ping.cpp
  lnw-example-net-tcp-echo.cpp
```
