---
title: "1. Purpose"
weight: 10
---

## 1. Purpose

SRS-005 gives the library an `http::server` that hands the application a
`(message, response_writer)` pair. The application still has to *produce* the
response body. For anything that emits HTML — a status page, an admin view, a
server-rendered app — hand-concatenating strings is where injection bugs and
unreadable handlers come from. This SRS adds the missing half: a **template
engine** and a one-call **renderer** that fills `response_writer::send` with
an escaped, content-typed body.

One guiding idea: **the engine is self-hosted and the syntax is not ours to
invent.** Mustache is a finished, specified, logic-less template language;
jmustache is a mature, widely-used implementation of it whose behavioural
choices we adopt wholesale. This SRS is therefore mostly a *conformance*
document — it pins which spec modules are implemented, which jmustache
options exist, and how a variant tree stands in for jmustache's reflective
object model. It deliberately adds no new template features.

### 1.1 What "self-hosted" means here

- **Zero third-party code.** The parser, the compiled-template representation,
  the executor, the HTML escaper, and the filesystem partial loader are all
  header-only C++23 under `include/lambdatech/networking/render/`. The only
  link dependency is pthreads, inherited from `core`. No `plugins/` entry,
  no `find_package`.
- **No embedded interpreter, no codegen.** A template compiles to an
  in-memory tree of segment nodes; `execute()` walks it. Compilation is
  done once and the result is immutable and shareable across loop iterations
  and threads.
- **The spec test suite is vendored, not a dependency.** The
  `mustache/spec` YAML files for the required modules are checked in under
  `tests/data/mustache-spec/` and driven by a test group (NFR-9); they are
  data, not a library.

### 1.2 jmustache parity

Someone who knows jmustache should get the same output from the same
template and the same data:

| jmustache | LambdaTech Networking |
|---|---|
| `Mustache.compiler()` | `mustache::compiler{}` |
| `.escapeHTML(false)` / `.standardsMode(true)` / `.strictSections(true)` | `.escape_html(false)` / `.standards_mode(true)` / `.strict_sections(true)` |
| `.nullValue("?")` / `.defaultValue("")` | `.null_value("?")` / `.default_value("")` |
| `.withLoader(loader)` | `.with_loader(loader)` |
| `compiler.compile("Hello {{name}}")` | `compiler.compile("Hello {{name}}")` |
| `template.execute(ctx)` → `String` | `tmpl.execute(ctx)` → `std::string` |
| `template.execute(ctx, writer)` | `tmpl.execute(ctx, sink)` |
| a `Map<String,Object>` / POJO context | a `core::object` (or any `core::variant`) |
| `Mustache.Lambda` | `mustache::lambda` (`§5.4`) |
| `Mustache.CustomContext` | `mustache::custom_context` (`§2.4`) |
| `{{-first}}` / `{{-last}}` / `{{-index}}` | identical (non-standards mode) |
| `{{obj.field}}` compound path | identical |
| missing variable → `MustacheException` | missing variable → `mustache::render_error` (`§3.4`) |

The differences are the ones C++ forces: the context is a variant tree
(there is no reflection over arbitrary structs — a struct reaches a template
either by conversion to `core::object` or through `custom_context`), values
are surfaced as `std::string` through a `formatter` rather than
`String.valueOf`, and there is no locale-sensitive number formatting unless
the application installs a `formatter` that does it.

### 1.3 Out of scope

Client-side rendering, template *inheritance* / blocks (the
`$block` / `<parent` inheritance extension — jmustache does not implement
it either), hot-reload / file-watching of templates, a caching layer keyed on
context, i18n message catalogues, and any non-HTML escaper beyond the
built-in `html` / `none` (an application supplies its own `escaper` for XML,
JSON-string, or URL contexts). Asset bundling and static-file serving belong
to a future SRS. TLS for the rendered response is SRS-005's `plugins/tls/`.
