---
title: "3. net::device — layer-2 frame I/O"
weight: 30
---

## 3. net::device — layer-2 frame I/O

`net::device` is the only object in this SRS that touches an fd. It owns the
raw socket, registers it with `core::event_loop`, and exposes an
Ethernet-frame-in / Ethernet-frame-out surface. Everything above it
(`net::stack`, the transports) is pure in-memory work on the loop thread.

### REQ-3.1 — create

- `device::create(event_loop&, const std::string& ifname)` — `AF_PACKET`
  backend bound to `ifname`.
- `device::create_tap(event_loop&, const std::string& tap_name = "")` — TAP
  backend; an empty name lets the kernel allocate one, readable back from
  `name()`.

`create()` resolves the interface index, reads the link MAC
(`SIOCGIFHWADDR`) and MTU (`SIOCGIFMTU`), sets the socket non-blocking, and
`watch()`es it for `POLLIN`. The object is `std::shared_ptr`-held; the
`event_loop` keeps a `weak_ptr` in the fd callback (SRS-003 NFR-3).

### REQ-3.2 — link parameters

`mac()` → the interface's `net::mac_address` (6 bytes). `mtu()` → the L2
payload MTU. `index()` / `name()` → the bound interface. `set_promiscuous(bool)`
toggles `PACKET_MR_PROMISC` (`AF_PACKET` only); the default is off, and the
stack still receives broadcast, multicast it has joined, and frames
addressed to `mac()`.

### REQ-3.3 — receive

On `POLLIN` the device drains the socket with `recv()` in a loop. Each frame
is delivered as `frame` (`device::on_frame()`), a
`core::event<core::buffer>` carrying one complete Ethernet frame (dest MAC,
src MAC, ethertype, payload; no FCS). Frames shorter than 14 bytes, or whose
`sockaddr_ll.sll_pkttype` is `PACKET_OUTGOING`, are dropped before the event
fires. There is no EOF concept — a device is live until destroyed.

### REQ-3.4 — send

`send(core::buffer frame)` writes one complete Ethernet frame. If the kernel
socket buffer is full (`EAGAIN`) the frame is queued and the device adds
`POLLOUT` interest; queued frames flush in order on writability, then
`POLLOUT` is dropped. `send()` returns `false` if anything was queued (the
caller may throttle on `on_drain()`), matching `tcp::client::write`
semantics (SRS-003 REQ-3.4). A frame larger than `mtu() + 14` is rejected
with `error`; IP-level fragmentation is `net::stack`'s job, not the
device's.

### REQ-3.5 — statistics

`stats()` returns counters — `rx_frames`, `rx_bytes`, `rx_dropped_short`,
`tx_frames`, `tx_bytes`, `tx_queued`, `tx_errors` — for tests and
diagnostics. Counters are plain data read on the loop thread.

### REQ-3.6 — teardown

Destroying the last `shared_ptr`, from any thread, `unwatch`es and closes the
fd and discards the TX queue. In-flight `on_frame` callbacks already hold
their own references to the delivered buffers.

### REQ-3.7 — the device is transport-agnostic

`net::device` performs no ethertype demux, no ARP, and no filtering beyond
REQ-3.3. Multiple `net::stack` instances (or a test harness) may attach to
one device; each receives every frame and decides what is relevant. A device
with no attached consumer simply drops received frames after the event.
