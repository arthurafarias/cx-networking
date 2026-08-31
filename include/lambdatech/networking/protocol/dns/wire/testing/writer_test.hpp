// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <cstddef>
#include <string>

#include <lambdatech/networking/protocol/dns/wire/reader.hpp>
#include <lambdatech/networking/protocol/dns/wire/writer.hpp>
#include <lambdatech/networking/testing/test_group.hpp>

namespace lambdatech::networking::testing {

namespace wire = lambdatech::networking::protocol::dns::wire;

struct dns_wire_writer_test : public test_group {
  dns_wire_writer_test()
      : test_group("dns::wire::writer",
                   {
                       {"integers round-trip through reader",
                        [](test_context &ctx) {
                          wire::writer writer;
                          writer.write_u8(0x2A);
                          writer.write_u16(0xBEEF);
                          writer.write_u32(0xDEADC0DE);
                          ctx.require_equal(writer.size(), std::size_t{7}, "encoded size");

                          wire::reader reader(writer.bytes());
                          ctx.check_equal(reader.read_u8().value_or(0), std::uint8_t{0x2A}, "u8");
                          ctx.check_equal(reader.read_u16().value_or(0), std::uint16_t{0xBEEF}, "u16");
                          ctx.check_equal(reader.read_u32().value_or(0), std::uint32_t{0xDEADC0DE}, "u32");
                        }},
                       {"a name round-trips through reader",
                        [](test_context &ctx) {
                          wire::writer writer;
                          ctx.require(writer.write_name("mail.lambdatech.io"), "write_name should accept a valid name");
                          wire::reader reader(writer.bytes());
                          ctx.check_equal(reader.read_name().value_or("?"), std::string{"mail.lambdatech.io"}, "name");
                        }},
                       {"an over-long label is rejected",
                        [](test_context &ctx) {
                          wire::writer writer;
                          ctx.check(!writer.write_name(std::string(64, 'a')), "a 64-byte label must be rejected");
                        }},
                   }) {}
};

inline static dns_wire_writer_test dns_wire_writer_test_instance;

} // namespace lambdatech::networking::testing
