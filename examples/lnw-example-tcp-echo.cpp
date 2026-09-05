// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

// Stands up a TCP echo server and a client on one event loop, sends a line
// through, prints the echo, and shuts down - the net.Server / net.Socket
// surface end to end.
//
//   g++ -std=c++23 -Iinclude -pthread examples/lnw-example-tcp-echo.cpp -o tcp-echo

#include <cstdio>
#include <future>
#include <memory>
#include <string>

#include <lambdatech/networking/core/buffer.hpp>
#include <lambdatech/networking/core/event_loop.hpp>
#include <lambdatech/networking/protocol/tcp/socket.hpp>
#include <lambdatech/networking/protocol/tcp/server.hpp>

namespace core = lambdatech::networking::core;
namespace tcp = lambdatech::networking::protocol::tcp;

int main() {
  core::event_loop loop;

  auto server = tcp::server::create(loop);
  auto client = tcp::socket::create(loop);

  std::promise<std::string> echoed;
  auto future = echoed.get_future();

  server->on_connect() += [](const std::shared_ptr<tcp::socket> conn) {
    conn->on_data() += [conn](const core::buffer &chunk) {
      std::printf("[server] echoing %zu bytes\n", chunk.size());
      conn->write(chunk);
    };
  };

  server->on_listening() += [&] {
    std::printf("[server] listening on 127.0.0.1:%u\n", server->address().port);
    client->connect(server->address().port, "127.0.0.1");
  };

  client->on_connect() += [&] { client->write(core::make_buffer("hello over tcp\n")); };
  client->on_data() += [&](const core::buffer &chunk) { echoed.set_value(core::to_string(chunk)); };
  client->on_error() += [](const std::string &message) { std::fprintf(stderr, "[client] error: %s\n", message.c_str()); };

  loop.start();
  server->listen(0, "127.0.0.1");

  std::string line = future.get();

  std::printf("[client] got back: %s", line.c_str());

  client->destroy();
  server->close();

  loop.stop();
  return 0;
}
