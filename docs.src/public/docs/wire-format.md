---
title: "DNS Wire Format"
weight: 60
---

A quick reference for the RFC 1035 message layout `protocol::dns::wire` reads
and writes. All integers are big-endian.

## Header (12 bytes)

| Offset | Field | Notes |
|---|---|---|
| 0 | ID | 16-bit request identifier |
| 2 | Flags | `QR`(1) `Opcode`(4) `AA`(1) `TC`(1) `RD`(1) `RA`(1) `Z`(3) `RCODE`(4) |
| 4 | QDCOUNT | question count |
| 6 | ANCOUNT | answer RR count |
| 8 | NSCOUNT | authority RR count |
| 10 | ARCOUNT | additional RR count |

`dns::header` stores those flag bits and codes unpacked; `parse()` / `write()`
do the bit-twiddling.

## Question

```
<name>  QTYPE(u16)  QCLASS(u16)
```

## Name encoding

A name is a sequence of labels — a length byte (0–63) then that many octets —
terminated by a zero byte. A length byte with its top two bits set (`0xC0`) is
a **compression pointer**: the low 14 bits are an offset from the start of the
message. `wire::reader::read_name` follows these (with a hard cap against
pointer loops); `wire::writer::write_name` never emits them (SRS-001 M4).

## Record types

`dns::record_type::` exposes `a` (1), `ns` (2), `cname` (5), `soa` (6), `ptr`
(12), `mx` (15), `txt` (16), `aaaa` (28). Unknown values pass through as plain
`uint16_t`.
