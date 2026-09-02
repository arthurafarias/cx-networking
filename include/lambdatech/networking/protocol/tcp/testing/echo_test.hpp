// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <future>
#include <memory>
#include <string>

#include <lambdatech/networking/core/buffer.hpp>
#include <lambdatech/networking/core/event_loop.hpp>
#include <lambdatech/networking/protocol/tcp/client.hpp>
#include <lambdatech/networking/protocol/tcp/server.hpp>
#include <lambdatech/networking/testing/loop_harness.hpp>
#include <lambdatech/networking/testing/test_group.hpp>

namespace lambdatech::networking::testing {

namespace core = lambdatech::networking::core;
namespace tcp = lambdatech::networking::protocol::tcp;

struct tcp_echo_test : public test_group {
  tcp_echo_test()
      : test_group("tcp::client+server",
                   {
                       {"a client round-trips a payload through an echo server",
                        [](test_context &ctx) {
                          core::event_loop loop;

                          auto srv = tcp::server::create(loop);
                          auto cli = tcp::client::create(loop);

                          std::promise<std::string> echoed;
                          auto fut = echoed.get_future();

                          srv->on_connect() += [](const std::shared_ptr<tcp::client> conn) {
                            conn->on_data() += [conn](const core::buffer &chunk) { conn->write(chunk); };
                          };
                          srv->on_listening() += [&] {
                            cli->connect(srv->address().port, "127.0.0.1");
                          };

                          cli->on_connect() += [&] { cli->write(core::make_buffer("hello-lambdatech")); };
                          cli->on_data() += [&](const core::buffer &chunk) {
                            echoed.set_value(core::to_string(chunk));
                          };

                          loop.start();
                          srv->listen(0, "127.0.0.1");

                          auto result = await(fut);
                          cli->destroy();
                          srv->close();
                          loop.stop();

                          ctx.require(result.has_value(), "the echo should have come back");
                          ctx.check_equal(*result, std::string{"hello-lambdatech"});
                        }},
                       {"the server reports the peer address on 'connect'",
                        [](test_context &ctx) {
                          core::event_loop loop;
                          auto srv = tcp::server::create(loop);
                          auto cli = tcp::client::create(loop);

                          std::promise<std::string> peer;
                          auto fut = peer.get_future();

                          srv->on_connect() += [&](const std::shared_ptr<tcp::client> conn) {
                            peer.set_value(conn->remote_address().address);
                          };
                          srv->on_listening() += [&] { cli->connect(srv->address().port, "127.0.0.1"); };

                          loop.start();
                          srv->listen(0, "127.0.0.1");

                          auto addr = await(fut);
                          cli->destroy();
                          srv->close();
                          loop.stop();

                          ctx.require(addr.has_value(), "connect should have fired");
                          ctx.check_equal(*addr, std::string{"127.0.0.1"});
                        }},
                   }) {}
};

inline static tcp_echo_test tcp_echo_test_instance;

} // namespace lambdatech::networking::testing
