---
title: "2. Scope"
weight: 20
---

## 2. Scope

### In scope

- A forward-only, bounds-checked wire cursor (`dns::wire::reader`) with
  big-endian `u8`/`u16`/`u32` reads, raw byte slices, and domain-name
  decoding that follows compression pointers.
- A matching `dns::wire::writer` that appends the same field types and
  encodes uncompressed domain names.
- The fixed 12-byte `dns::header` (RFC 1035 §4.1.1) with unpacked flag
  fields and `parse()` / `write()`.
- `dns::message` = `dns::header` + `std::vector<dns::question>`, with
  `message::parse(span)` and `message::serialize()`, both total (no throw,
  no out-of-bounds read) on arbitrary input.

### Out of scope (this SRS)

- The answer / authority / additional resource-record sections — M3.
- Name **compression on write** — M4.
- EDNS(0), TSIG, DNS Cookies, OPT-record handling.
- Network transport — that is [SRS-003](../srs-003-protocol-clients-servers/).
- Caching, DNSSEC.
