---
title: "2. Scope & Data Model"
weight: 20
---

## 2. Scope & Data Model

### REQ-2.1 — placement and layout

The engine is header-only core code, not a plugin (it needs nothing beyond
the STL and the already-vendored `core` containers). It lives at:

```
include/lambdatech/networking/render/
  mustache/
    value.hpp        # core::variant aliases + custom_context interface
    escaper.hpp      # escapers::html / ::none / custom
    formatter.hpp    # variant scalar -> std::string
    loader.hpp       # template_loader interface + filesystem_loader + map_loader
    lambda.hpp       # mustache::lambda (value + section forms), fragment
    template.hpp     # compiled template_ : execute(ctx) / execute(ctx, sink)
    compiler.hpp     # mustache::compiler : options + compile(source)
    parser.hpp       # source text -> segment tree (internal; not for callers)
    testing/
      spec_test.hpp        # drives tests/data/mustache-spec/*.yml
      compiler_test.hpp    parser_test.hpp
      sections_test.hpp    partials_test.hpp    lambda_test.hpp
  http_view.hpp      # render::view : template_ + loader -> http::message / send()
  testing/
    http_view_test.hpp      # loopback: router + view, over tcp::server
```

Namespace `lambdatech::networking::render`; the engine sub-namespace is
`lambdatech::networking::render::mustache`, aliased `mustache`. The HTTP glue
(`render::view`) is the only part that includes anything from
`protocol/http/`.

### REQ-2.2 — the data context is a `core::variant`

`execute()` takes one context value: a `core::variant` (SRS-005 §2 —
vendored from cxflow `containers::variant`). A variant is exactly one of:

| Variant kind | Template meaning |
|---|---|
| **null** / monostate | absent for interpolation (`§3.4`); falsy for sections |
| **bool** | falsy if `false`; a `true` renders its section once |
| **integer** (`std::int64_t`) / **double** | interpolated via the `formatter`; falsy only if `zero_is_false` and value is `0` |
| **string** (`std::string`) | interpolated as-is; falsy only if `empty_string_is_false` and value is `""` |
| **array** (`core::array` — ordered `variant` list) | a section iterates it, pushing each element as the section context; falsy if empty |
| **object** (`core::object` — ordered `string → variant` map) | a variable path indexes it by key; a section pushes it as context once (not iterated); never falsy when present |
| **`mustache::lambda`** (held in the variant's user slot) | `§5.4` |
| **`std::shared_ptr<custom_context>`** (user slot) | `§2.4` |

`core::array` is the ordered `std::vector<core::variant>` alias and
`core::object` the insertion-ordered map, both defined alongside
`core::variant` in the vendored headers. The engine adds only the two user
slot types (`lambda`, `custom_context`).

### REQ-2.3 — name resolution

A tag name is either a single key or a dot-separated **compound path**
(`a.b.c`). Resolution of the first component walks the **context stack**
(the current context and every enclosing section context, innermost first —
jmustache's non-standards-mode "parent fallback"); each remaining component
indexes strictly into the value the previous one produced:

1. `.` (a lone dot) resolves to the top of the context stack — the "implicit
   iterator" (`{{.}}` inside a list-of-scalars section).
2. `this` is an alias for `.` in non-standards mode.
3. For `a.b.c`: find `a` by stack walk; then `b` must be a key of that
   object (or a `custom_context` lookup); then `c` likewise. A missing
   intermediate is the same as a missing variable (`§3.4`). In
   **standards mode** the whole dotted name is also tried as one literal key
   first, then split (spec §"Dotted Names").
4. `-first` / `-last` / `-index` / `-odd` / `-even` / `-first`... — see
   `§5.3`; only bound inside an iterating section, only in non-standards
   mode.

### REQ-2.4 — `custom_context` (the reflection escape hatch)

For data that is computed, lazy, or backed by a real C++ object rather than
a `core::object`, a context value may be a `std::shared_ptr<custom_context>`:

```c++
struct custom_context {
  virtual ~custom_context() = default;
  // return the value for `name`, or std::nullopt if this context does not
  // define it (resolution then continues up the context stack).
  virtual std::optional<core::variant> get(std::string_view name) const = 0;
};
```

This is jmustache's `Mustache.CustomContext`. It participates in the stack
walk (REQ-2.3) exactly like an object: `get` is called for the first path
component; a returned variant is then indexed by any remaining components.
`get` must be pure and fast — it runs on the loop thread when the renderer
is driven from `http::server`.

### REQ-2.5 — in scope (spec modules)

The required Mustache spec modules are **fully** implemented:
`interpolation`, `sections`, `inverted`, `comments`, `delimiters`,
`partials`. The optional **`~lambdas`** module is implemented with the C++
`mustache::lambda` shape (`§5.4`). The optional **`~inheritance`** module is
**not** implemented (parity with jmustache — `§1.3`).

### REQ-2.6 — out of scope

Template inheritance / blocks, dynamic partials (`{{>*name}}`), template
pre-compilation to a file, and any data source other than a `core::variant`
tree or `custom_context`. Reading a struct's fields by name without an
explicit `to_variant` conversion or a `custom_context` is not possible and
is not a goal.
