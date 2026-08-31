// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <array>
#include <cstddef>

#include <lambdatech/networking/protocol/dns/message.hpp>
#include <lambdatech/networking/testing/test_group.hpp>

namespace lambdatech::networking::testing {

namespace dns = lambdatech::networking::protocol::dns;

struct dns_message_test : public test_group {
  dns_message_test()
      : test_group("dns::message",
                   {
                       {"a query serializes and parses back to an equal value",
                        [](test_context &ctx) {
                          dns::message query;
                          query.header.id = 0x1234;
                          query.header.recursion_desired = true;
                          query.questions.push_back({"www.lambdatech.io", dns::record_type::a, dns::record_class::in});

                          auto wire = query.serialize();
                          ctx.require(wire.has_value(), "serialize() should succeed");

                          auto parsed = dns::message::parse(*wire);
                          ctx.require(parsed.has_value(), "parse() should succeed");
                          ctx.check_equal(parsed->header.id, std::uint16_t{0x1234}, "id");
                          ctx.check(parsed->header.recursion_desired, "RD bit should survive the round trip");
                          ctx.check(!parsed->header.response, "QR should still be a query");
                          ctx.require_equal(parsed->questions.size(), std::size_t{1}, "question count");
                          ctx.check_equal(parsed->questions[0].name, std::string{"www.lambdatech.io"}, "qname");
                          ctx.check(*parsed == query, "the full message should compare equal");
                        }},
                       {"a truncated datagram parses to nullopt",
                        [](test_context &ctx) {
                          std::array<std::byte, 5> runt{std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
                                                       std::byte{0}};
                          ctx.check(!dns::message::parse(runt).has_value(), "a 5-byte datagram cannot be a DNS message");
                        }},
                       {"a header-declared question that is absent fails cleanly",
                        [](test_context &ctx) {
                          dns::message header_only;
                          header_only.header.question_count = 1; // lie: no question bytes follow
                          dns::wire::writer writer;
                          header_only.header.write(writer);
                          ctx.check(!dns::message::parse(writer.bytes()).has_value(),
                                    "a QDCOUNT/body mismatch should not read out of bounds");
                        }},
                   }) {}
};

inline static dns_message_test dns_message_test_instance;

} // namespace lambdatech::networking::testing
