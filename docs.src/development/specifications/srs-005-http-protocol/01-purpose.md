---
title: "1. Purpose"
weight: 10
---

## 1. Purpose

SRS-003 gives the library a byte-stream transport (`tcp::client` /
`tcp::server`). This SRS puts HTTP/1.1 on top of it, with one guiding idea:
**an HTTP message is a key/value object, nothing more.** The wire framing
(the request line, `Name: value` folding, `Content-Length`, chunked
transfer coding) is an encoding detail the parser and serializer own; every
layer above them — the router, the client callback, the server handler —
sees only:

```jsonc
{
  "header": "GET /users/42 HTTP/1.1",   // the start line, verbatim
  "host": "api.example.com",             // one key per header field
  "accept": "application/json",
  "body": ""                             // the entity body, decoded
}
```

A response is the same shape with a status line in `header`:

```jsonc
{
  "header": "HTTP/1.1 200 OK",
  "content-type": "application/json",
  "body": "{\"id\":42}"
}
```

That object is `core::object` (vendored from cxflow's
`containers::object`): an insertion-ordered map with a `property_changed`
signal. Using it — rather than a bespoke struct with typed `method` /
`target` / `status` fields — means serialization and deserialization are
total (`for (auto& [k, v] : msg)`), a handler can add, drop, or rewrite any
field by name, and the router matches on the same container the transport
produced with no adapter type in between.

### 1.1 Node.js parity

Someone who knows Node's `http` module and Express should read and write
this layer with almost no translation:

| Node.js | LambdaTech Networking |
|---|---|
| `http.createServer(fn)` | `protocol::http::server` + `on_request()` |
| `http.request(opts, cb)` / `fetch` | `protocol::http::client::request(...)` |
| `express()` app / `Router` | `protocol::http::router` |
| `IncomingMessage` | `const http::message &` |
| `ServerResponse` | `const http::response_writer &` |
| `req.method`, `req.url` | `msg.method()`, `msg.target()` (parsed from `header`) |
| `req.headers['content-type']` | `msg.field("content-type")` |
| `res.writeHead(200).end(body)` | `res.send(http::message{...})` |
| `app.get('/u/:id', fn)` | `router->get("/u/:id", fn)` |

The differences are the ones C++ and SRS-003 already force: objects are held
through `std::shared_ptr`, events are typed `core::event<...>` accessors,
bodies are `core::buffer`, and there is no implicit global server.

### 1.2 Out of scope

HTTP/2 and HTTP/3, TLS (`https://`), automatic decompression, cookie jars,
proxy handling, and a caching layer are **not** in this SRS. TLS and HTTP/2
arrive later as `plugins/` (SRS guideline: anything needing a third-party
library lives under `plugins/<name>/`). `http::client` given an `https` URL
emits `error` until the TLS plugin is present.
