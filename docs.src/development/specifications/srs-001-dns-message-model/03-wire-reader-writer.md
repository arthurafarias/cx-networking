---
title: "3. Wire Reader & Writer"
weight: 30
---

## 3. Wire Reader & Writer

Implemented in
[protocol/dns/wire/reader.hpp](../../../../include/lambdatech/networking/protocol/dns/wire/reader.hpp)
and
[protocol/dns/wire/writer.hpp](../../../../include/lambdatech/networking/protocol/dns/wire/writer.hpp).

### REQ-3.1 — reader is bounds-checked and total

`wire::reader` wraps a `std::span<const std::byte>` and a cursor. Every
`read_*` checks `remaining()` first and returns `std::optional`; none throws
or reads past the span. `seek()` allows repositioning.

### REQ-3.2 — big-endian integers

`read_u16` / `read_u32` (and the writer's `write_u16` / `write_u32`)
serialize most-significant byte first, per RFC 1035 §2.3.2.

### REQ-3.3 — name decoding follows pointers

`reader::read_name` walks labels until a zero octet. On a `0xC0` pointer it
jumps to the target offset and continues; the cursor is left just past the
**first** pointer (or past the terminating zero if uncompressed). A pointer
loop is bounded by a hard iteration cap and yields `std::nullopt`. Reserved
label types are rejected.

### REQ-3.4 — writer emits uncompressed names

`writer::write_name` splits a dotted string on `.`, emits each label
length-prefixed, and terminates with a zero octet. An empty label, or one
longer than 63 bytes, returns `false`. Compression on write is M4.

### REQ-3.5 — round-trip

For any value written by the writer, reading it back with a `reader` over
`writer.bytes()` yields an equal value. Covered by the `dns::wire::reader`
and `dns::wire::writer` test groups.
