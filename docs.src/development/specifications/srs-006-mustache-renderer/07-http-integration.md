---
title: "7. HTTP Integration"
weight: 70
---

## 7. HTTP Integration — the server-side renderer

Implemented in
[render/http_view.hpp](../../../../include/lambdatech/networking/render/http_view.hpp).
This is the only file that includes `protocol/http/`. It is a thin bridge:
compiled `template_` + a data context → an `http::message`, delivered
through SRS-005's `response_writer`.

### REQ-7.1 — `render::view`

```c++
class view {
public:
  struct options {
    std::shared_ptr<mustache::template_loader> loader;   // usually a filesystem_loader
    mustache::compiler compiler = {};                    // escaping, mode, etc.
    std::string default_content_type = "text/html; charset=utf-8";
    bool cache_compiled = true;                          // false → recompile per render (dev)
  };

  static std::shared_ptr<view> create(options opt);

  // compile now (eager) or on first use; throws mustache::compile_error
  void add(std::string name, std::string_view source);
  void add_from_loader(std::string name);                // name resolved via opt.loader

  // build a response message (no socket involved)
  http::message render(std::string_view name,
                       const core::variant &context,
                       int status = 200) const;

  // render straight into the SRS-005 writer
  void render_to(const http::response_writer &res,
                 std::string_view name,
                 const core::variant &context,
                 int status = 200) const;
};
```

`render`:

1. looks up (or compiles, honouring `cache_compiled`) the named template;
2. `execute()`s it against `context` into a `core::buffer`-backed sink;
3. returns `http::message::response(status, {{"content-type", default_content_type}})`
   with `body` set to the rendered bytes. `Content-Length` is left for
   `response_writer::send` / `message::serialize` to fill (SRS-005 §2.6 /
   §5.4).

`render_to` calls `render` then `res.send(std::move(msg))` — one call from a
handler.

### REQ-7.2 — error mapping

| Failure | `render` | `render_to` |
|---|---|---|
| unknown template name | throws `std::out_of_range` | same, before any `send` |
| `mustache::compile_error` (lazy compile) | propagates | propagates; nothing sent |
| `mustache::render_error` (missing var, depth cap, bad lambda) | propagates | propagates; nothing sent |

`render_to` never partially sends: it builds the full body first, so a
render failure leaves the `response_writer` untouched and the caller (or
`http::router`'s REQ-6.4 exception guard) turns it into `500`. The renderer
does **not** itself catch and format errors into a response — that policy
lives in the router / handler.

### REQ-7.3 — router convenience

```c++
// register a GET route that renders `template_name` with a context built
// per request by `build` (params + query -> variant).
void get_view(http::router &r, std::string pattern,
              std::string template_name,
              std::function<core::variant(const http::message &,
                                          const http::route_params &)> build,
              std::shared_ptr<view> v);
```

A convenience wrapper over `router::get` (SRS-005 §6.2): it calls `build`,
then `v->render_to(res, template_name, ctx)`, with the handler-exception
path (SRS-005 REQ-6.4) catching any `render_error` as `500`.

### REQ-7.4 — what the renderer does not do

- No layout / master-template mechanism — compose with a partial
  (`{{>layout}}`) or render an inner view into a `content` string and pass
  it to an outer view.
- No automatic `ETag` / `Last-Modified` / conditional-GET handling — an
  application adds those headers to the returned `http::message`.
- No streaming of a half-rendered template to the socket — the body is
  fully built before `send` (bounded by NFR-4). Chunked streaming of a large
  view is a possible **M6** once SRS-005 M6 lands its streaming writer.
- No content negotiation — the caller picks the template and the
  `content_type`.

### REQ-7.5 — standalone use

`render::view` needs `http::message` but never a socket; `mustache::*` needs
neither. A CLI tool or a test can `compiler{}.compile(src).execute(ctx)` with
no part of `protocol/` linked. The `lnw-example-http-view` example (M4) and
an `lnw-mustache` one-shot CLI tool (`render <template> <context.json>`,
M5 — JSON parsing via a tiny vendored reader or `core::object` literal) show
both paths.
