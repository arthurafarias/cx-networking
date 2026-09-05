// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

// lambdatech::networking::protocol::tcp::socket - a non-blocking TCP
// connection modeled on Node.js's net.Socket. Events (subscribe with
// on_<name>() += listener):
//
//   connect   the connection is established
//   data      a chunk arrived                (core::buffer)
//   drain     the write buffer emptied
//   end       the peer half-closed (FIN)
//   error     a fatal error                  (std::string)
//   close     the socket is fully closed
//
// Always hold a socket through std::shared_ptr (use tcp::socket::create or
// take one from tcp::server's 'connect' event): the event loop keeps a
// weak_ptr and every listener runs on the loop thread.
//
// All OS access is via core::socket_ops / core::descriptor / core::poller
// (SRS-008): this header names no sockaddr, no errno, no poll flags.

#include <cstddef>
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
#include <lambdatech/networking/core/thread_pool.hpp>

namespace lambdatech::networking::protocol::tcp {

namespace core = lambdatech::networking::core;
namespace sock = lambdatech::networking::core::socket_ops;
using core::poller::interest;

class server;

class socket : public std::enable_shared_from_this<socket> {
public:
  enum class state { idle, connecting, open, closed };

  static std::shared_ptr<socket> create(core::event_loop &loop = core::event_loop::instance()) {
    return std::shared_ptr<socket>(new socket(loop));
  }

  ~socket() { core::descriptor::close(desc_); }

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
      auto addrs = core::resolver::resolve(host, port, /*datagram=*/false);
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
      loop_.modify(desc_, interest::read | interest::write);
    }
    return !buffered;
  }

  // Half-closes once the write buffer is flushed (net.Socket.end()).
  void end() {
    std::unique_lock lock(mutex_);
    ended_ = true;
    if (state_ == state::open && out_offset_ >= outbuf_.size()) {
      sock::shutdown(desc_, sock::shut::write);
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

  // Adopt an already-connected descriptor (used by tcp::server).
  socket(core::event_loop &loop, core::descriptor::state d, core::socket_address peer)
      : loop_(loop), desc_(std::move(d)), state_(state::open), peer_(std::move(peer)) {}

  void begin_reading() {
    auto weak = weak_from_this();
    loop_.watch(desc_, interest::read, [weak](core::poller::ready r) {
      if (auto self = weak.lock()) {
        self->on_io(r);
      }
    });
  }

  void start_connect(const core::socket_address &addr) {
    auto opened = sock::open(addr.is_inet6() ? sock::domain::inet6 : sock::domain::inet, sock::transport::stream);
    if (!opened) {
      fail(sock::describe(opened.error, "socket"));
      return;
    }
    core::descriptor::state d = std::move(opened.handle);
    if (std::errc nb = sock::set_nonblocking(d); nb != std::errc{}) {
      fail(sock::describe(nb, "socket"));
      return;
    }

    std::errc ec = sock::connect(d, addr);
    if (ec != std::errc{} && ec != std::errc::operation_in_progress) {
      fail(sock::describe(ec, "connect"));
      return;
    }

    {
      std::unique_lock lock(mutex_);
      desc_ = std::move(d);
      state_ = state::connecting;
      peer_ = addr;
    }

    auto weak = weak_from_this();
    loop_.watch(desc_, interest::write, [weak](core::poller::ready r) {
      if (auto self = weak.lock()) {
        self->on_io(r);
      }
    });
  }

  void on_io(core::poller::ready r) {
    state current;
    {
      std::unique_lock lock(mutex_);
      current = state_;
    }

    if (current == state::connecting) {
      std::errc err = sock::pending_error(desc_);
      if (err != std::errc{}) {
        fail(sock::describe(err, "connect"));
        return;
      }
      {
        std::unique_lock lock(mutex_);
        state_ = state::open;
      }
      loop_.modify(desc_, interest::read);
      connect_.emit();
      std::unique_lock lock(mutex_);
      if (out_offset_ < outbuf_.size()) {
        loop_.modify(desc_, interest::read | interest::write);
      }
      return;
    }

    if (current != state::open) {
      return;
    }

    if (r.writable) {
      bool drained;
      {
        std::unique_lock lock(mutex_);
        pump_output(lock);
        drained = out_offset_ >= outbuf_.size();
      }
      if (drained) {
        loop_.modify(desc_, interest::read);
        drain_.emit();
        std::unique_lock lock(mutex_);
        if (ended_) {
          sock::shutdown(desc_, sock::shut::write);
        }
      }
    }

    if (r.readable || r.hangup) {
      drain_input();
    }
  }

  void drain_input() {
    std::byte tmp[65536];
    while (true) {
      sock::transfer got = sock::recv(desc_, std::span<std::byte>(tmp, sizeof(tmp)));
      if (got.count > 0) {
        data_.emit(core::make_buffer(
            std::span<const std::byte>(tmp, static_cast<std::size_t>(got.count))));
        continue;
      }
      if (got.count == 0) {
        end_.emit();
        // default allowHalfOpen=false: close our side too
        close_with(std::string{});
        return;
      }
      if (core::would_block(got.error)) {
        return;
      }
      if (got.error == std::errc::interrupted) {
        continue;
      }
      fail(sock::describe(got.error, "recv"));
      return;
    }
  }

  // Sends as much of outbuf_ as the kernel will take right now. Caller holds
  // `lock` on mutex_.
  void pump_output(std::unique_lock<std::mutex> &lock) {
    while (out_offset_ < outbuf_.size()) {
      sock::transfer sent = sock::send(
          desc_, std::span<const std::byte>(outbuf_.data() + out_offset_, outbuf_.size() - out_offset_));
      if (sent.count > 0) {
        out_offset_ += static_cast<std::size_t>(sent.count);
        continue;
      }
      if (sent.count < 0 && core::would_block(sent.error)) {
        return;
      }
      if (sent.count < 0 && sent.error == std::errc::interrupted) {
        continue;
      }
      lock.unlock();
      fail(sock::describe(sent.error, "send"));
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
      was_live = state_ != state::closed && core::descriptor::valid(desc_);
      state_ = state::closed;
    }
    if (!was_live) {
      return;
    }
    loop_.unwatch(desc_);
    {
      std::unique_lock lock(mutex_);
      core::descriptor::close(desc_);
    }
    close_.emit();
  }

  core::event_loop &loop_;
  mutable std::mutex mutex_;
  core::descriptor::state desc_;
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
