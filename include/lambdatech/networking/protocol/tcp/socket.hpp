// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

// lambdatech::networking::protocol::tcp::client - a non-blocking TCP
// connection modeled on Node.js's net.Socket. Events (subscribe with
// on["name"] += listener):
//
//   connect   the connection is established
//   data      a chunk arrived                (core::buffer)
//   drain     the write buffer emptied
//   end       the peer half-closed (FIN)
//   error     a fatal error                  (std::string)
//   close     the socket is fully closed
//
// Always hold a client through std::shared_ptr (use tcp::client::create or
// take one from tcp::server's 'connection' event): the event loop keeps a
// weak_ptr and every listener runs on the loop thread.

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
#include <sys/types.h>

#include <lambdatech/networking/core/address.hpp>
#include <lambdatech/networking/core/buffer.hpp>
#include <lambdatech/networking/core/event.hpp>
#include <lambdatech/networking/core/event_loop.hpp>
#include <lambdatech/networking/core/native.hpp>
#include <lambdatech/networking/core/thread_pool.hpp>
#include <lambdatech/networking/core/descriptor.hpp>

namespace lambdatech::networking::protocol::tcp {

namespace core = lambdatech::networking::core;
namespace native = lambdatech::networking::core::native;

class server;

class socket : public std::enable_shared_from_this<socket> {
public:
  enum class state { idle, connecting, open, closed };

  static std::shared_ptr<socket> create(core::event_loop &loop = core::event_loop::instance()) {
    return std::shared_ptr<socket>(new socket(loop));
  }

  // ~socket() { native::close_fd(desc); }
  ~socket() { core::descriptor::close(desc); }

  socket(const socket &) = delete;
  socket &operator=(const socket &) = delete;

  // --- events (subscribe with on_<name>() += listener) -------------
  core::event<> &on_connect() { return connect_; }
  core::event<const core::buffer &> &on_data() { return data_; }
  core::event<> &on_drain() { return drain_; }
  core::event<> &on_end() { return end_; }
  core::event<const std::string &> &on_error() { return error_; }
  core::event<> &on_close() { return close_; }

  // --- net.Socket surface -----------------------------------------
  // Resolves `host` off the loop, then connects. 'connect' fires on success.
  void connect(std::uint16_t port, std::string host) {
    auto self = shared_from_this();
    core::thread_pool::instance().submit([self, host = std::move(host), port] {
      auto addrs = core::resolve(host, port, /*datagram=*/false);
      self->loop_.defer([self, addrs] {
        if (addrs.empty()) {
          self->fail("ENOTFOUND: getaddrinfo returned no results");
          return;
        }
        self->start_connect(addrs.front());
      });
    });
  }

  // Queues `chunk` for sending. Returns false when data had to be buffered
  // (wait for 'drain'), true when it went straight to the kernel.
  bool write(std::span<const std::byte> chunk) {
    std::unique_lock lock(mutex_);
    if (state_ != state::open) {
      lock.unlock();
      fail("ERR_STREAM_WRITE_AFTER_END: socket is not open");
      return false;
    }
    bool had_backlog = out_offset_ < outbuf_.size();
    core::append(outbuf_, chunk);
    if (!had_backlog) {
      pump_output(lock); // may leave a remainder buffered
    }
    bool buffered = out_offset_ < outbuf_.size();
    if (buffered) {
      loop_.modify(desc, POLLIN | POLLOUT);
    }
    return !buffered;
  }

  // Half-closes once the write buffer is flushed (net.Socket.end()).
  void end() {
    std::unique_lock lock(mutex_);
    ended_ = true;
    if (state_ == state::open && out_offset_ >= outbuf_.size()) {
      ::shutdown(desc, SHUT_WR);
    }
  }

  // Immediately tears the socket down (net.Socket.destroy()).
  void destroy() { close_with(std::string{}); }

  state connection_state() const {
    std::unique_lock lock(mutex_);
    return state_;
  }
  const core::socket_address &remote_address() const { return peer_; }

private:
  friend class server;

  explicit socket(core::event_loop &loop) : loop_(loop) {}

  // Adopt an already-connected fd (used by tcp::server).
  socket(core::event_loop &loop, int fd, core::socket_address peer)
      : loop_(loop), desc(fd), state_(state::open), peer_(std::move(peer)) {}

  void begin_reading() {
    auto weak = weak_from_this();
    loop_.watch(desc, POLLIN, [weak](short revents) {
      if (auto self = weak.lock()) {
        self->on_io(revents);
      }
    });
  }

  void start_connect(const core::socket_address &addr) {
    int fd = ::socket(addr.family == AF_INET6 ? AF_INET6 : AF_INET, SOCK_STREAM, 0);
    if (fd < 0 || !native::set_nonblocking(fd)) {
      native::close_fd(fd);
      fail(native::last_error("socket"));
      return;
    }

    socklen_t len = 0;
    sockaddr_storage ss = addr.to_sockaddr(len);
    int rc = ::connect(fd, reinterpret_cast<sockaddr *>(&ss), len);
    if (rc != 0 && errno != EINPROGRESS) {
      native::close_fd(fd);
      fail(native::last_error("connect"));
      return;
    }

    {
      std::unique_lock lock(mutex_);
      desc = fd;
      state_ = state::connecting;
      peer_ = addr;
    }

    auto weak = weak_from_this();
    loop_.watch(desc, POLLOUT, [weak](short revents) {
      if (auto self = weak.lock()) {
        self->on_io(revents);
      }
    });
  }

  void on_io(short revents) {
    state current;
    {
      std::unique_lock lock(mutex_);
      current = state_;
    }

    if (current == state::connecting) {
      int err = native::socket_error(desc);
      if (err != 0) {
        fail(std::string("connect: ") + std::strerror(err));
        return;
      }
      {
        std::unique_lock lock(mutex_);
        state_ = state::open;
      }
      loop_.modify(desc, POLLIN);
      connect_.emit();
      std::unique_lock lock(mutex_);
      if (out_offset_ < outbuf_.size()) {
        loop_.modify(desc, POLLIN | POLLOUT);
      }
      return;
    }

    if (current != state::open) {
      return;
    }

    if (revents & POLLOUT) {
      bool drained;
      {
        std::unique_lock lock(mutex_);
        pump_output(lock);
        drained = out_offset_ >= outbuf_.size();
      }
      if (drained) {
        loop_.modify(desc, POLLIN);
        drain_.emit();
        std::unique_lock lock(mutex_);
        if (ended_) {
          ::shutdown(desc, SHUT_WR);
        }
      }
    }

    if (revents & (POLLIN | POLLHUP)) {
      drain_input();
    }
  }

  void drain_input() {
    std::byte tmp[65536];
    while (true) {
      ssize_t got = ::recv(desc, tmp, sizeof(tmp), 0);
      if (got > 0) {
        data_.emit(core::make_buffer(std::span<const std::byte>(tmp, static_cast<std::size_t>(got))));
        continue;
      }
      if (got == 0) {
        end_.emit();
        bool half;
        {
          std::unique_lock lock(mutex_);
          half = ended_;
        }
        if (half) {
          close_with(std::string{});
        } else {
          // default allowHalfOpen=false: close our side too
          close_with(std::string{});
        }
        return;
      }
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return;
      }
      if (errno == EINTR) {
        continue;
      }
      fail(native::last_error("recv"));
      return;
    }
  }

  // Sends as much of outbuf_ as the kernel will take right now. Caller holds
  // `lock` on mutex_.
  void pump_output(std::unique_lock<std::mutex> &lock) {
    while (out_offset_ < outbuf_.size()) {
      ssize_t sent = ::send(desc, outbuf_.data() + out_offset_, outbuf_.size() - out_offset_, MSG_NOSIGNAL);
      if (sent > 0) {
        out_offset_ += static_cast<std::size_t>(sent);
        continue;
      }
      if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        return;
      }
      if (sent < 0 && errno == EINTR) {
        continue;
      }
      lock.unlock();
      fail(native::last_error("send"));
      lock.lock();
      return;
    }
    outbuf_.clear();
    out_offset_ = 0;
  }

  void fail(std::string message) {
    error_.emit(message);
    close_with(std::move(message));
  }

  void close_with(std::string /*reason*/) {
    bool was_live;
    {
      std::unique_lock lock(mutex_);
      was_live = state_ != state::closed && descriptor::valid(desc);
      state_ = state::closed;
    }
    if (!was_live) {
      return;
    }
    loop_.unwatch(desc);
    {
      std::unique_lock lock(mutex_);
      // native::close_fd(descriptor);
      descriptor::close(desc);
    }
    close_.emit();
  }

  core::event_loop &loop_;
  mutable std::mutex mutex_;
  tcp::descriptor::state desc;
  state state_ = state::idle;
  core::socket_address peer_;

  core::buffer outbuf_;
  std::size_t out_offset_ = 0;
  bool ended_ = false;

  core::event<> connect_;
  core::event<const core::buffer &> data_;
  core::event<> drain_;
  core::event<> end_;
  core::event<const std::string &> error_;
  core::event<> close_;
};

} // namespace lambdatech::networking::protocol::tcp
