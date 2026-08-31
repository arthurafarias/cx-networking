// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <chrono>
#include <future>
#include <poll.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

#include <lambdatech/networking/core/event_loop.hpp>
#include <lambdatech/networking/testing/loop_harness.hpp>
#include <lambdatech/networking/testing/test_group.hpp>

namespace lambdatech::networking::testing {

namespace core = lambdatech::networking::core;

struct event_loop_test : public test_group {
  event_loop_test()
      : test_group("core::event_loop",
                   {
                       {"an fd watch fires its callback when data arrives",
                        [](test_context &ctx) {
                          int sv[2];
                          ctx.require(::socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0, "socketpair");

                          core::event_loop loop;
                          std::promise<char> got;
                          auto fut = got.get_future();

                          loop.watch(sv[0], POLLIN, [&](short) {
                            char c = 0;
                            if (::read(sv[0], &c, 1) == 1) {
                              got.set_value(c);
                            }
                          });
                          loop.start();

                          char ping = 'Z';
                          ctx.check(::write(sv[1], &ping, 1) == 1, "write to the pair");

                          auto received = await(fut);
                          loop.stop();
                          ctx.require(received.has_value(), "the watch callback should have run");
                          ctx.check_equal(*received, 'Z');

                          ::close(sv[0]);
                          ::close(sv[1]);
                        }},
                       {"defer() runs the function on the loop thread",
                        [](test_context &ctx) {
                          core::event_loop loop;
                          loop.start();
                          std::thread::id loop_thread;
                          std::promise<void> done;
                          auto fut = done.get_future();
                          loop.defer([&] {
                            loop_thread = std::this_thread::get_id();
                            done.set_value();
                          });
                          ctx.require(fut.wait_for(std::chrono::seconds(2)) == std::future_status::ready, "defer ran");
                          loop.stop();
                          ctx.check(loop_thread != std::this_thread::get_id(),
                                    "the deferred function must not run on the caller's thread");
                        }},
                       {"set_timeout() fires after roughly the requested delay",
                        [](test_context &ctx) {
                          core::event_loop loop;
                          loop.start();
                          auto start = std::chrono::steady_clock::now();
                          std::promise<void> fired;
                          auto fut = fired.get_future();
                          loop.set_timeout(std::chrono::milliseconds(80), [&] { fired.set_value(); });
                          ctx.require(fut.wait_for(std::chrono::seconds(2)) == std::future_status::ready, "timer fired");
                          auto elapsed = std::chrono::steady_clock::now() - start;
                          loop.stop();
                          ctx.check(elapsed >= std::chrono::milliseconds(70), "the timer should not fire early");
                        }},
                       {"clear_timeout() cancels a pending timer",
                        [](test_context &ctx) {
                          core::event_loop loop;
                          loop.start();
                          std::atomic<bool> fired{false};
                          auto id = loop.set_timeout(std::chrono::milliseconds(40), [&] { fired.store(true); });
                          loop.clear_timeout(id);
                          std::this_thread::sleep_for(std::chrono::milliseconds(120));
                          loop.stop();
                          ctx.check(!fired.load(), "a cleared timer must not fire");
                        }},
                   }) {}
};

inline static event_loop_test event_loop_test_instance;

} // namespace lambdatech::networking::testing
