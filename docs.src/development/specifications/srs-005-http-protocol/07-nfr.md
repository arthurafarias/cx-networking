---
title: "7. Non-Functional Requirements"
weight: 70
---

## 7. Non-Functional Requirements

### NFR-1 — header-only

All of `protocol::http` is headers under
`include/lambdatech/networking/protocol/http/`. The only link dependency is
pthreads (inherited from `core`). `core::object` and its `core::map` /
`core::variant` support are vendored headers under
`include/lambdatech/networking/core/`.

### NFR-2 — vendored `core::object`, no divergence

`core/object.hpp` (and the `map` / `variant` / `variant_map` headers it
needs) are copies of cxflow's `containers::` equivalents with only the
namespace and include paths changed, each recording its origin in the file
header — the same contract SRS-002 §2 sets for the threading primitives.
Fixes go back to cxflow and are re-vendored, not forked.

### NFR-3 — no blocking on the loop thread

The only blocking call in the HTTP path is `getaddrinfo`, reached through
`tcp::client::connect`, which already runs it on `core::thread_pool`. The
parser, serializer, router matching, and all body handling are pure
in-memory work on the loop thread.

### NFR-4 — bounded memory

`http::parser::limits` (§3.7) caps the start line, header block, body, and
pipelined-message backlog. Defaults hold a single connection to well under
2 MiB regardless of peer behaviour. A slow-loris peer is bounded by the same
limits plus the server's header-to-response timeout (REQ-5.4); no
per-connection thread is ever spawned.

### NFR-5 — safe teardown

`http::client`, `http::server`, and `http::router` follow SRS-003 REQ-2.1 /
NFR-3: `create()` factories, `weak_ptr` in every callback, destruction from
any thread at any time is use-after-free-free because the underlying
`tcp::*` sockets already guarantee it and the HTTP objects add only
plain-data state.

### NFR-6 — HTTP/1.1 (and 1.0) only; TLS and HTTP/2 are plugins

This SRS implements RFC 9110/9112 message syntax for versions `HTTP/1.0` and
`HTTP/1.1` over cleartext TCP. `https://`, HTTP/2, and HTTP/3 are explicitly
out of core and arrive as `plugins/tls/` and `plugins/http2/` behind the
same `http::client` / `http::server` / `message` API (SRS guideline: one
plugin, one library, one directory).

### NFR-7 — bytes in, bytes out

Header field values and the body are opaque octet strings (`std::string`).
The library performs no charset decoding, no percent-decoding of the body,
no JSON/form parsing, and no content-negotiation. Percent-decoding happens
only for router path-segment matching (§6.3).

### NFR-8 — Node/Express parity is documented, not enforced

Where the surface intentionally differs from Node's `http` module or
Express (typed events, `shared_ptr`, `core::buffer` bodies, the flat
key/value message instead of `IncomingMessage`/`ServerResponse`, no
implicit middleware), the difference is called out in this SRS and the docs
(§1.1), not worked around.

### NFR-9 — bounded tests

Every HTTP test group (`http::message`, `http::parser`, `http::server`,
`http::client+server`, `http::router`) runs against `127.0.0.1` ephemeral
ports where it uses a socket, drives the local `core::event_loop`, and wraps
every wait in `testing::await(future, timeout)` so a regression fails a case
rather than hanging CI (SRS-003 NFR-4, testing-convention guideline).
