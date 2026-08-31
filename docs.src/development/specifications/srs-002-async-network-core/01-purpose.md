---
title: "1. Purpose"
weight: 10
---

## 1. Purpose

The protocol layer (SRS-003) needs an asynchronous runtime with three
properties:

1. **One thread runs all callbacks.** Node.js's model — a single event loop —
   is what makes socket code tractable: no listener ever races another, and
   shared state needs no locking.
2. **Blocking work has somewhere to go.** `getaddrinfo` and friends must not
   stall the loop; there must be a worker pool and a way to hand a result
   back to the loop thread.
3. **It is already written and tested.** The [cxflow](https://github.com/arthurafarias/cxflow)
   library drives its dataflow pipelines with exactly this: a dedicated-thread
   loop runner (`task`), a strict-priority worker pool (`thread_pool`), and a
   connect/emit `signal`. Rather than reinvent them, this SRS **vendors them
   verbatim** into `lambdatech::networking::core` and builds the reactor and
   the event primitive on top.

This SRS specifies that runtime: what is vendored, the `core::event` wrapper,
and the `core::event_loop` reactor.
