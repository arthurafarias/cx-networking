// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

// lambdatech::networking::protocol::udp::peer - a datagram socket modeled
// on Node.js's dgram.Socket. Events (subscribe with on_<name>() += cb):
//
//   listening    bound and ready
//   message      a datagram arrived   (core::buffer, core::socket_address sender)
//   error        a fatal error        (std::string)
//   close        the socket closed
//
// send() takes an explicit destination per call, like dgram.Socket.send().

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <utility>

#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>

#include <lambdatech/networking/core/address.hpp>
#include <lambdatech/networking/core/buffer.hpp>
#include <lambdatech/networking/core/event.hpp>
#include <lambdatech/networking/core/event_loop.hpp>
#include <lambdatech/networking/core/native.hpp>

namespace lambdatech::networking::protocol::udp {

namespace core = lambdatech::networking::core;
namespace native = lambdatech::networking::core::native;

class peer : public std::enable_shared_from_this<peer> {
public:
  // family: "udp4" (default) or "udp6", mirroring dgram.createSocket.
  static std::shared_ptr<peer> create(std::string family = "udp4",
                                      core::event_loop &loop = core::event_loop::instance()) {
    return std::shared_ptr<peer>(new peer(family == "udp6" ? AF_INET6 : AF_INET, loop));
  }

  ~peer() { native::close_fd(fd_); }

  peer(const peer &) = delete;
  peer &operator=(const peer &) = delete;

  core::event<> &on_listening() { return listening_; }
  core::event<const core::buffer &, const core::socket_address &> &on_message() { return message_; }
  core::event<const std::string &> &on_error() { return error_; }
  core::event<> &on_close() { return close_; }

  // dgram.Socket.bind([port][, address])
  void bind(std::uint16_t port = 0, std::string address = "0.0.0.0") {
    if (family_ == AF_INET6 && address == "0.0.0.0") {
      address = "::";
    }
    if (!open_socket()) {
      return;
    }
    core::socket_address local{address, port, family_};
    socklen_t len = 0;
    sockaddr_storage ss = local.to_sockaddr(len);
    if (::bind(fd_, reinterpret_cast<sockaddr *>(&ss), len) != 0) {
      emit_error(native::last_error("bind"));
      return;
    }
    read_back_local();
    begin_reading();
    listening_.emit();
  }

  // Sends one datagram. Resolves `host` synchronously (numeric hosts are the
  // common case for DNS); returns false if the socket isn't usable.
  bool send(std::span<const std::byte> datagram, std::uint16_t port, const std::string &host) {
    if (fd_ < 0 && !open_socket()) {
      return false;
    }
    auto dest = core::resolve_one(host, port, /*datagram=*/true);
    if (!dest) {
      emit_error("ENOTFOUND: " + host);
      return false;
    }
    socklen_t len = 0;
    sockaddr_storage ss = dest->to_sockaddr(len);
    ssize_t n = ::sendto(fd_, datagram.data(), datagram.size(), MSG_NOSIGNAL, reinterpret_cast<sockaddr *>(&ss), len);
    if (n < 0) {
      emit_error(native::last_error("sendto"));
      return false;
    }
    if (!reading_) {
      // send() before bind(): still deliver replies to this ephemeral port.
      read_back_local();
      begin_reading();
    }
    return true;
  }

  void close() {
    if (fd_ < 0) {
      return;
    }
    loop_.unwatch(fd_);
    native::close_fd(fd_);
    reading_ = false;
    close_.emit();
  }

  const core::socket_address &address() const { return local_; }

private:
  peer(int family, core::event_loop &loop) : loop_(loop), family_(family) {}

  bool open_socket() {
    if (fd_ >= 0) {
      return true;
    }
    fd_ = ::socket(family_, SOCK_DGRAM, 0);
    if (fd_ < 0 || !native::set_nonblocking(fd_)) {
      emit_error(native::last_error("socket"));
      native::close_fd(fd_);
      return false;
    }
    int yes = 1;
    ::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    return true;
  }

  void read_back_local() {
    sockaddr_storage ss{};
    socklen_t len = sizeof(ss);
    if (::getsockname(fd_, reinterpret_cast<sockaddr *>(&ss), &len) == 0) {
      local_ = core::socket_address::from_sockaddr(reinterpret_cast<sockaddr *>(&ss));
    }
  }

  void begin_reading() {
    if (reading_) {
      return;
    }
    reading_ = true;
    auto weak = weak_from_this();
    loop_.watch(fd_, POLLIN, [weak](short) {
      if (auto self = weak.lock()) {
        self->drain();
      }
    });
  }

  void drain() {
    std::byte tmp[65536];
    while (true) {
      sockaddr_storage from{};
      socklen_t from_len = sizeof(from);
      ssize_t got = ::recvfrom(fd_, tmp, sizeof(tmp), 0, reinterpret_cast<sockaddr *>(&from), &from_len);
      if (got >= 0) {
        message_.emit(core::make_buffer(std::span<const std::byte>(tmp, static_cast<std::size_t>(got))),
                      core::socket_address::from_sockaddr(reinterpret_cast<sockaddr *>(&from)));
        continue;
      }
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return;
      }
      if (errno == EINTR) {
        continue;
      }
      emit_error(native::last_error("recvfrom"));
      return;
    }
  }

  void emit_error(std::string message) { error_.emit(message); }

  core::event_loop &loop_;
  int family_;
  int fd_ = -1;
  bool reading_ = false;
  core::socket_address local_;

  core::event<> listening_;
  core::event<const core::buffer &, const core::socket_address &> message_;
  core::event<const std::string &> error_;
  core::event<> close_;
};

} // namespace lambdatech::networking::protocol::udp
