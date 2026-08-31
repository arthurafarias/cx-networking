---
title: "4. Template Syntax & Semantics"
weight: 40
---

## 4. Template Syntax & Semantics

The grammar and whitespace rules are `mustache(5)` / the Mustache spec
verbatim. This section states which constructs are recognised and pins the
spec-defined behaviour the parser must reproduce; `§5` covers what section
and variable *values* do.

### REQ-4.1 — tag types

| Tag | Name | Behaviour |
|---|---|---|
| `{{ name }}` | variable | resolve (`§2.3`), format (`REQ-3.5`), escape (`REQ-4.4`), emit |
| `{{{ name }}}` | unescaped variable | as above, no escaping. Only valid with `{{`/`}}` delimiters |
| `{{& name }}` | unescaped variable | same as `{{{ }}}`, works under any delimiters |
| `{{# name }}` … `{{/ name }}` | section | `§5.1` |
| `{{^ name }}` … `{{/ name }}` | inverted section | `§5.2` |
| `{{! text }}` | comment | discarded; may span lines; contents never parsed |
| `{{> name }}` | partial | `§6` |
| `{{= L R =}}` | set delimiters | changes the active delimiters for the rest of the *current* template level from this point |

Surrounding whitespace inside a tag is trimmed from the name
(`{{ name }}` ≡ `{{name}}`). A name may be a compound path (`{{a.b}}`) for
every tag type except the delimiter tag.

### REQ-4.2 — delimiters

Default `{{` `}}`. `{{=<% %>=}}` switches to `<%` `%>`; `<%={{ }}=%>`
switches back. A delimiter change:

- takes effect immediately after the tag and lasts to the end of the
  enclosing section or template;
- does **not** leak into or out of a partial — a partial is parsed with the
  delimiters in force at its `{{>}}` site's *start*, per spec;
- is a standalone tag for whitespace purposes (REQ-4.3);
- with `{{{ }}}` triple-mustache is only meaningful while delimiters are the
  default; after a change, use `{{& }}` for unescaped output.

`compiler.delimiters("<%", "%>")` sets the *initial* delimiters, equivalent
to a leading `{{= =}}`.

### REQ-4.3 — standalone tags and whitespace

Reproduce the spec's "standalone" rule exactly. A line that contains only a
section-open, section-close, inverted-open, comment, partial, or
delimiter-change tag plus optional surrounding whitespace (and a line
ending, or EOF) is a **standalone** tag: the whitespace and the line's
newline are removed from the output, so block tags do not leave blank lines.
A variable tag (`{{x}}`, `{{{x}}}`, `{{&x}}`) is **never** standalone. A
partial's own content is re-indented by the standalone partial tag's leading
whitespace (spec "Standalone Indentation").

Non-standalone whitespace is preserved byte for byte. `\r\n` and `\n` line
endings are both handled and passed through unchanged where not stripped.

### REQ-4.4 — HTML escaping

When `escape_html` is true, `{{name}}` escapes the formatted value by
replacing, in one left-to-right pass:

| char | replacement |
|---|---|
| `&` | `&amp;` |
| `<` | `&lt;` |
| `>` | `&gt;` |
| `"` | `&quot;` |
| `'` | `&#39;` |
| `` ` `` | `&#96;` |
| `=` | `&#61;` |

This is jmustache's `Escapers.HTML` set (broader than the spec's minimal
`& < > "` — it also neutralises `'`, `` ` `` and `=` for unquoted-attribute
safety). `{{{name}}}` and `{{&name}}` bypass it. `escape_html(false)` makes
`{{name}}` behave like `{{&name}}` globally. A custom `escaper`
(`compiler.with_escaper`) replaces the whole function; `escapers::none` is
the identity.

### REQ-4.5 — compile-time errors

Detected during `compile()` (never deferred to `execute`):

- a tag opened and not closed before EOF or before a newline where the
  delimiter forbids it;
- `{{/name}}` with no matching open, or a name mismatch with the innermost
  open section;
- a section left open at EOF;
- `{{=` … without a well-formed `=}}` and exactly two space-separated
  non-empty delimiter tokens, neither containing `=`;
- `{{{` under non-default delimiters;
- a `{{>name}}` when the compiler has no loader **and** `name` is not a
  known in-memory partial.

All raise `mustache::compile_error` with a 1-based line and column.
