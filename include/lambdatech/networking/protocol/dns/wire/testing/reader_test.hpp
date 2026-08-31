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

#include <lambdatech/networking/protocol/dns/wire/reader.hpp>
#include <lambdatech/networking/testing/test_group.hpp>

namespace lambdatech::networking::testing {

namespace wire = lambdatech::networking::protocol::dns::wire;

struct dns_wire_reader_test : public test_group {
  dns_wire_reader_test()
      : test_group("dns::wire::reader",
                   {
                       {"reads big-endian integers and advances the cursor",
                        [](test_context &ctx) {
                          std::array<std::byte, 7> bytes{std::byte{0x12}, std::byte{0x34}, std::byte{0xAB},
                                                        std::byte{0xCD}, std::byte{0x00}, std::byte{0x00},
                                                        std::byte{0xFF}};
                          wire::reader reader(bytes);
                          ctx.check_equal(reader.read_u8().value_or(0), std::uint8_t{0x12}, "u8");
                          ctx.check_equal(reader.read_u16().value_or(0), std::uint16_t{0x34AB}, "u16");
                          ctx.check_equal(reader.read_u32().value_or(0), std::uint32_t{0xCD0000FF}, "u32");
                          ctx.check(reader.at_end(), "cursor should be exhausted");
                        }},
                       {"read past the end yields nullopt, not a crash",
                        [](test_context &ctx) {
                          std::array<std::byte, 1> bytes{std::byte{0x01}};
                          wire::reader reader(bytes);
                          ctx.check(!reader.read_u16().has_value(), "u16 on a 1-byte buffer must fail");
                        }},
                       {"decodes a domain name with a compression pointer",
                        [](test_context &ctx) {
                          std::array<std::byte, 19> bytes{
                              std::byte{3},   std::byte{'w'},  std::byte{'w'}, std::byte{'w'}, std::byte{7},
                              std::byte{'e'}, std::byte{'x'},  std::byte{'a'}, std::byte{'m'}, std::byte{'p'},
                              std::byte{'l'}, std::byte{'e'},  std::byte{3},   std::byte{'c'}, std::byte{'o'},
                              std::byte{'m'}, std::byte{0},    std::byte{0xC0}, std::byte{4}};
                          wire::reader reader(bytes);
                          ctx.check_equal(reader.read_name().value_or("?"), std::string{"www.example.com"}, "full name");
                          ctx.check_equal(reader.position(), std::size_t{17}, "cursor past first name");
                          ctx.check_equal(reader.read_name().value_or("?"), std::string{"example.com"}, "pointer name");
                          ctx.check_equal(reader.position(), std::size_t{19}, "cursor past the 2-byte pointer");
                        }},
                   }) {}
};

inline static dns_wire_reader_test dns_wire_reader_test_instance;

} // namespace lambdatech::networking::testing
