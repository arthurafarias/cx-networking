// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

// The fixed 12-byte DNS message header (RFC 1035 §4.1.1). The second and
// third bytes pack a set of flag bits and two 4-bit codes; this type
// exposes them as ordinary fields and handles the bit-twiddling in
// parse()/write().

#include <cstddef>
#include <cstdint>
#include <optional>

#include <lambdatech/networking/protocol/dns/wire/reader.hpp>
#include <lambdatech/networking/protocol/dns/wire/writer.hpp>

namespace lambdatech::networking::protocol::dns {

enum class opcode : std::uint8_t { query = 0, iquery = 1, status = 2, notify = 4, update = 5 };

enum class rcode : std::uint8_t {
  no_error = 0,
  format_error = 1,
  server_failure = 2,
  name_error = 3,
  not_implemented = 4,
  refused = 5,
};

struct header {
  std::uint16_t id = 0;

  bool response = false; // QR
  dns::opcode opcode = dns::opcode::query;
  bool authoritative = false;       // AA
  bool truncated = false;           // TC
  bool recursion_desired = false;   // RD
  bool recursion_available = false; // RA
  dns::rcode rcode = dns::rcode::no_error;

  std::uint16_t question_count = 0;   // QDCOUNT
  std::uint16_t answer_count = 0;     // ANCOUNT
  std::uint16_t authority_count = 0;  // NSCOUNT
  std::uint16_t additional_count = 0; // ARCOUNT

  static constexpr std::size_t wire_size = 12;

  static std::optional<header> parse(wire::reader &reader) {
    header h;
    auto id = reader.read_u16();
    auto flags = reader.read_u16();
    auto qd = reader.read_u16();
    auto an = reader.read_u16();
    auto ns = reader.read_u16();
    auto ar = reader.read_u16();
    if (!id || !flags || !qd || !an || !ns || !ar) {
      return std::nullopt;
    }

    h.id = *id;
    h.response = (*flags >> 15) & 0x1;
    h.opcode = static_cast<dns::opcode>((*flags >> 11) & 0xF);
    h.authoritative = (*flags >> 10) & 0x1;
    h.truncated = (*flags >> 9) & 0x1;
    h.recursion_desired = (*flags >> 8) & 0x1;
    h.recursion_available = (*flags >> 7) & 0x1;
    h.rcode = static_cast<dns::rcode>(*flags & 0xF);

    h.question_count = *qd;
    h.answer_count = *an;
    h.authority_count = *ns;
    h.additional_count = *ar;
    return h;
  }

  void write(wire::writer &writer) const {
    std::uint16_t flags = 0;
    flags |= static_cast<std::uint16_t>(response) << 15;
    flags |= (static_cast<std::uint16_t>(opcode) & 0xF) << 11;
    flags |= static_cast<std::uint16_t>(authoritative) << 10;
    flags |= static_cast<std::uint16_t>(truncated) << 9;
    flags |= static_cast<std::uint16_t>(recursion_desired) << 8;
    flags |= static_cast<std::uint16_t>(recursion_available) << 7;
    flags |= static_cast<std::uint16_t>(rcode) & 0xF;

    writer.write_u16(id);
    writer.write_u16(flags);
    writer.write_u16(question_count);
    writer.write_u16(answer_count);
    writer.write_u16(authority_count);
    writer.write_u16(additional_count);
  }

  bool operator==(const header &) const = default;
};

} // namespace lambdatech::networking::protocol::dns
