---
title: "Testing Convention"
weight: 1
---

## Testing Convention

**Every module ships its unit tests as a colocated header at
`<module-path>/testing/<name>_test.hpp` that declares one self-registering
`test_group`. No external test framework (GoogleTest, Catch2, doctest, …) is
added to the project.**

The test facility is itself header-only C++23, living under
[`include/lambdatech/networking/testing/`](../../../../include/lambdatech/networking/testing).

### 1. Layout

```
include/lambdatech/networking/
  core/
    event_loop.hpp
    testing/event_loop_test.hpp
  protocol/
    tcp/
      client.hpp  server.hpp
      testing/echo_test.hpp
    udp/
      peer.hpp
      testing/peer_test.hpp
    dns/
      message.hpp  client.hpp  server.hpp
      wire/reader.hpp  wire/writer.hpp
      testing/message_test.hpp  testing/client_server_test.hpp
      wire/testing/reader_test.hpp  wire/testing/writer_test.hpp
```

### 2. A test header

```c++
#pragma once
#include <lambdatech/networking/testing/test_group.hpp>
#include <lambdatech/networking/core/event_loop.hpp>

namespace lambdatech::networking::testing {

struct event_loop_test : public test_group {
  event_loop_test() : test_group("core::event_loop", {
    {"defer() runs on the loop thread", [](test_context &ctx) {
      // ctx.check(...), ctx.check_equal(...), ctx.require(...)
    }},
  }) {}
};

inline static event_loop_test event_loop_test_instance;

} // namespace lambdatech::networking::testing
```

### 3. Socket tests

Groups that exercise real sockets stand a listener up on a `127.0.0.1`
ephemeral port, drive it through a local `core::event_loop`, and block the
test thread on a `std::future` fed from a listener — always via
`testing::await(future, timeout)` so a broken path fails the case instead of
hanging the whole run.

### 4. Build wiring

The root `CMakeLists.txt` globs `include/*_test.hpp` with `CONFIGURE_DEPENDS`,
writes one generated `all_tests.cpp` of `#include`s, and compiles it plus
`tests/main.cpp` into `lambdatech-networking-tests`. Adding a new `*_test.hpp`
triggers a reconfigure on the next build.
