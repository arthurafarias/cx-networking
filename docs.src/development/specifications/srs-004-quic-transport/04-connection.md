---
title: "4. quic::connection"
weight: 40
---

## 4. quic::connection

A `quic::connection` is one QUIC connection: a handshake, a set of streams,
connection-level flow control, and a close. It is always held through
`std::shared_ptr`; the endpoint keeps a `std::shared_ptr` until the
connection reaches state `closed`, then drops it (SRS-003 REQ-3.2 pattern).

### REQ-4.1 — states

`handshaking` → `open` → `closing` → `closed`. `draining` is entered instead
of `closing` when the peer initiated the close. Every state transition is
observable through the events below; no event fires after `close`.

### REQ-4.2 — client open

`quic::client::connect(port, host, opts)` resolves `host` (SRS-002
`resolve()` on `core::thread_pool`), `defer()`s creation of the connection
and its Initial packet onto the loop, and drives the handshake. On handshake
completion the connection emits `connect`; the negotiated ALPN, 0-RTT
acceptance, and peer transport parameters are then readable.

### REQ-4.3 — opening streams

`open_stream(dir = bidi)` returns a `std::shared_ptr<quic::stream>`
immediately (stream IDs are assigned lazily by the backend on first flush).
`open_stream` fails with an `error` on the connection — not an exception —
if the peer's `initial_max_streams` for that direction is exhausted; the
caller retries after `stream_capacity` fires.

### REQ-4.4 — incoming streams

A stream the peer opens is emitted as `stream`
(`std::shared_ptr<quic::stream>`). If no listener is attached to `stream`
when it fires, the connection resets that stream with
`STREAM_STATE_ERROR`-equivalent so a peer cannot pin memory by opening
streams nobody reads.

### REQ-4.5 — connection flow control

The connection tracks `MAX_DATA` in both directions. `data_capacity()`
reports the bytes the peer will currently accept across all streams;
`stream::write` past that returns backpressure (§5). Incoming `MAX_DATA`
increases emit `data_capacity` so writers can resume.

### REQ-4.6 — datagrams (RFC 9221)

If both peers advertised `max_datagram_frame_size`, `send_datagram(bytes)`
queues one unreliable datagram (returns `false` if it will not fit the
current max or the queue is full) and inbound datagrams emit `datagram`
(`core::buffer`). Datagrams are not retransmitted and may be dropped or
reordered.

### REQ-4.7 — keep-alive and idle

`set_keep_alive(interval)` arms periodic `PING`s. The idle timeout is the
minimum of the two peers' `max_idle_timeout`; on expiry the connection goes
straight to `closed` with an `idle` reason on the `close` event.

### REQ-4.8 — close

`close(app_error = 0, reason = "")` sends `CONNECTION_CLOSE`, moves to
`closing`, and emits `close` after the drain period (or immediately if no
packets are in flight). A `CONNECTION_CLOSE` from the peer emits `error`
(unless `app_error == 0`) then `close`. A stateless reset emits `error`
("stateless reset") then `close`.

### REQ-4.9 — events

`connect`, `stream` (`std::shared_ptr<quic::stream>`), `datagram`
(`core::buffer`), `data_capacity`, `stream_capacity` (stream direction),
`error` (`std::string`), `close` (reason: `idle` | `app` | `transport` |
`reset` | `local`). `close` fires exactly once and is always the last event.

### REQ-4.10 — peer address & TLS info

`remote_address()` returns the current `core::socket_address`.
`alpn()`, `session_reused()` (0-RTT / resumption), `handshake_confirmed()`,
and the peer certificate chain are available for logging and policy.
