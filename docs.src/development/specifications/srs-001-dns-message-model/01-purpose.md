---
title: "1. Purpose"
weight: 10
---

## 1. Purpose

Every higher-level DNS capability — a stub resolver, a cache, a zone-file
loader, DNSSEC validation — needs one thing first: a correct,
allocation-light, panic-free codec between the RFC 1035 binary wire format
and a usable in-memory structure. Getting that boundary right once, with its
edge cases (big-endian integers, the packed flag byte, length-prefixed
labels, compression pointers, and every possible truncation) covered by
tests, is the prerequisite for all of it.

This SRS specifies that codec and no more: the `dns::wire::reader` /
`dns::wire::writer` primitives, the `dns::header` type, and the
`dns::message` type with its question section. It stops short of the
resource-record sections and any network I/O — those belong to
[SRS-003](../srs-003-protocol-clients-servers/) and a follow-up pass.
