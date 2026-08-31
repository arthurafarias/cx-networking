---
title: "8. Non-Functional Requirements"
weight: 80
---

## 8. Non-Functional Requirements

### NFR-1 — header-only, zero third-party

All of `render::` is headers under
`include/lambdatech/networking/render/`. The only link dependency is
pthreads (inherited from `core`). No `find_package`, no `plugins/` entry, no
vendored library beyond the `core::variant` / `core::object` / `core::array`
headers SRS-005 already vendors from cxflow. `<filesystem>` and
`<charconv>` (STL) are the extent of the "exotic" standard headers used.

### NFR-2 — self-hosted parser and executor

The template parser, the compiled-tree representation, the executor, the
HTML escaper, and the filesystem loader are original code in this repo. No
generated code, no embedded scripting engine, no regex-driven parsing (a
hand-written scanner — regex is neither fast enough nor precise enough for
the standalone-whitespace rules).

### NFR-3 — no blocking, no I/O on the loop thread

`execute()` is pure in-memory string work. All filesystem access is confined
to `filesystem_loader::load`, which runs **only during `compile()`**
(REQ-6.2). An application that compiles its views at startup (the default,
`cache_compiled = true`) does zero I/O per request. `cache_compiled = false`
(dev hot-reload) re-reads files on the calling thread and is documented as
not for production.

### NFR-4 — bounded memory and recursion

```c++
struct limits {
  std::size_t max_output_bytes = 8 * 1024 * 1024;  // execute() aborts past this
  std::size_t max_depth        = 64;               // sections + partials nesting
  std::size_t max_template_bytes = 1 * 1024 * 1024; // one source file
};
```

Exceeding any limit is a `mustache::render_error` (or `compile_error` for
`max_template_bytes`), never an OOM, a hang, or a C++ stack overflow — the
executor uses an explicit heap stack (REQ-5.5). A recursive partial or a
pathological lambda is bounded by `max_depth` and `max_output_bytes`. The
renderer's response body is therefore bounded before it reaches
`response_writer::send`, complementing SRS-005 NFR-4.

### NFR-5 — compiled templates are immutable and thread-safe

A `template_` has no mutable state after `compile()`. Concurrent `execute()`
from multiple threads on one `template_` is safe and lock-free. `view` with
`cache_compiled = true` is likewise safe to share across the event-loop
thread and any worker threads. A `custom_context::get` and any `lambda` the
application installs must themselves be thread-safe if the app renders off
the loop thread.

### NFR-6 — deterministic, escaping-correct output

Given the same template, options, and context, `execute()` is byte-for-byte
deterministic (object key order is insertion order — `core::object` — and
`std::to_chars` float formatting is deterministic). HTML escaping is on by
default (REQ-3.3) and applies to every `{{x}}`; producing unescaped output
requires the explicit `{{{x}}}` / `{{&x}}` or `escape_html(false)`. There is
no path by which context data reaches an escaped tag without passing through
the escaper.

### NFR-7 — spec conformance is measured

The required spec modules (`interpolation`, `sections`, `inverted`,
`comments`, `delimiters`, `partials`) and the `~lambdas` module are driven
from the checked-in `mustache/spec` YAML fixtures by the `mustache::spec`
test group (NFR-9). Each fixture is a case; the group asserts exact output.
Deviations that are intentional (the broader HTML escape set REQ-4.4, the
jmustache extensions when `standards_mode(false)`) are encoded as an
allow-list in the test with a comment pointing at the REQ that justifies
each, so an unexpected deviation fails CI.

### NFR-8 — jmustache parity is documented, not enforced

Where behaviour intentionally differs from jmustache — the variant data
model instead of reflection, `render_error` instead of `MustacheException`,
`std::to_chars` number formatting instead of Java's `String.valueOf`,
snake_case option names — the difference is stated in this SRS (`§1.2`,
`§2`, `§3.5`) and in the docs, not worked around.

### NFR-9 — bounded tests

Every render test group is pure in-memory and finishes in milliseconds. The
one group that stands up a socket (`render::http_view`, exercising `view` +
`http::router` + `tcp::server`) uses a `127.0.0.1` ephemeral port, drives
the local `core::event_loop`, and wraps every wait in
`testing::await(future, timeout)` (SRS-003 NFR-4, testing-convention
guideline). `filesystem_loader` tests write fixtures into a unique
temp directory and remove it in teardown.
