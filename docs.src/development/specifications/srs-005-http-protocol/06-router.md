---
title: "6. http::router"
weight: 60
---

## 6. http::router

Implemented in
[protocol/http/router.hpp](../../../../include/lambdatech/networking/protocol/http/router.hpp).
A dispatch table that sits on `http::server::on_request()` and calls the
first handler whose method and path pattern match.

### REQ-6.1 — create and attach

```c++
static std::shared_ptr<router> create();
void attach(const std::shared_ptr<server> srv);   // subscribes to on_request()
```

`attach` may be called on more than one server. The router keeps a
`weak_ptr` to itself in the subscription, consistent with SRS-003 REQ-2.1.

### REQ-6.2 — registering routes

```c++
using handler = std::function<void(const message &req,
                                   const response_writer &res,
                                   const route_params &params)>;

void route(std::string method, std::string pattern, handler h);
void get(std::string pattern, handler h);      // + post / put / patch / del / head / options
void any(std::string pattern, handler h);      // matches every method
```

Routes are matched in registration order; the first match wins.

### REQ-6.3 — pattern grammar

Patterns are `/`-separated. Each segment is one of:

| Segment | Matches | Capture |
|---|---|---|
| literal (`users`) | that exact, percent-decoded segment | — |
| `:name` | exactly one non-empty segment | `params.get("name")` |
| `*` or `*name` | the rest of the path, including `/` | `params.get("name")`, `"*"` for bare `*` |

A trailing `/` in the pattern and in the request path are normalised away
before matching (so `/users` and `/users/` are the same route). The query
string is never part of matching; `req.target()` still carries it and
`params.query("k")` reads it.

### REQ-6.4 — dispatch outcomes

For each incoming `(req, res)`:

- **match** → the handler runs. An exception escaping the handler is caught
  and turned into `500 Internal Server Error` (the message text is logged
  via `on_error()`, not sent). A handler that returns without calling
  `res.send()` is the handler's bug — the server's REQ-5.4 timeout applies.
- **path matches, method does not** → `405 Method Not Allowed` with an
  `Allow` header listing the methods registered for that path.
- **no path matches** → `404 Not Found`.
- `HEAD` with no explicit `head` route falls through to the matching `get`
  route; the server drops the body (REQ-5.4 / parser `expect_no_body`).
- `OPTIONS *` and per-path `OPTIONS` without a route → `204` with `Allow`.

The 404/405/500 responses are minimal `text/plain`; an application can
override them with `router::set_fallback(handler)` and
`router::set_error_handler(...)`.

### REQ-6.5 — middleware is deferred

An Express-style `use(middleware)` chain (`void(req, res, next)`), route
groups / sub-routers mounted on a path prefix, and per-route middleware are
M6. M4 ships flat routing only.
