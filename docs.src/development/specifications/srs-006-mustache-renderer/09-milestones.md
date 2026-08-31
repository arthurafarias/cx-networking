---
title: "9. Milestones"
weight: 90
---

## 9. Milestones

| Milestone | Covers | Status |
|---|---|---|
| **M1** | The scanner + `compiler` + `template_`: `{{var}}`, `{{{var}}}`, `{{&var}}`, `{{! comment }}`, `{{=L R=}}` delimiter changes, the HTML escaper (REQ-4.4), standalone-whitespace rules (REQ-4.3), the default `formatter`, compound paths (`{{a.b.c}}`) and `{{.}}`. Context is `core::variant`. Missing-variable throw + `null_value` / `default_value`. Test groups `mustache::parser`, `mustache::compiler`, and `mustache::spec` for the `interpolation`, `comments`, `delimiters` modules. | Not started |
| **M2** | Sections and inverted sections (REQ-5.1 / REQ-5.2) over bool / number / string / array / object / `custom_context`, the context stack with parent-context fallback, `strict_sections`, `empty_string_is_false`, `zero_is_false`, `standards_mode`. `{{-index}}` / `{{-first}}` / `{{-last}}` / `{{-odd}}` / `{{-even}}` / `{{-length}}`. `mustache::spec` for `sections`, `inverted`. Test group `mustache::sections`. | Not started |
| **M3** | Partials (`§6`): `template_loader`, `map_loader`, `filesystem_loader` (with traversal rejection), `chain_loader`, compile-time partial walk + cache, standalone indentation, recursion bounded by `limits.max_depth`. `mustache::spec` for `partials`. Test group `mustache::partials`. | Not started |
| **M4** | Lambdas (REQ-5.4): interpolation + section forms, `fragment`, parse-and-render of results. `mustache::spec` `~lambdas`. `render::view` + `render_to` + `get_view` (`§7`) over SRS-005. Test group `mustache::lambda`, `render::http_view` (loopback). `examples/lnw-example-http-view.cpp` (a routed server rendering a page + partial layout). | Not started |
| **M5** | `template_::referenced_names()` (REQ-3.6) for up-front context validation. `lnw-mustache` CLI tool (`render <template> <context.json>`) with a minimal vendored JSON→`core::variant` reader. `cache_compiled = false` dev hot-reload path documented + tested. | Not started |
| **M6** | Streaming render: `execute(context, sink)` wired to a chunked response body once SRS-005 M6 lands its streaming `response_writer`, so a large view never fully buffers. Backpressure surfaced from the sink. | Not started |
| **M7** | Additional escapers shipped (`escapers::xml`, `escapers::json_string`, `escapers::uri`), a `formatter` recipe for locale/date rendering in the docs, and a fuzz target over `compiler::compile` + `execute` (libFuzzer harness under `tests/fuzz/`, into the coverage build) — parity with SRS-001 M5. | Not started |

### File map when M1–M4 land

```
include/lambdatech/networking/render/
  mustache/
    value.hpp   escaper.hpp   formatter.hpp   loader.hpp   lambda.hpp
    parser.hpp  template.hpp  compiler.hpp
    testing/
      spec_test.hpp   parser_test.hpp   compiler_test.hpp
      sections_test.hpp   partials_test.hpp   lambda_test.hpp
  http_view.hpp
  testing/
    http_view_test.hpp
examples/
  lnw-example-http-view.cpp
tests/data/mustache-spec/
  interpolation.yml  sections.yml  inverted.yml  comments.yml
  delimiters.yml  partials.yml  ~lambdas.yml
```
