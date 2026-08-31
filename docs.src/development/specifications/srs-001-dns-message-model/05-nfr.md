---
title: "5. Non-Functional Requirements"
weight: 50
---

## 5. Non-Functional Requirements

### NFR-1 — header-only, standard library only

Every type in this SRS is defined in a header under
`include/lambdatech/networking/protocol/dns/`. The only dependency is the
C++23 standard library. No third-party library is introduced.

### NFR-2 — total, panic-free parsing

No parsing path throws, aborts, or reads outside the input span for **any**
byte sequence — empty input, a lone header, a lying `QDCOUNT`, maximal
pointer nesting. Errors are `std::nullopt`.

### NFR-3 — allocation discipline

Parsing allocates only the `std::string` for each name and the
`std::vector<question>` (reserved to `QDCOUNT`). The reader is a span +
offset and copies nothing.

### NFR-4 — portability

No assumption about host endianness — all conversions are explicit shifts.
`std::byte` buffers throughout; no `reinterpret_cast` of integer types over
the wire bytes.
