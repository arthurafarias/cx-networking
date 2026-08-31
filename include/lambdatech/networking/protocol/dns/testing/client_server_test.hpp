// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

#include <future>
#include <optional>

#include <lambdatech/networking/core/event_loop.hpp>
#include <lambdatech/networking/protocol/dns/client.hpp>
#include <lambdatech/networking/protocol/dns/server.hpp>
#include <lambdatech/networking/testing/loop_harness.hpp>
#include <lambdatech/networking/testing/test_group.hpp>

namespace lambdatech::networking::testing {

namespace core = lambdatech::networking::core;
namespace dns = lambdatech::networking::protocol::dns;

struct dns_client_server_test : public test_group {
  dns_client_server_test()
      : test_group(
            "dns::client+server",
            {
                {"the client's query reaches the server and its reply reaches the client",
                 [](test_context &ctx) {
                   core::event_loop loop;

                   auto srv = dns::server::create(loop);
                   srv->on_query() += [](const dns::message &request, const dns::server::responder &reply) {
                     dns::message answer;
                     answer.header.recursion_available = true;
                     answer.header.rcode = dns::rcode::no_error;
                     answer.questions = request.questions; // echo the question section back
                     reply.send(answer);
                   };

                   std::promise<std::optional<dns::message>> answered;
                   auto fut = answered.get_future();
                   std::shared_ptr<dns::client> cli; // lifetime spans the whole case

                   srv->on_listening() += [&] {
                     cli = dns::client::create("127.0.0.1", srv->address().port, loop);
                     cli->set_timeout(std::chrono::milliseconds(800));
                     cli->query("example.com", dns::record_type::a,
                                [&](std::optional<dns::message> m) { answered.set_value(std::move(m)); });
                   };

                   loop.start();
                   srv->listen(0, "127.0.0.1");

                   auto result = await(fut, std::chrono::seconds(3));
                   srv->close();
                   loop.stop();

                   ctx.require(result.has_value(), "await() should not have timed out");
                   ctx.require(result->has_value(), "the client should have received a reply, not a timeout");
                   ctx.check((*result)->header.response, "the reply must have the QR bit set");
                   ctx.check((*result)->header.recursion_available, "RA bit set by the server should survive");
                   ctx.require_equal((*result)->questions.size(), std::size_t{1}, "echoed question count");
                   ctx.check_equal((*result)->questions[0].name, std::string{"example.com"}, "echoed qname");
                 }},
                {"a query with no server answering times out to nullopt",
                 [](test_context &ctx) {
                   core::event_loop loop;
                   loop.start();

                   auto cli = dns::client::create("127.0.0.1", 1, loop); // port 1: nothing listening
                   cli->set_timeout(std::chrono::milliseconds(150));

                   std::promise<std::optional<dns::message>> answered;
                   auto fut = answered.get_future();
                   cli->query("example.com", dns::record_type::a,
                              [&](std::optional<dns::message> m) { answered.set_value(std::move(m)); });

                   auto result = await(fut, std::chrono::seconds(2));
                   loop.stop();

                   ctx.require(result.has_value(), "the callback should have fired");
                   ctx.check(!result->has_value(), "with nobody answering, the result must be a timeout (nullopt)");
                 }},
            }) {}
};

inline static dns_client_server_test dns_client_server_test_instance;

} // namespace lambdatech::networking::testing
