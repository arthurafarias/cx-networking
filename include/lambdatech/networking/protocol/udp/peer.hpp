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
// All OS access is via core::socket_ops / core::descriptor / core::poller
// (SRS-008).

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <system_error>
#include <utility>

#include <lambdatech/networking/core/address.hpp>
#include <lambdatech/networking/core/buffer.hpp>
#include <lambdatech/networking/core/descriptor.hpp>
#include <lambdatech/networking/core/event.hpp>
#include <lambdatech/networking/core/event_loop.hpp>
#include <lambdatech/networking/core/poller.hpp>
#include <lambdatech/networking/core/resolver.hpp>
#include <lambdatech/networking/core/socket_ops.hpp>

namespace lambdatech::networking::protocol::udp {

namespace core = lambdatech::networking::core;
namespace sock = lambdatech::networking::core::socket_ops;
using core::poller::interest;

class peer : public std::enable_shared_from_this<peer> {
public:
  // family: "udp4" (default) or "udp6", mirroring dgram.createSocket.
  static std::shared_ptr<peer> create(std::string family = "udp4",
                                      core::event_loop &loop = core::event_loop::instance()) {
    return std::shared_ptr<peer>(new peer(family == "udp6" ? sock::domain::inet6 : sock::domain::inet, loop));
  }

  ~peer() { core::descriptor::close(fd_); }

  peer(const peer &) = delete;
  peer &operator=(const peer &) = delete;

  core::event<> &on_listening() { return listening_; }
  core::event<const core::buffer &, const core::socket_address &> &on_message() { return message_; }
  core::event<const std::string &> &on_error() { return error_; }
  core::event<> &on_close() { return close_; }

  // dgram.Socket.bind([port][, address])
  void bind(std::uint16_t port = 0, std::string address = "0.0.0.0") {
    if (family_ == sock::domain::inet6 && address == "0.0.0.0") {
      address = "::";
    }
    if (!open_socket()) {
      return;
    }
    core::socket_address local{address, port, family_ == sock::domain::inet6
                                                  ? static_cast<int>(core::address_family::inet6)
                                                  : static_cast<int>(core::address_family::inet)};
    if (std::errc e = sock::bind(fd_, local); e != std::errc{}) {
      emit_error(sock::describe(e, "bind"));
      return;
    }
    read_back_local();
    begin_reading();
    listening_.emit();
  }

  // Sends one datagram. Resolves `host` synchronously (numeric hosts are the
  // common case for DNS); returns false if the socket isn't usable.
  bool send(std::span<const std::byte> datagram, std::uint16_t port, const std::string &host) {
    if (!core::descriptor::valid(fd_) && !open_socket()) {
      return false;
    }
    auto dest = core::resolver::resolve_one(host, port, /*datagram=*/true);
    if (!dest) {
      emit_error("ENOTFOUND: " + host);
      return false;
    }
    sock::transfer n = sock::send_to(fd_, datagram, *dest);
    if (n.error != std::errc{}) {
      emit_error(sock::describe(n.error, "sendto"));
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
    if (!core::descriptor::valid(fd_)) {
      return;
    }
    loop_.unwatch(fd_);
    core::descriptor::close(fd_);
    reading_ = false;
    close_.emit();
  }

  const core::socket_address &address() const { return local_; }

private:
  peer(sock::domain family, core::event_loop &loop) : loop_(loop), family_(family) {}

  bool open_socket() {
    if (core::descriptor::valid(fd_)) {
      return true;
    }
    auto opened = sock::open(family_, sock::transport::datagram);
    if (!opened) {
      emit_error(sock::describe(opened.error, "socket"));
      return false;
    }
    fd_ = std::move(opened.handle);
    if (std::errc e = sock::set_nonblocking(fd_); e != std::errc{}) {
      emit_error(sock::describe(e, "socket"));
      core::descriptor::close(fd_);
      return false;
    }
    sock::set_reuse_addr(fd_);
    return true;
  }

  void read_back_local() { local_ = sock::local_endpoint(fd_); }

  void begin_reading() {
    if (reading_) {
      return;
    }
    reading_ = true;
    auto weak = weak_from_this();
    loop_.watch(fd_, interest::read, [weak](core::poller::ready) {
      if (auto self = weak.lock()) {
        self->drain();
      }
    });
  }

  void drain() {
    std::byte tmp[65536];
    while (true) {
      sock::received got = sock::recv_from(fd_, std::span<std::byte>(tmp, sizeof(tmp)));
      if (got.count >= 0 && got.error == std::errc{}) {
        message_.emit(core::make_buffer(std::span<const std::byte>(tmp, static_cast<std::size_t>(got.count))),
                      got.from);
        continue;
      }
      if (core::would_block(got.error)) {
        return;
      }
      if (got.error == std::errc::interrupted) {
        continue;
      }
      emit_error(sock::describe(got.error, "recvfrom"));
      return;
    }
  }

  void emit_error(std::string message) { error_.emit(message); }

  core::event_loop &loop_;
  sock::domain family_;
  core::descriptor::state fd_;
  bool reading_ = false;
  core::socket_address local_;

  core::event<> listening_;
  core::event<const core::buffer &, const core::socket_address &> message_;
  core::event<const std::string &> error_;
  core::event<> close_;
};

} // namespace lambdatech::networking::protocol::udp
