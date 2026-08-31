---
title: "5. quic::stream"
weight: 50
---

## 5. quic::stream

A `quic::stream` is one QUIC stream: an ordered, reliable, flow-controlled
byte sequence in one or both directions. Its surface is deliberately close to
`tcp::client`'s read/write half (SRS-003 REQ-3.4, REQ-3.5) so stream code
ports with minimal change.

### REQ-5.1 — direction and lifetime

A stream is `bidi` or `uni`. For `uni`, exactly one side has the write half:
the opener writes, the peer reads. The connection holds a
`std::shared_ptr` to each stream until both halves are finished (FIN sent
and received, or a reset in either direction), then drops it.

### REQ-5.2 — write and backpressure

`write(bytes)` appends to the stream's send buffer and asks the connection to
flush. It returns `false` when bytes remain buffered because either the
stream's `MAX_STREAM_DATA` or the connection's `MAX_DATA` is exhausted; the
caller waits for `drain`, which fires when the send buffer empties. `write`
after `end()` / a reset is a no-op that emits `error`.

### REQ-5.3 — read

Inbound `STREAM` frames are reassembled in order and emitted as `data`
(`core::buffer`) per contiguous chunk. The stream advertises `MAX_STREAM_DATA`
credit as the consumer drains; there is no explicit `pause()` in M1 (the
credit window is fixed-size and auto-tuned by the backend).

### REQ-5.4 — half-close

A received FIN emits `end` (no more `data` will follow). `end()` sends a FIN
after the send buffer flushes; the read half stays open until the peer's FIN.
When both directions have finished, `close` fires.

### REQ-5.5 — reset

`reset(app_error)` sends `RESET_STREAM` and abandons the send buffer.
`stop_sending(app_error)` asks the peer to stop writing. A `RESET_STREAM`
from the peer emits `error` ("reset by peer", with the code) and then
`close`; any not-yet-delivered `data` is dropped.

### REQ-5.6 — identity

`id()` returns the 62-bit stream ID once assigned (`std::nullopt` before the
first flush). `direction()`, `is_local()` (which side opened it), and
`connection()` (a `std::weak_ptr`) are available.

### REQ-5.7 — events

`data` (`core::buffer`), `drain`, `end`, `error` (`std::string`), `close`.
`close` fires exactly once, after `end` (clean) or `error` (reset), and is
the stream's last event.
