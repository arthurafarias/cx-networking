---
title: "3. The Compiler & Configuration"
weight: 30
---

## 3. The Compiler & Configuration

Implemented in
[render/mustache/compiler.hpp](../../../../include/lambdatech/networking/render/mustache/compiler.hpp)
and
[render/mustache/template.hpp](../../../../include/lambdatech/networking/render/mustache/template.hpp).

### REQ-3.1 — `compiler` is an immutable options bag with fluent setters

```c++
class compiler {
public:
  compiler() = default;                       // jmustache defaults (REQ-3.3)

  compiler escape_html(bool) const;           // returns a modified copy
  compiler standards_mode(bool) const;
  compiler strict_sections(bool) const;
  compiler null_value(std::string) const;
  compiler default_value(std::string) const;  // also disables the missing-var throw
  compiler empty_string_is_false(bool) const;
  compiler zero_is_false(bool) const;
  compiler delimiters(std::string open, std::string close) const;   // default "{{" "}}"
  compiler with_loader(std::shared_ptr<template_loader>) const;
  compiler with_escaper(escaper) const;       // default escapers::html
  compiler with_formatter(formatter) const;   // default formatter (REQ-3.5)
  compiler with_collector(...) const;         // reserved; see REQ-3.6

  template_ compile(std::string_view source) const;
};
```

Every setter returns a new `compiler` by value (cheap: options are small
scalars plus three `shared_ptr`s). A `compiler` is safe to keep as a
configured factory and call `compile` on repeatedly from any thread.

### REQ-3.2 — `compile()` produces an immutable, thread-safe `template_`

```c++
class template_ {
public:
  std::string execute(const core::variant &context) const;
  void        execute(const core::variant &context, sink out) const;

  // convenience: context built inline from an initializer list of pairs
  std::string execute(std::initializer_list<core::object::value_type>) const;
};
```

`template_` holds the parsed segment tree, the resolved options, and a cache
of compiled partials (`§6`). It is `const`-correct and has no mutable state
after construction: concurrent `execute` calls on one `template_` from
different threads are safe and independent. `sink` is
`std::function<void(std::string_view)>`; a `core::buffer`-backed sink and a
`std::string`-backed sink are provided so the renderer can stream straight
into a response without a second copy.

`compile()` throws `mustache::compile_error` (subclass of
`std::runtime_error`) on: an unterminated tag, a section-close name that
does not match the open (`{{#a}}…{{/b}}`), an unbalanced section at EOF, a
malformed delimiter-change tag, or a partial-recursion limit hit at compile
time for statically self-including partials.

### REQ-3.3 — default configuration (matches jmustache)

| Option | Default | Effect |
|---|---|---|
| `escape_html` | `true` | `{{x}}` HTML-escapes; `{{{x}}}` / `{{&x}}` do not |
| `standards_mode` | `false` | jmustache extensions on: `-index` &co., `this`, parent-context fallback for section var lookup |
| `strict_sections` | `false` | a missing **section** name is falsy, not an error |
| `null_value` | *(unset)* | an explicit null interpolates as `""` |
| `default_value` | *(unset)* | a **missing** variable is an error (REQ-3.4) |
| `empty_string_is_false` | `false` | `""` is a truthy section value |
| `zero_is_false` | `false` | numeric `0` is a truthy section value |
| `delimiters` | `{{` `}}` | |
| escaper | `escapers::html` | |
| loader | none | a `{{>partial}}` with no loader is a `compile_error` |

### REQ-3.4 — missing-variable behaviour

With neither `null_value` nor `default_value` set (the default), an
interpolation `{{x}}` whose name resolves nowhere on the context stack
throws `mustache::render_error` from `execute()` — jmustache's
`MustacheException` for an "unresolvable" variable. Precedence:

1. `default_value` set → missing variables and missing dotted-path
   components substitute the default string; no throw.
2. else `null_value` set → an **explicit** null substitutes it; a genuinely
   **missing** name still throws.
3. else → missing throws; explicit null renders `""`.

`strict_sections(true)` extends the throw to a `{{#x}}` / `{{^x}}` whose
name is missing (not merely falsy). Inverted sections over an explicit
null/false always render regardless of strictness.

### REQ-3.5 — the default formatter

`formatter` is `std::function<std::string(const core::variant &)>`, called
for every interpolation *before* escaping. The default:

- string → the string verbatim;
- bool → `"true"` / `"false"`;
- integer → shortest decimal (`std::to_chars`);
- double → shortest round-trip decimal (`std::to_chars`, no trailing
  `.0`), `NaN` / `inf` → `"NaN"` / `"Infinity"` (jmustache/Java parity);
- null → `null_value` if set, else `""`;
- array / object / lambda / custom_context reaching interpolation → the
  formatter is *not* consulted; `§5` defines those (an object or array in a
  `{{x}}` position renders `""` in standards mode and is a `render_error`
  otherwise — jmustache stringifies via `toString`, which we decline).

An application installs its own `formatter` for locale-aware numbers or
date rendering.

### REQ-3.6 — variable collection is deferred

jmustache's `Mustache.collect` / `VisitorContext` (statically enumerating
the names a template references) is useful for validating a context up
front. It is **M5**: `template_::referenced_names()` returning the set of
top-level names and a tree of compound paths. M1–M4 do not expose it.
