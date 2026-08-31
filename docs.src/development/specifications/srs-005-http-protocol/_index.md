---
title: "SRS-005: HTTP Messages, Client, Server & Router"
weight: 5
---

**Status:** Specified, not started — no `protocol::http` code exists yet.
This document defines the HTTP/1.1 surface: a key/value **message
container**, an incremental **parser/serializer**, an `http::client` over
[`tcp::client`](../srs-003-protocol-clients-servers/03-tcp/), an
`http::server` over `tcp::server`, and an `http::router` that dispatches
parsed requests to handlers.

**Author:** Arthur de Araújo Farias
**Date:** 2026-08-31

**Depends on:** [SRS-003](../srs-003-protocol-clients-servers/) (the TCP
transport), [SRS-002](../srs-002-async-network-core/) (the `core` runtime),
and the [cxflow](https://github.com/arthurafarias/cxflow) library's
`containers::object` — an observable, ordered bag of variant properties —
vendored verbatim (namespace-swapped) into `lambdatech::networking::core`
as `core::object`, the same way SRS-002 vendors the threading primitives.

Every HTTP message — request or response, on the client or the server — is
one `core::object`: a flat map of string keys to string values with two
reserved keys (`header` for the start line, `body` for the entity body) and
one key per header field. The parser turns bytes into that object; the
serializer turns it back into bytes; the router matches on it. Lives at
`include/lambdatech/networking/protocol/http/`.

## Sections

| # | Section |
|---|---|
| 1 | [Purpose](01-purpose) |
| 2 | [The Message Container](02-message-container) |
| 3 | [The Parser & Serializer](03-parser) |
| 4 | [http::client](04-client) |
| 5 | [http::server](05-server) |
| 6 | [http::router](06-router) |
| 7 | [Non-Functional Requirements](07-nfr) |
| 8 | [Milestones](08-milestones) |
