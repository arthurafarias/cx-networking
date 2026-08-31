# Documentation source

Hugo project that generates the documentation site. Two content trees, two
build environments, two branches.

## Layout

| Path                     | Contents                                                        |
| ------------------------ | -------------------------------------------------------------- |
| `public/`                | Public API / usage documentation. Section: `docs/`.            |
| `development/`           | Internal docs — `specifications/` (SRS) and `notes/`.          |
| `layouts/`, `static/`    | Shared theme, used by both environments.                       |
| `config/_default/`       | Public build config.                                           |
| `config/development/`    | Development build config (merges over `_default`).             |

Both content trees are mounted onto Hugo's `content` root, so `public/docs/`
becomes `/docs/` and `development/specifications/` becomes `/specifications/`.

## Building

```sh
hugo                  # public only  -> ../docs      (committed, deployed to Pages)
hugo -e development    # public + dev -> ../docs-dev  (git-ignored, local preview)
hugo server -e development   # live preview of everything
```

## Branches

| Branch        | Holds                                   | Purpose                        |
| ------------- | --------------------------------------- | ------------------------------ |
| `development` | full `docs.src/` + built `docs/`        | primary working branch         |
| `public`      | `docs.src/` without `development/`, `config/development/`; built `docs/` | GitHub Pages source |

Write docs on `development`. Promote public-facing changes to `public` by
merging or cherry-picking the `public/`, `layouts/`, `static/`,
`config/_default/`, and `docs/` paths.
