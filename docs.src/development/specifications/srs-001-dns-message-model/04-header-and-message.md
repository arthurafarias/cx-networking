---
title: "4. Header & Message"
weight: 40
---

## 4. Header & Message

Implemented in
[protocol/dns/header.hpp](../../../../include/lambdatech/networking/protocol/dns/header.hpp)
and
[protocol/dns/message.hpp](../../../../include/lambdatech/networking/protocol/dns/message.hpp).

### REQ-4.1 — header fields are unpacked

`dns::header` stores `id`, the boolean flags (`response`, `authoritative`,
`truncated`, `recursion_desired`, `recursion_available`), `opcode`, `rcode`,
and the four section counts as ordinary members. `parse(reader)` and
`write(writer)` are the only code that touches the packed flag word;
`wire_size == 12`.

### REQ-4.2 — enums are closed but non-lossy

`dns::opcode` and `dns::rcode` are `enum class : uint8_t` covering the common
values. An unrecognized 4-bit code round-trips as its numeric value, so an
unknown opcode is never undefined behavior.

### REQ-4.3 — message = header + questions

`dns::message` holds a `dns::header` and a `std::vector<dns::question>`.
`dns::question` is `{ std::string name; uint16_t type; uint16_t class_; }`.

### REQ-4.4 — parse is total

`message::parse(std::span<const std::byte>)` returns `std::nullopt` if the
header is short, if `read_name` fails, or if any of the `QDCOUNT` questions
is truncated. It never throws and never reads out of bounds. ANCOUNT /
NSCOUNT / ARCOUNT are parsed into the header counts but the RR bytes
themselves are not consumed (M3).

### REQ-4.5 — serialize refreshes counts

`message::serialize()` writes `question_count` from `questions.size()`,
then the header, then each question. It returns `std::nullopt` only if a
question name has an invalid label.

### REQ-4.6 — content equality

`message::operator==` compares the header (excluding the four derived
section counts) and the question vector, so a freshly built message compares
equal to the same message parsed back from the wire.
