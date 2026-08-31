// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

// `lnw-dns-inspect` reads a raw DNS message from stdin (the bytes of a
// single UDP payload) and prints its header and question section, dig-style:
//
//   lnw-dns-inspect < query.bin
//
// Resource-record sections are not decoded yet (SRS-001 M3); the header
// counts are printed so a truncated capture is still obvious.

#include <cstddef>
#include <cstdio>
#include <iostream>
#include <iterator>
#include <vector>

#include <lambdatech/networking/protocol/dns/message.hpp>

namespace dns = lambdatech::networking::protocol::dns;

namespace {

const char *opcode_name(dns::opcode code) {
  switch (code) {
  case dns::opcode::query:
    return "QUERY";
  case dns::opcode::iquery:
    return "IQUERY";
  case dns::opcode::status:
    return "STATUS";
  case dns::opcode::notify:
    return "NOTIFY";
  case dns::opcode::update:
    return "UPDATE";
  }
  return "?";
}

const char *rcode_name(dns::rcode code) {
  switch (code) {
  case dns::rcode::no_error:
    return "NOERROR";
  case dns::rcode::format_error:
    return "FORMERR";
  case dns::rcode::server_failure:
    return "SERVFAIL";
  case dns::rcode::name_error:
    return "NXDOMAIN";
  case dns::rcode::not_implemented:
    return "NOTIMP";
  case dns::rcode::refused:
    return "REFUSED";
  }
  return "?";
}

} // namespace

int main() {
  std::vector<char> raw((std::istreambuf_iterator<char>(std::cin)), std::istreambuf_iterator<char>());
  if (raw.empty()) {
    std::fprintf(stderr, "lnw-dns-inspect: nothing on stdin\n");
    return 1;
  }

  std::vector<std::byte> datagram(raw.size());
  for (std::size_t i = 0; i < raw.size(); ++i) {
    datagram[i] = static_cast<std::byte>(static_cast<unsigned char>(raw[i]));
  }

  auto message = dns::message::parse(datagram);
  if (!message) {
    std::fprintf(stderr, "lnw-dns-inspect: not a well-formed DNS message (%zu bytes)\n", datagram.size());
    return 1;
  }

  const auto &h = message->header;
  std::printf(";; ->>HEADER<<- opcode: %s, status: %s, id: %u\n", opcode_name(h.opcode), rcode_name(h.rcode), h.id);
  std::printf(";; flags:%s%s%s%s%s; QUERY: %u, ANSWER: %u, AUTHORITY: %u, ADDITIONAL: %u\n", h.response ? " qr" : "",
              h.authoritative ? " aa" : "", h.truncated ? " tc" : "", h.recursion_desired ? " rd" : "",
              h.recursion_available ? " ra" : "", h.question_count, h.answer_count, h.authority_count,
              h.additional_count);

  std::printf("\n;; QUESTION SECTION:\n");
  for (const auto &q : message->questions) {
    std::printf(";%-30s\tIN\t%u\n", q.name.c_str(), q.type);
  }

  return 0;
}
