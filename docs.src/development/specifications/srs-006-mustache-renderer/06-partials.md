---
title: "6. Partials & Loaders"
weight: 60
---

## 6. Partials & Loaders

Implemented in
[render/mustache/loader.hpp](../../../../include/lambdatech/networking/render/mustache/loader.hpp).

### REQ-6.1 — the loader interface

```c++
struct template_loader {
  virtual ~template_loader() = default;
  // return the raw source for partial `name`, or std::nullopt if unknown.
  virtual std::optional<std::string> load(std::string_view name) const = 0;
};
```

`compiler.with_loader(std::make_shared<...>())` installs it. `load` is
called at **compile time**: `compile()` walks every `{{>name}}` reachable
from the root, loads and parses each once, and stores the compiled partial
in the `template_`'s partial cache (REQ-3.2). A partial not found at compile
time is a `compile_error` unless `default_value` is set, in which case the
partial renders as the empty string (jmustache: a missing partial renders
nothing rather than throwing when lenient).

### REQ-6.2 — built-in loaders

| Loader | Source |
|---|---|
| `map_loader{{ "header", "…" }, { "footer", "…" }}` | an in-memory `core::object` / `std::map` of name → source. Zero I/O. |
| `filesystem_loader{root, ext = ".mustache"}` | reads `root / (name + ext)` via `<filesystem>` + `std::ifstream`. |
| `chain_loader{a, b, …}` | first non-empty `load` wins. |

`filesystem_loader`:

- **rejects path traversal.** `name` is split on `/`; any component that is
  `.`, `..`, empty, absolute, a Windows drive, or contains a NUL is a
  `load` failure (→ `compile_error`). The resolved path must stay lexically
  under `root` after `weakly_canonical`.
- reads the file **once, at compile time**. There is no watch, no re-read,
  no cache invalidation — recompile the template to pick up an edit. (Dev
  hot-reload is an application concern, e.g. recompiling per request behind
  a flag.)
- is the only part of the engine that touches the filesystem, and it does so
  **only during `compile()`**, never during `execute()` — so `execute()`
  from the loop thread never does I/O (NFR-3).

### REQ-6.3 — partial semantics

Per the spec's `partials` module:

- a partial is rendered with the **current context** at its tag site (it is
  not a new scope);
- the partial's source is parsed with the delimiters active at the `{{>}}`
  tag's start (REQ-4.2) — a partial cannot see or change the parent's later
  delimiters, and its own `{{= =}}` does not escape it;
- standalone-partial indentation (REQ-4.3) prefixes every line the partial
  emits with the tag's leading whitespace;
- a partial may reference partials; the compile-time walk is depth-first
  with a cycle check.

### REQ-6.4 — recursion

Mutually or directly recursive partials (`{{>node}}` inside `node`) are
allowed and compile fine (the cache breaks the compile-time cycle). At
**render** time recursion is bounded by `limits.max_depth` (`§8` NFR-4);
a recursive partial that never hits a falsy section to stop it produces a
`render_error` at the depth cap, not an OOM or a crash. The classic
recursive-tree spec fixture renders correctly within the default depth.

### REQ-6.5 — dynamic partials are out of scope

`{{>*name}}` (the partial name taken from a context variable) is not
implemented — jmustache does not implement it either. A partial name is
always a compile-time literal.
