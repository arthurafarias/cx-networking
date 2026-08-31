---
title: "3. The Parser & Serializer"
weight: 30
---

## 3. The Parser & Serializer

Implemented in
[protocol/http/parser.hpp](../../../../include/lambdatech/networking/protocol/http/parser.hpp).
The serializer is `message::serialize()` (§2.6); this section is the
incremental **reader** that turns a TCP byte stream into `http::message`
objects.

### REQ-3.1 — incremental, mode-bound

`parser` is constructed for one direction:

```c++
enum class mode { request, response };
explicit parser(mode m, limits lim = {});
```

`request` mode reads request lines; `response` mode reads status lines and
applies response-only body rules (REQ-3.5). A `parser` is fed by exactly one
socket and is not thread-safe by itself (it runs on the loop thread, like
everything in SRS-003).

### REQ-3.2 — feeding bytes

```c++
core::event<const message &> &on_message();   // one full message decoded
core::event<const std::string &> &on_error(); // fatal protocol error

bool feed(std::span<const std::byte> chunk);   // false once errored
void finish();                                 // peer EOF (see REQ-3.5)
void reset();                                  // back to start-line state
```

`feed` appends to an internal accumulator and advances a state machine:
**start line → header block → body → (emit) → start line**. Multiple
messages in one `feed` (HTTP pipelining) each emit in turn; bytes past the
last complete message stay buffered. After `on_error` the parser latches:
every further `feed` returns `false` and emits nothing.

### REQ-3.3 — start line and headers

- Lines end in CRLF; a bare LF is accepted, a bare CR is an error.
- The start line is split into exactly three tokens (request:
  method / target / version; status: version / code / reason, reason may be
  empty). A malformed version, a non-3-digit status, or a request target
  with control characters is an error.
- Header lines are `name ":" OWS value OWS`. Leading whitespace on the first
  header line (obs-fold continuation) is rejected (RFC 9110 §5.2). Duplicate
  names fold per §2.4. A header block not terminated by the blank line
  before `limits.max_header_bytes` is an error.
- `name` is validated as a token; the value is stored as opaque bytes.

### REQ-3.4 — request body framing

In `request` mode, in this order:

1. `Transfer-Encoding` ending in `chunked` → chunked decoding (REQ-3.6).
   `Transfer-Encoding` present but not ending in `chunked` → `501`-class
   error (`on_error`, message dropped).
2. `Content-Length: n` (single, well-formed; conflicting duplicates are an
   error) → exactly `n` body bytes.
3. neither → empty body. A request never has a "until EOF" body.

### REQ-3.5 — response body framing

In `response` mode:

1. status `1xx`, `204`, `304`, or a response to a `HEAD` request (the
   caller sets `expect_no_body()` before feeding) → zero-length body,
   emitted as soon as the header block closes.
2. `Transfer-Encoding: chunked` → chunked decoding.
3. `Content-Length: n` → `n` bytes.
4. none of the above → the body runs to connection close: bytes accumulate
   until `finish()`, which emits the final message. `finish()` with a
   partial chunked body or fewer than `Content-Length` bytes is an error.

### REQ-3.6 — chunked transfer coding

Each chunk is `hex-size [";" ext] CRLF <size bytes> CRLF`; the terminating
`0` chunk is followed by an optional trailer field block and a final CRLF.
Trailer fields are merged into the message like ordinary fields. Chunk
extensions are parsed and discarded. A chunk size above
`limits.max_body_bytes` (running total) is an error. On completion the
parser removes `Transfer-Encoding` and sets `Content-Length` to the decoded
length, so downstream code never has to special-case framing.

### REQ-3.7 — limits

```c++
struct limits {
  std::size_t max_start_line   = 8   * 1024;
  std::size_t max_header_bytes = 64  * 1024;   // whole block, names + values
  std::size_t max_body_bytes   = 1   * 1024 * 1024;
  std::size_t max_pipelined    = 32;           // undelivered messages buffered
};
```

Every limit is a hard error when exceeded (not a truncation). Defaults are
conservative; a server or client may widen them at construction. The
accumulator never holds more than one message's headers plus one chunk plus
the unparsed tail.
