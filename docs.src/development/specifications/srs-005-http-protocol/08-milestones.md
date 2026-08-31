---
title: "8. Milestones"
weight: 80
---

## 8. Milestones

| Milestone | Covers | Status |
|---|---|---|
| **M1** | Vendor `core::object` (+ `map` / `variant`). `http::message` (§2) and `http::parser` (§3): request & response modes, folded headers, `Content-Length`, chunked decoding, limits, `serialize()` / one-shot `parse()`. Test groups `http::message`, `http::parser`. | Not started |
| **M2** | `http::server` (§5) + `response_writer`, over `tcp::server`. `on_request` event, `400` on bad input, one-request-at-a-time pipelining, persistence decision. Test group `http::server` (loopback). | Not started |
| **M3** | `http::client` (§4) over `tcp::client`: URL parsing, connection-per-request, `Host`/`Content-Length` defaults, request timeout, `HEAD`. Test group `http::client+server`. `examples/lnw-example-http-get.cpp` (`./http-get <url>`). | Not started |
| **M4** | `http::router` (§6): literal / `:param` / `*wildcard` segments, first-match dispatch, `404` / `405` (+`Allow`) / `500`, `HEAD`→`GET` fallthrough, overridable fallbacks. Test group `http::router`. `examples/lnw-example-http-echo.cpp` (routed echo/JSON server). | Not started |
| **M5** | Keep-alive: persistent server connections with an ordered write queue, client per-host idle-connection pool + pipelining, `Expect: 100-continue`. | Not started |
| **M6** | Streaming bodies: chunked request/response writer API, trailer emission, backpressure surfaced from `tcp::client::write` / `drain`. Express-style `router::use(middleware)` and mounted sub-routers. | Not started |
| **M7** | `plugins/tls/` — `https://` for `http::client` and TLS termination for `http::server` (OpenSSL) behind the unchanged API. `plugins/http2/` (nghttp2) exploration. Optional `dns::client`-backed resolution cache. | Not started |

### File map when M1–M4 land

```
include/lambdatech/networking/
  core/
    object.hpp  map.hpp  variant.hpp  variant_map.hpp   (vendored from cxflow)
  protocol/http/
    message.hpp   parser.hpp   client.hpp   server.hpp   router.hpp
    testing/
      message_test.hpp   parser_test.hpp
      server_test.hpp     client_server_test.hpp   router_test.hpp
examples/
  lnw-example-http-get.cpp
  lnw-example-http-echo.cpp
```
