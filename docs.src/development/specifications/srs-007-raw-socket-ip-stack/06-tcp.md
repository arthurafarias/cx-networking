---
title: "6. net::tcp_socket & net::tcp_listener"
weight: 60
---

## 6. net::tcp_socket & net::tcp_listener

A full userspace TCP (RFC 9293), presenting the SRS-003
[`tcp::client`](../srs-003-protocol-clients-servers/03-tcp/) /
`tcp::server` surface. The state machine, timers, and buffers live in
`tcp_state.hpp`; `net::tcp_socket` / `net::tcp_listener` are the
`shared_ptr` + `on_<name>()` wrappers the stack hands out.

### REQ-6.1 — listener

`stack->tcp_listen(port, address = "")` claims `{addr|any, port}` in the
stack's listener table and emits `listening`. Each completed handshake
produces a `net::tcp_socket` in state `open`, delivered as `connect`
(`listener::on_connect()`). The listener holds a `shared_ptr` to each
accepted socket until it emits `close` (SRS-003 REQ-3.2). A bounded accept
backlog (default 128) of half-open connections is maintained; when it is
full, new `SYN`s are dropped, or answered with SYN-cookies if
`enable_syn_cookies()` is set.

### REQ-6.2 — active open

`stack->tcp_connect(port, host)` (numeric `host`, per REQ-5.3) allocates a
local ephemeral port, sends `SYN` with the negotiated options (REQ-6.5),
and runs the connect timer. `SYN,ACK` → `ACK` transitions to `open` and
emits `connect`; an `RST`, an ICMP unreachable, or connect-timer expiry
emits `error` then `close`.

### REQ-6.3 — data transfer

- `write(core::buffer)` appends to the send buffer; the socket emits data
  as the window and congestion controller allow. It returns `false` while
  unacked+unsent data remains buffered; `drain` fires when the send buffer
  empties (SRS-003 REQ-3.4).
- Inbound in-order segment data fires `data` (`core::event<core::buffer>`)
  per delivered chunk. Out-of-order segments within the receive window are
  held and coalesced (SACK, REQ-6.5); ACKs are sent per RFC 9293 (every
  second full-sized segment, or after the delayed-ACK timer ≤ 200 ms).
- Flow control: an advertised receive window derived from the free receive
  buffer, with window-scaling (RFC 7323). Zero-window probes are sent on the
  persist timer.

### REQ-6.4 — teardown

`end()` sends `FIN` after the send buffer flushes and moves through
`FIN-WAIT` / `CLOSING` / `TIME-WAIT`; a peer `FIN` delivers `end` and, with
the default `allowHalfOpen == false`, the socket then closes its own side
(SRS-003 REQ-3.5). `destroy()` sends `RST` and closes immediately.
`TIME-WAIT` duration is configurable (default 2×MSL = 60 s) and capped by a
global `TIME-WAIT` bucket (REQ-7.4).

### REQ-6.5 — negotiated features by milestone

| Feature | RFC | Milestone |
|---|---|---|
| 3-way handshake, MSS option, sequence/ACK, cumulative ACK, retransmission on RTO | 9293 | M3 |
| RTT estimation (Jacobson/Karn), exponential RTO backoff, delayed ACK, Nagle (+`set_no_delay`) | 6298, 9293 | M3 |
| Sliding window, window scaling, PAWS timestamps | 7323 | M4 |
| Fast retransmit / fast recovery, congestion control (Reno: slow-start, CA, on loss) | 5681 | M4 |
| Selective acknowledgement | 2018 | M5 |
| Keepalive (`set_keepalive`), user timeout, `RST` on unmatched segment | 9293 | M4 |
| CUBIC congestion control; ECN | 9438, 3168 | M7 |

### REQ-6.6 — MSS and path MTU

The advertised MSS is `interface.mtu() − IP/TCP headers`. An inbound ICMP
frag-needed / packet-too-big (REQ-4.8) clamps the connection MSS and
retransmits the affected segments smaller. TCP never emits `DF`-clear IPv4
segments larger than the current path MSS.

### REQ-6.7 — remote address

`remote_address()` / `local_address()` return `core::socket_address`,
populated on connect or accept.
