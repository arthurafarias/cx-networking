// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

// A parsed DNS message (RFC 1035 §4.1): the fixed header plus the question
// section. Resource-record parsing (answer / authority / additional
// sections) is intentionally left for a follow-up pass - see SRS-001.
// parse() still reads QDCOUNT questions so the cursor and header counts
// stay honest.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <lambdatech/networking/protocol/dns/header.hpp>
#include <lambdatech/networking/protocol/dns/wire/reader.hpp>
#include <lambdatech/networking/protocol/dns/wire/writer.hpp>

namespace lambdatech::networking::protocol::dns {

// RR TYPE / CLASS values (RFC 1035 §3.2.2-§3.2.4), the subset a scaffold
// needs. Plain uint16 constants so unknown values passing through are
// never undefined.
namespace record_type {
inline constexpr std::uint16_t a = 1;
inline constexpr std::uint16_t ns = 2;
inline constexpr std::uint16_t cname = 5;
inline constexpr std::uint16_t soa = 6;
inline constexpr std::uint16_t ptr = 12;
inline constexpr std::uint16_t mx = 15;
inline constexpr std::uint16_t txt = 16;
inline constexpr std::uint16_t aaaa = 28;
} // namespace record_type

namespace record_class {
inline constexpr std::uint16_t in = 1;
} // namespace record_class

struct question {
  std::string name;
  std::uint16_t type = record_type::a;
  std::uint16_t class_ = record_class::in;

  bool operator==(const question &) const = default;
};

struct message {
  dns::header header;
  std::vector<question> questions;

  static std::optional<message> parse(std::span<const std::byte> datagram) {
    wire::reader reader(datagram);

    auto parsed_header = dns::header::parse(reader);
    if (!parsed_header) {
      return std::nullopt;
    }

    message msg;
    msg.header = *parsed_header;
    msg.questions.reserve(msg.header.question_count);

    for (std::uint16_t i = 0; i < msg.header.question_count; ++i) {
      question q;
      auto name = reader.read_name();
      auto type = reader.read_u16();
      auto class_ = reader.read_u16();
      if (!name || !type || !class_) {
        return std::nullopt;
      }
      q.name = std::move(*name);
      q.type = *type;
      q.class_ = *class_;
      msg.questions.push_back(std::move(q));
    }

    return msg;
  }

  std::optional<std::vector<std::byte>> serialize() const {
    wire::writer writer;

    dns::header out_header = header;
    out_header.question_count = static_cast<std::uint16_t>(questions.size());
    out_header.write(writer);

    for (const question &q : questions) {
      if (!writer.write_name(q.name)) {
        return std::nullopt;
      }
      writer.write_u16(q.type);
      writer.write_u16(q.class_);
    }

    return writer.bytes();
  }

  // Two messages are equal when they carry the same content. The header's
  // four section counts are excluded: they are derived from the section
  // vectors on serialize(), so a freshly built message (counts still 0)
  // compares equal to the same message parsed back from the wire.
  bool operator==(const message &other) const {
    dns::header a = header;
    dns::header b = other.header;
    a.question_count = a.answer_count = a.authority_count = a.additional_count = 0;
    b.question_count = b.answer_count = b.authority_count = b.additional_count = 0;
    return a == b && questions == other.questions;
  }
};

} // namespace lambdatech::networking::protocol::dns
