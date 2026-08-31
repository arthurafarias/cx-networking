// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

// Boost.Asio echo server - the C++ reference point for the benchmark. One
// io_context on one thread, to match LambdaTech Networking's single-reactor
// model. Speaks the common CLI; prints "READY <port>" once bound.

#include <array>
#include <csignal>
#include <cstdio>
#include <memory>

#include <boost/asio.hpp>

#include "bench_common.hpp"

namespace asio = boost::asio;
using tcp = asio::ip::tcp;
using udp = asio::ip::udp;

namespace {

class tcp_session : public std::enable_shared_from_this<tcp_session> {
public:
  explicit tcp_session(tcp::socket sock) : sock_(std::move(sock)) {}

  void start() { read(); }

private:
  void read() {
    auto self = shared_from_this();
    sock_.async_read_some(asio::buffer(buf_), [this, self](boost::system::error_code ec, std::size_t n) {
      if (ec) {
        return;
      }
      asio::async_write(sock_, asio::buffer(buf_, n),
                        [this, self](boost::system::error_code wec, std::size_t) {
                          if (!wec) {
                            read();
                          }
                        });
    });
  }

  tcp::socket sock_;
  std::array<std::byte, 65536> buf_{};
};

class tcp_echo {
public:
  tcp_echo(asio::io_context &io, const tcp::endpoint &ep) : acceptor_(io, ep) {
    std::printf("READY %u\n", acceptor_.local_endpoint().port());
    std::fflush(stdout);
    accept();
  }

private:
  void accept() {
    acceptor_.async_accept([this](boost::system::error_code ec, tcp::socket sock) {
      if (!ec) {
        sock.set_option(tcp::no_delay(true));
        std::make_shared<tcp_session>(std::move(sock))->start();
      }
      accept();
    });
  }

  tcp::acceptor acceptor_;
};

class udp_echo {
public:
  udp_echo(asio::io_context &io, const udp::endpoint &ep) : sock_(io, ep) {
    std::printf("READY %u\n", sock_.local_endpoint().port());
    std::fflush(stdout);
    receive();
  }

private:
  void receive() {
    sock_.async_receive_from(asio::buffer(buf_), peer_, [this](boost::system::error_code ec, std::size_t n) {
      if (!ec) {
        sock_.async_send_to(asio::buffer(buf_, n), peer_,
                            [](boost::system::error_code, std::size_t) {});
      }
      receive();
    });
  }

  udp::socket sock_;
  udp::endpoint peer_;
  std::array<std::byte, 65536> buf_{};
};

} // namespace

int main(int argc, char **argv) {
  auto o = bench::parse_options(argc, argv, "asio");
  std::signal(SIGPIPE, SIG_IGN);

  asio::io_context io;
  auto addr = asio::ip::make_address(o.host);

  std::unique_ptr<tcp_echo> t;
  std::unique_ptr<udp_echo> u;
  if (o.proto == "udp") {
    u = std::make_unique<udp_echo>(io, udp::endpoint(addr, o.port));
  } else {
    t = std::make_unique<tcp_echo>(io, tcp::endpoint(addr, o.port));
  }

  io.run();
  return 0;
}
