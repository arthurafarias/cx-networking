---
title: "6. Milestones"
weight: 60
---

## 6. Milestones

| Milestone | Covers | Status |
|---|---|---|
| **M1** | `wire::reader` / `wire::writer` — integers, byte slices, name decode with pointer following, name encode without compression. Test groups `dns::wire::reader`, `dns::wire::writer`. | Done |
| **M2** | `header` (parse/write, flag packing) and `message` (header + question section, total `parse()`, `serialize()`, content equality). Test group `dns::message`. `lnw-dns-inspect` tool. | Done |
| **M3** | Resource-record sections: an `rr` type, `rdata` representation, a `name` type, and `message::parse` consuming ANCOUNT/NSCOUNT/ARCOUNT records. | Not started |
| **M4** | Name compression on write, so `serialize()` output matches a conformant server. | Not started |
| **M5** | Fuzz target over `message::parse` (libFuzzer harness under `tests/fuzz/`), wired into the coverage build. | Not started |
