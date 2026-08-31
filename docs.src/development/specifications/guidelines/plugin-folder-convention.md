---
title: "Plugin Folder Convention"
weight: 2
---

## Plugin Folder Convention

**Any code that requires an external, third-party library to build lives
under `plugins/<name>/`, never under `include/lambdatech/networking/`.** This
is the boundary that keeps the core runtime and every protocol header-only,
dependency-free (beyond the STL and pthreads), and buildable everywhere;
`plugins/` is where an `io_uring` binding, OpenSSL for DNS-over-TLS, or
`c-ares`/`libunbound` interop is explicitly, visibly opted into by name — one
plugin, one library, one directory, each independently optional.

This guideline is pinned independently of any individual plugin SRS's own
status.

### 1. Directory layout

```
plugins/
  CMakeLists.txt                 # add_subdirectory() for every plugin below
  <name>/
    CMakeLists.txt               # this plugin's own dependency detection + targets
    include/lambdatech/networking/plugins/<name>/
      <name>.hpp
      testing/<name>_test.hpp
```

### 2. CMake contract

Each `plugins/<name>/CMakeLists.txt`:

- does its own `find_package()` / `pkg_check_modules()` and defines
  `option(LAMBDATECH_NETWORKING_PLUGIN_<NAME> ... ${<DEP>_FOUND})`;
- produces **no targets** if the dependency is absent, so the top-level
  `add_subdirectory(plugins)` stays on by default;
- exposes its build as a target named
  `lambdatech-networking-plugin-<name>` that links `lambdatech-networking`
  plus its own dependency;
- generates its own `lambdatech-networking-plugin-<name>-tests` binary the
  same way the root does, gated on the dependency being present.

No plugins exist yet.
