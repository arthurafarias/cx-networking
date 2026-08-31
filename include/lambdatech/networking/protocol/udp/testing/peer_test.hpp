// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <future>
#include <string>

#include <lambdatech/networking/core/buffer.hpp>
#include <lambdatech/networking/core/event_loop.hpp>
#include <lambdatech/networking/protocol/udp/peer.hpp>
#include <lambdatech/networking/testing/loop_harness.hpp>
#include <lambdatech/networking/testing/test_group.hpp>

namespace lambdatech::networking::testing {

namespace core = lambdatech::networking::core;
namespace udp = lambdatech::networking::protocol::udp;

struct udp_peer_test : public test_group {
  udp_peer_test()
      : test_group("udp::peer",
                   {
                       {"a datagram sent to a bound peer arrives with the sender address",
                        [](test_context &ctx) {
                          core::event_loop loop;

                          auto listener = udp::peer::create("udp4", loop);
                          auto sender = udp::peer::create("udp4", loop);

                          std::promise<std::string> received;
                          auto fut = received.get_future();

                          listener->on_message() +=
                              [&](const core::buffer &datagram, const core::socket_address &from) {
                                received.set_value(core::to_string(datagram) + "@" + from.address);
                              };
                          listener->on_listening() += [&] {
                            sender->send(core::make_buffer("ping"), listener->address().port, "127.0.0.1");
                          };

                          loop.start();
                          listener->bind(0, "127.0.0.1");

                          auto result = await(fut);
                          listener->close();
                          sender->close();
                          loop.stop();

                          ctx.require(result.has_value(), "the datagram should have been delivered");
                          ctx.check_equal(*result, std::string{"ping@127.0.0.1"});
                        }},
                   }) {}
};

inline static udp_peer_test udp_peer_test_instance;

} // namespace lambdatech::networking::testing
