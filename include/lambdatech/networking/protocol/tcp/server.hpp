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
//   connect      a peer connected  (std::shared_ptr<tcp::client>)  on_connect()
//   error        accept()/bind() failed  (std::string)             on_error()
//   close        the listener stopped                              on_close()
//
// Each accepted client is kept alive by the server until it emits 'close',
// so a listener may simply wire up handlers and return.

#include <algorithm>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>

#include <lambdatech/networking/core/address.hpp>
#include <lambdatech/networking/core/event.hpp>
#include <lambdatech/networking/core/event_loop.hpp>
#include <lambdatech/networking/core/native.hpp>
#include <lambdatech/networking/protocol/tcp/client.hpp>

namespace lambdatech::networking::protocol::tcp {

namespace core = lambdatech::networking::core;
namespace native = lambdatech::networking::core::native;

class server : public std::enable_shared_from_this<server> {
public:
  static std::shared_ptr<server> create(core::event_loop &loop = core::event_loop::instance()) {
    return std::shared_ptr<server>(new server(loop));
  }

  ~server() { native::close_fd(fd_); }

  server(const server &) = delete;
  server &operator=(const server &) = delete;

  // --- events (subscribe with on_<name>() += listener) -------------
  core::event<> &on_listening() { return listening_; }
  core::event<const std::shared_ptr<client> &> &on_connect() { return connection_; }
  core::event<const std::string &> &on_error() { return error_; }
  core::event<> &on_close() { return close_; }

  // net.Server.listen(port[, host])
  void listen(std::uint16_t port, std::string host = "0.0.0.0") {
    auto addr = core::resolve_one(host, port, /*datagram=*/false);
    if (!addr) {
      error_.emit("EADDRNOTAVAIL: could not resolve " + host);
      return;
    }

    int fd = ::socket(addr->family == AF_INET6 ? AF_INET6 : AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
      error_.emit(native::last_error("socket"));
      return;
    }
    int yes = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    socklen_t len = 0;
    sockaddr_storage ss = addr->to_sockaddr(len);
    if (::bind(fd, reinterpret_cast<sockaddr *>(&ss), len) != 0 || ::listen(fd, SOMAXCONN) != 0 ||
        !native::set_nonblocking(fd)) {
      error_.emit(native::last_error("listen"));
      native::close_fd(fd);
      return;
    }

    // Read back the actual bound address (port 0 -> an ephemeral port).
    sockaddr_storage bound{};
    socklen_t bound_len = sizeof(bound);
    if (::getsockname(fd, reinterpret_cast<sockaddr *>(&bound), &bound_len) == 0) {
      bound_ = core::socket_address::from_sockaddr(reinterpret_cast<sockaddr *>(&bound));
    }

    fd_ = fd;
    auto weak = weak_from_this();
    loop_.watch(fd_, POLLIN, [weak](short) {
      if (auto self = weak.lock()) {
        self->accept_ready();
      }
    });
    listening_.emit();
  }

  void close() {
    if (fd_ < 0) {
      return;
    }
    loop_.unwatch(fd_);
    native::close_fd(fd_);
    close_.emit();
  }

  const core::socket_address &address() const { return bound_; }

private:
  explicit server(core::event_loop &loop) : loop_(loop) {}

  void accept_ready() {
    while (true) {
      sockaddr_storage peer{};
      socklen_t peer_len = sizeof(peer);
      int cfd = ::accept(fd_, reinterpret_cast<sockaddr *>(&peer), &peer_len);
      if (cfd < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          return;
        }
        if (errno == EINTR) {
          continue;
        }
        error_.emit(native::last_error("accept"));
        return;
      }

      if (!native::set_nonblocking(cfd)) {
        native::close_fd(cfd);
        continue;
      }

      auto conn = std::shared_ptr<client>(
          new client(loop_, cfd, core::socket_address::from_sockaddr(reinterpret_cast<sockaddr *>(&peer))));
      conn->begin_reading();

      {
        std::unique_lock lock(mutex_);
        conns_.push_back(conn);
      }
      std::weak_ptr<server> weak = weak_from_this();
      std::weak_ptr<client> weak_conn = conn;
      conn->on_close() += [weak, weak_conn] {
        if (auto self = weak.lock()) {
          self->drop(weak_conn.lock());
        }
      };

      connection_.emit(conn);
    }
  }

  void drop(const std::shared_ptr<client> &conn) {
    std::unique_lock lock(mutex_);
    std::erase(conns_, conn);
  }

  core::event_loop &loop_;
  std::mutex mutex_;
  int fd_ = -1;
  core::socket_address bound_;
  std::vector<std::shared_ptr<client>> conns_;

  core::event<> listening_;
  core::event<const std::shared_ptr<client> &> connection_;
  core::event<const std::string &> error_;
  core::event<> close_;
};

} // namespace lambdatech::networking::protocol::tcp
