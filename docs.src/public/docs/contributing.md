---
title: "Contributing"
weight: 70
---

We welcome issues and pull requests. Suggestions for optimizations, new
protocols, or feature ideas are especially appreciated!

## Running the tests

The self-hosted test facility
(`include/lambdatech/networking/**/testing/*_test.hpp`, see
[Core Concepts]({{< relref "core-concepts.md" >}})) builds into one binary:

```bash
cmake -S . -B build
cmake --build build --target lambdatech-networking-tests
build/lambdatech-networking-tests
```

The socket test groups stand real listeners up on `127.0.0.1` ephemeral
ports and drive them through the event loop, each guarded by a hard timeout
so a broken path fails the case instead of hanging the run.

## Coverage

A line-coverage report is generated with [gcovr](https://gcovr.com/) (GCC or
Clang only) and published alongside this site at
[/lambdatech-networking/coverage/](/lambdatech-networking/coverage/). To regenerate it
locally:

```bash
cmake -S . -B build -DLAMBDATECH_NETWORKING_ENABLE_COVERAGE=ON
cmake --build build --target coverage
```

The report scopes to `include/lambdatech/networking/` and excludes the
`testing/` subdirectories.
