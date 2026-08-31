// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

// LambdaTech Networking echo server for the cross-stack benchmark. Speaks
// the common CLI (--proto/--host/--port); echoes every byte / datagram back
// verbatim; prints "READY <port>" once bound. The orchestrator ends it with
// SIGTERM.
//
//   lnw-bench-echo-server --proto tcp --host 127.0.0.1 --port 7001

#include <csignal>
#include <cstdio>
#include <memory>

#include <lambdatech/networking/core/buffer.hpp>
#include <lambdatech/networking/core/event_loop.hpp>
#include <lambdatech/networking/protocol/tcp/server.hpp>
#include <lambdatech/networking/protocol/udp/peer.hpp>

#include "bench_common.hpp"

namespace core = lambdatech::networking::core;
namespace tcp = lambdatech::networking::protocol::tcp;
namespace udp = lambdatech::networking::protocol::udp;

int main(int argc, char **argv) {
  auto o = bench::parse_options(argc, argv, "lnw");
  std::signal(SIGPIPE, SIG_IGN);

  core::event_loop loop;
  std::shared_ptr<tcp::server> srv;
  std::shared_ptr<udp::peer> pr;

  if (o.proto == "udp") {
    pr = udp::peer::create("udp4", loop);
    auto *raw = pr.get();
    pr->on_message() += [raw](const core::buffer &msg, const core::socket_address &from) {
      raw->send(msg, from.port, from.address);
    };
    pr->on_listening() += [raw] {
      std::printf("READY %u\n", raw->address().port);
      std::fflush(stdout);
    };
    pr->on_error() += [](const std::string &m) { std::fprintf(stderr, "[lnw-echo] %s\n", m.c_str()); };
    loop.start();
    pr->bind(o.port, o.host);
  } else {
    srv = tcp::server::create(loop);
    auto *raw = srv.get();
    srv->on_connect() += [](const std::shared_ptr<tcp::client> &conn) {
      conn->on_data() += [conn](const core::buffer &chunk) { conn->write(chunk); };
    };
    srv->on_listening() += [raw] {
      std::printf("READY %u\n", raw->address().port);
      std::fflush(stdout);
    };
    srv->on_error() += [](const std::string &m) { std::fprintf(stderr, "[lnw-echo] %s\n", m.c_str()); };
    loop.start();
    srv->listen(o.port, o.host);
  }

  pause(); // until SIGTERM
  return 0;
}
