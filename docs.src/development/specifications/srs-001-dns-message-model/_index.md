---
title: "SRS-001: DNS Message Model"
weight: 1
---

**Status:** Partially implemented — the wire reader/writer, the 12-byte
header, and the `message` type with its question section are implemented and
tested (M1–M2). Resource-record parsing (M3) and name compression on write
(M4) are specified here but not started.

**Author:** Arthur de Araújo Farias
**Date:** 2026-08-31

The in-memory representation of a DNS message (RFC 1035 §4) and the binary
codec that reads and writes it: a bounds-checked wire cursor, the fixed
header with its packed flag bits, and a `message` type that round-trips a
datagram through `parse()` / `serialize()`. Lives at
`include/lambdatech/networking/protocol/dns/`.

## Sections

| # | Section |
|---|---|
| 1 | [Purpose](01-purpose) |
| 2 | [Scope](02-scope) |
| 3 | [Wire Reader & Writer](03-wire-reader-writer) |
| 4 | [Header & Message](04-header-and-message) |
| 5 | [Non-Functional Requirements](05-nfr) |
| 6 | [Milestones](06-milestones) |
