---
title: "5. Values, Sections & Lambdas"
weight: 50
---

## 5. Values, Sections & Lambdas

### REQ-5.1 — sections `{{#name}} … {{/name}}`

Resolve `name` (`§2.3`) to a value `v` and render the block per `v`'s kind:

| `v` | Render |
|---|---|
| missing | not rendered (or `render_error` if `strict_sections`) |
| null | not rendered |
| `false` | not rendered |
| `true` | block once, context unchanged |
| empty array | not rendered |
| non-empty array | block **once per element**, each element pushed as the new context top; `-index` &co. bound (`§5.3`) |
| object | block **once**, the object pushed as the new context top (jmustache/spec — an object is not iterated) |
| non-empty string | block once, context unchanged (string is truthy unless `empty_string_is_false`) |
| `""` | block once, unless `empty_string_is_false` → not rendered |
| number `0` | block once, unless `zero_is_false` → not rendered |
| number ≠ 0 | block once |
| `custom_context` | block once, pushed as context top |
| `lambda` (section form) | REQ-5.4 |

The section context is *pushed*, not replaced: an inner tag that does not
resolve against the element still falls back up the stack (non-standards
mode).

### REQ-5.2 — inverted sections `{{^name}} … {{/name}}`

The exact complement of REQ-5.1's "not rendered" rows: the block renders
once (context unchanged) **iff** `name` is missing, null, `false`, an empty
array, `""` with `empty_string_is_false`, or `0` with `zero_is_false`.
Otherwise it is skipped. `strict_sections` does **not** make a missing name
an error for the inverted form (a missing name is the canonical reason to
use `{{^}}`).

### REQ-5.3 — iteration variables (non-standards mode only)

Bound only while rendering an element of an **array** section, shadowed
correctly by nested sections, and absent (→ missing-variable rules) in
standards mode:

| Name | Value |
|---|---|
| `{{-index}}` | 1-based position (`1`, `2`, …) — jmustache uses 1-based |
| `{{-first}}` | `true` on the first element, else `false` (use as `{{#-first}}`) |
| `{{-last}}` | `true` on the last element |
| `{{-odd}}` | `true` when `-index` is odd |
| `{{-even}}` | `true` when `-index` is even |
| `{{-length}}` | element count of the section being iterated |

These match jmustache's non-standard section variables. `{{.}}` remains the
element value itself.

### REQ-5.4 — lambdas

A context value may carry a `mustache::lambda`, held in the variant's user
slot. Two forms, distinguished by which `std::function` member is set:

```c++
struct fragment {
  // re-render this section's raw inner source against `ctx`
  // (defaults to the current context) and return the text
  std::string execute(const core::variant &ctx) const;
  std::string execute() const;                       // current context
  std::string_view source() const;                   // the literal block, unrendered
};

struct lambda {
  // interpolation form: value for a {{lambda}} tag. The returned string is
  // itself parsed and rendered as a Mustache template in the current context
  // and with the current delimiters (spec "~lambdas" Interpolation).
  std::function<std::string()> value;

  // section form: value for a {{#lambda}}…{{/lambda}}. Receives the block.
  // The returned string is parsed and rendered (spec "Section").
  // If it only needs the rendered block, call frag.execute().
  std::function<std::string(const fragment &frag)> section;
};
```

Rules, per the spec's optional lambda module and jmustache's
`Mustache.Lambda`:

- **Interpolation** `{{fn}}` with `fn.value` set → call it; parse-and-render
  the result in the current context; escape the *rendered* result unless the
  tag was `{{{ }}}` / `{{& }}`.
- **Section** `{{#fn}}body{{/fn}}` with `fn.section` set → call it once with
  a `fragment` over `body`; parse-and-render the returned string. A lambda
  used as a section is **not** iterated even if it returns a list-shaped
  string.
- **Inverted** `{{^fn}}` → a lambda is a truthy value; the inverted block is
  skipped. (Spec parity.)
- A lambda reached where the other form is expected (e.g. `fn.value` unset
  but used as `{{fn}}`) is a `render_error`.
- Lambdas run synchronously on the calling thread — on the loop thread when
  invoked from the renderer. They must not block. There is no async lambda.

### REQ-5.5 — the context stack and `execute` re-entrancy

`execute()` maintains an explicit stack (no recursion on the C++ call stack
per section level beyond a bounded depth). Maximum nesting depth of
sections + partials is `limits.max_depth` (`§8` NFR-4); exceeding it is a
`render_error`, never a stack overflow. `fragment::execute` and lambda
re-rendering re-enter the same executor with the current stack snapshot.
