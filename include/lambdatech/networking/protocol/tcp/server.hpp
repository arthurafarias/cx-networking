// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

// lambdatech::networking::protocol::tcp::server - a listening TCP socket
// modeled on Node.js's net.Server. Events (subscribe with on_<name>() += cb):
//
//   listening    the socket is bound and accepting     on_listening()
//   connect      a peer connected  (std::shared_ptr<tcp::socket>)  on_connect()
//   error        accept()/bind() failed  (std::string)             on_error()
//   close        the listener stopped                              on_close()
//
// Each accepted socket is kept alive by the server until it emits 'close',
// so a listener may simply wire up handlers and return.
//
// All OS access is via core::socket_ops / core::descriptor / core::poller
// (SRS-008).

#include <algorithm>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <lambdatech/networking/core/address.hpp>
#include <lambdatech/networking/core/descriptor.hpp>
#include <lambdatech/networking/core/event.hpp>
#include <lambdatech/networking/core/event_loop.hpp>
#include <lambdatech/networking/core/poller.hpp>
#include <lambdatech/networking/core/resolver.hpp>
#include <lambdatech/networking/core/socket_ops.hpp>
#include <lambdatech/networking/protocol/tcp/socket.hpp>

namespace lambdatech::networking::protocol::tcp {

namespace core = lambdatech::networking::core;
namespace sock = lambdatech::networking::core::socket_ops;
using core::poller::interest;

class server : public std::enable_shared_from_this<server> {
public:
  static std::shared_ptr<server> create(core::event_loop &loop = core::event_loop::instance()) {
    return std::shared_ptr<server>(new server(loop));
  }

  ~server() { core::descriptor::close(fd_); }

  server(const server &) = delete;
  server &operator=(const server &) = delete;

  // --- events (subscribe with on_<name>() += listener) -------------
  core::event<> &on_listening() { return listening_; }
  core::event<std::shared_ptr<socket>> &on_connect() { return connection_; }
  core::event<const std::string &> &on_error() { return error_; }
  core::event<> &on_close() { return close_; }

  // net.Server.listen(port[, host])
  void listen(std::uint16_t port, std::string host = "0.0.0.0") {
    auto addr = core::resolver::resolve_one(host, port, /*datagram=*/false);
    if (!addr) {
      error_.emit("EADDRNOTAVAIL: could not resolve " + host);
      return;
    }

    auto opened = sock::open(addr->is_inet6() ? sock::domain::inet6 : sock::domain::inet, sock::transport::stream);
    if (!opened) {
      error_.emit(sock::describe(opened.error, "socket"));
      return;
    }
    core::descriptor::state fd = std::move(opened.handle);
    sock::set_reuse_addr(fd);

    if (std::errc e = sock::bind(fd, *addr); e != std::errc{}) {
      error_.emit(sock::describe(e, "bind"));
      return;
    }
    if (std::errc e = sock::listen(fd, 1024); e != std::errc{}) {
      error_.emit(sock::describe(e, "listen"));
      return;
    }
    if (std::errc e = sock::set_nonblocking(fd); e != std::errc{}) {
      error_.emit(sock::describe(e, "listen"));
      return;
    }

    // Read back the actual bound address (port 0 -> an ephemeral port).
    bound_ = sock::local_endpoint(fd);

    fd_ = std::move(fd);
    auto weak = weak_from_this();
    loop_.watch(fd_, interest::read, [weak](core::poller::ready) {
      if (auto self = weak.lock()) {
        self->accept_ready();
      }
    });
    listening_.emit();
  }

  void close() {
    if (!core::descriptor::valid(fd_)) {
      return;
    }
    loop_.unwatch(fd_);
    core::descriptor::close(fd_);
    close_.emit();
  }

  const core::socket_address &address() const { return bound_; }

private:
  explicit server(core::event_loop &loop) : loop_(loop) {}

  void accept_ready() {
    while (true) {
      sock::accepted a = sock::accept(fd_);
      if (a.error != std::errc{}) {
        if (core::would_block(a.error)) {
          return;
        }
        if (a.error == std::errc::interrupted) {
          continue;
        }
        error_.emit(sock::describe(a.error, "accept"));
        return;
      }

      if (sock::set_nonblocking(a.handle) != std::errc{}) {
        continue; // a.handle closes on scope exit
      }

      auto conn = std::shared_ptr<socket>(new socket(loop_, std::move(a.handle), a.peer));
      conn->begin_reading();

      {
        std::unique_lock lock(mutex_);
        conns_.push_back(conn);
      }
      std::weak_ptr<server> weak = weak_from_this();
      std::weak_ptr<socket> weak_conn = conn;
      conn->on_close() += [weak, weak_conn] {
        if (auto self = weak.lock()) {
          self->drop(weak_conn.lock());
        }
      };

      connection_.emit(conn);
    }
  }

  void drop(const std::shared_ptr<socket> conn) {
    std::unique_lock lock(mutex_);
    std::erase(conns_, conn);
  }

  core::event_loop &loop_;
  std::mutex mutex_;
  core::descriptor::state fd_;
  core::socket_address bound_;
  std::vector<std::shared_ptr<socket>> conns_;

  core::event<> listening_;
  core::event<std::shared_ptr<socket>> connection_;
  core::event<const std::string &> error_;
  core::event<> close_;
};

} // namespace lambdatech::networking::protocol::tcp
