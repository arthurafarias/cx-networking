// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

// A tiny `dig`: sends one A-record query to an upstream resolver over UDP
// and prints the response header. Needs outbound UDP/53.
//
//   g++ -std=c++23 -Iinclude -pthread examples/lnw-example-dns-dig.cpp -o dig
//   ./dig example.com 1.1.1.1

#include <cstdio>
#include <future>
#include <memory>
#include <optional>
#include <string>

#include <lambdatech/networking/core/event_loop.hpp>
#include <lambdatech/networking/protocol/dns/client.hpp>

namespace core = lambdatech::networking::core;
namespace dns = lambdatech::networking::protocol::dns;

int main(int argc, char **argv) {
  const std::string name = argc > 1 ? argv[1] : "example.com";
  const std::string resolver = argc > 2 ? argv[2] : "1.1.1.1";

  core::event_loop loop;
  loop.start();

  auto client = dns::client::create(resolver, 53, loop);
  client->set_timeout(std::chrono::seconds(3));

  std::promise<std::optional<dns::message>> answered;
  auto future = answered.get_future();

  client->on_error() += [](const std::string &message) { std::fprintf(stderr, "error: %s\n", message.c_str()); };
  client->query(name, dns::record_type::a,
                [&](std::optional<dns::message> reply) { answered.set_value(std::move(reply)); });

  auto reply = future.get();
  loop.stop();

  if (!reply) {
    std::fprintf(stderr, ";; no response from %s (timed out)\n", resolver.c_str());
    return 1;
  }

  const auto &h = reply->header;
  std::printf(";; got answer from %s for %s\n", resolver.c_str(), name.c_str());
  std::printf(";; ->>HEADER<<- id: %u, rcode: %u, flags:%s%s; QUERY: %u, ANSWER: %u, AUTHORITY: %u, ADDITIONAL: %u\n",
              h.id, static_cast<unsigned>(h.rcode), h.response ? " qr" : "", h.recursion_available ? " ra" : "",
              h.question_count, h.answer_count, h.authority_count, h.additional_count);
  std::printf(";; (answer-section decoding lands in SRS-001 M3)\n");
  return 0;
}
