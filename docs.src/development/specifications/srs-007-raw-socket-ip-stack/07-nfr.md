---
title: "7. Non-Functional Requirements"
weight: 70
---

## 7. Non-Functional Requirements

### NFR-1 — header-only, zero third-party dependencies

All of `netstack` is headers under
`include/lambdatech/networking/netstack/`. The only link dependency is
pthreads, inherited from `core`. No `libpcap`, no `libnl`, no external
protocol library. The `AF_XDP` acceleration path, if built, is the separate
optional `plugins/xdp/` (REQ-2.6).

### NFR-2 — one loop thread, no blocking

`net::device` is one more fd in the SRS-002 `poll(2)` cycle. Frame parsing,
checksum computation, ARP/NDP, reassembly, and the entire TCP state machine
run on the loop thread as pure in-memory work. The stack makes **no**
blocking syscalls and starts **no** threads. There is no `getaddrinfo` in
this SRS's path — `send`/`connect` take numeric addresses (§5.3).

### NFR-3 — safe teardown

`net::device`, `net::stack`, `net::udp_socket`, `net::tcp_socket`, and
`net::tcp_listener` are all `create()`-factory, `shared_ptr`-held, with
`weak_ptr` in every callback. Destruction from any thread at any time is
use-after-free-free: the device `unwatch`es and closes its fd, and every
transport removes itself from the stack's demux tables on the loop thread.
Destroying a `net::stack` with live sockets sends `RST` for each open TCP
connection where possible, then tears down.

### NFR-4 — bounded memory under adversarial peers

Every accumulation point has a cap with a documented default:

- IP reassembly: total in-progress bytes and per-tuple bytes are capped;
  the oldest incomplete datagram is evicted on pressure; each tuple has a
  30 s timer.
- ARP/NDP: pending-packet queue per unresolved next hop is capped (3
  packets); cache size is capped with LRU eviction of `stale` entries.
- TCP: per-connection send and receive buffers are capped (default 256 KiB
  each); the out-of-order segment queue is capped; the accept backlog and a
  global half-open-connection count are capped (SYN-flood → drop or
  SYN-cookies); a global `TIME-WAIT` bucket bounds accumulation.
- `net::device` TX queue is capped; frames are dropped with a counter, not
  buffered without limit.

A hostile peer cannot push a single `net::stack` past a few MiB regardless
of behaviour.

### NFR-5 — deterministic, kernel-independent behaviour

No `sysctl`, no `/proc/sys/net`, no kernel routing table, no netfilter is
read or consulted. Two `net::stack` instances with the same
`net::interface` config and the same input frames produce the same output
frames. This is what makes the TCP tests (NFR-8) reproducible.

### NFR-6 — SRS-003 surface parity is documented, not enforced

`net::udp_socket` and `net::tcp_socket`/`net::tcp_listener` mirror
`udp::peer` and `tcp::server`/`tcp::client` deliberately. Where they differ
— explicit `net::stack` + `net::interface` construction, numeric-only
`host`, `net::` return types — the difference is called out here and in the
docs (§1.2), not papered over with a compatibility shim.

### NFR-7 — privilege failure is a normal error

Missing `CAP_NET_RAW` / `CAP_NET_ADMIN`, a non-existent interface, or a
`/dev/net/tun` that cannot be opened surface as a `net::device` `error`
event with the `errno` string — never a crash, an assert, or a silent hang.
The recommended non-root CI setup is `unshare -Urn` with a `tap` pair
(REQ-2.3).

### NFR-8 — bounded tests over a TAP pair

Every `netstack` test group builds two `net::device::create_tap` ends (or
one tap plus a hand-rolled frame injector), drives the local
`core::event_loop`, and wraps every wait in `testing::await(future, timeout)`
so a regression fails a case rather than hanging CI
(testing-convention guideline, SRS-003 NFR-4). The TCP groups explicitly
exercise loss, reordering, and duplication by dropping/reordering frames
between the two ends.

### NFR-9 — RFC conformance is scoped and documented

Each protocol header records which RFC sections it implements and which it
intentionally omits (§2.4 / §2.5). The stack targets interoperability with
the Linux kernel stack as the reference peer for every milestone; a
divergence from an RFC that is required for that interop is commented at the
divergence.
