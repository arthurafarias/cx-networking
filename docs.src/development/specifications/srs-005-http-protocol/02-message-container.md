---
title: "2. The Message Container"
weight: 20
---

## 2. The Message Container

Implemented in
[protocol/http/message.hpp](../../../../include/lambdatech/networking/protocol/http/message.hpp),
over
[core/object.hpp](../../../../include/lambdatech/networking/core/object.hpp).

### REQ-2.1 — `http::message` is a thin view over `core::object`

`http::message` holds one `core::object` by value and adds HTTP-aware
accessors. It is a plain value type: copyable, movable, default-constructible
to the empty object. `message::object()` exposes the underlying container for
generic iteration and for the `property_changed` signal.

```c++
class message {
public:
  static inline const std::string start_line_key = "header";
  static inline const std::string body_key       = "body";

  core::object       &object()       { return obj_; }
  const core::object &object() const { return obj_; }
  // ...
};
```

### REQ-2.2 — reserved keys

Exactly two keys are reserved and are **not** header fields:

| Key | Meaning |
|---|---|
| `header` | the start line, stored verbatim without its trailing CRLF — a request line (`GET /p HTTP/1.1`) or a status line (`HTTP/1.1 200 OK`) |
| `body` | the decoded entity body as a UTF-8-agnostic byte string (`std::string`; may contain NULs) |

Every other key is a header field name. A real HTTP field literally named
`Header` or `Body` (neither is a registered field; both are vanishingly
rare) would collide; for that case the parser and serializer also accept the
colon-prefixed aliases `:start-line` and `:body`, which can never appear as
wire field names. Aliases and reserved keys are interchangeable on input;
serialization always emits from whichever is present, preferring the
colon-prefixed form only if both exist.

### REQ-2.3 — field access is case-insensitive

`field(name)`, `set_field(name, value)`, `has_field(name)`, and
`remove_field(name)` lower-case `name` before touching the object, so keys
are stored lower-cased. `field(name)` returns `std::optional<std::string>`.
Callers that want the raw container semantics use `message::object()`
directly.

### REQ-2.4 — multi-valued fields

A field that appears more than once on the wire is combined into one value,
comma-joined, in first-seen order (RFC 9110 §5.2). `Set-Cookie` is the
documented exception: it is never folded — `set_cookies()` returns
`std::vector<std::string>` and the serializer emits one `Set-Cookie:` line
per element. Internally the object stores the list under `set-cookie` as a
newline-joined value; callers should use the accessor, not `field()`.

### REQ-2.5 — start-line accessors

Parsed from `field(start_line_key)` on demand, never cached:

| Method | For | Returns |
|---|---|---|
| `method()` | requests | `std::string` (`"GET"`, …), empty if not a request line |
| `target()` | requests | request target (`"/users/42?q=1"`) |
| `version()` | both | `"HTTP/1.1"`, `"HTTP/1.0"` |
| `status_code()` | responses | `int` (`200`), `0` if not a status line |
| `reason()` | responses | reason phrase (`"OK"`) |

Setters `set_request_line(method, target, version = "HTTP/1.1")` and
`set_status_line(code, reason = "", version = "HTTP/1.1")` compose the
string (an empty `reason` is filled from a built-in code→phrase table).

### REQ-2.6 — one-shot `serialize()` / `parse()`

```c++
core::buffer serialize() const;                 // never fails: a message with
                                                // no start line serializes an
                                                // empty first line
static std::optional<message> parse(std::span<const std::byte> whole);
```

`serialize()`:

1. writes `field("header")` (or `""`), then CRLF;
2. writes each remaining key except `body` / `:body` as
   `Canonical-Name: value` + CRLF, in object order, `Set-Cookie` expanded
   per REQ-2.4. `Canonical-Name` is `Train-Case` (`content-length`,
   `www-authenticate` → `Content-Length`, `WWW-Authenticate` via a small
   known-token table, otherwise capitalise each `-`-separated part);
3. if `body` is non-empty (or present and the start line is a response) and
   neither `content-length` nor `transfer-encoding` is set, inserts
   `Content-Length: <byte count>`;
4. writes CRLF, then the raw `body` bytes.

`parse()` is the whole-buffer convenience wrapper around `http::parser`
(§3): it feeds the span, calls `finish()`, and returns the single message or
`std::nullopt` on any protocol error or trailing garbage. Servers and
clients use the streaming `http::parser` instead.

### REQ-2.7 — construction helpers

- `message::request(method, url_or_target, fields = {})` — sets the request
  line; if given an absolute URL, also sets `host` and returns the parsed
  `authority` via `message::url()` for the client to dial.
- `message::response(code, fields = {})` — sets the status line.
- Both take an initializer list of `{name, value}` pairs for fields and
  leave `body` empty.
