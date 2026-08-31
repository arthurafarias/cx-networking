// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

// lambdatech::networking::protocol::dns::client - a minimal stub resolver.
// It sends a query over a udp::peer to an upstream resolver and calls back
// with the parsed response (or std::nullopt on timeout). Outstanding
// queries are matched to replies by the 16-bit message id.
//
//   auto c = dns::client::create("1.1.1.1");
//   c->query("example.com", dns::record_type::a, [](std::optional<dns::message> m) {
//     if (m) { /* inspect m->header.rcode, ... */ }
//   });

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <unordered_map>
#include <utility>

#include <lambdatech/networking/core/event.hpp>
#include <lambdatech/networking/core/event_loop.hpp>
#include <lambdatech/networking/protocol/dns/message.hpp>
#include <lambdatech/networking/protocol/udp/peer.hpp>

namespace lambdatech::networking::protocol::dns {

namespace core = lambdatech::networking::core;

class client : public std::enable_shared_from_this<client> {
public:
  using answer_handler = std::function<void(std::optional<message>)>;

  static std::shared_ptr<client> create(std::string resolver_host = "127.0.0.1", std::uint16_t resolver_port = 53,
                                        core::event_loop &loop = core::event_loop::instance()) {
    auto c = std::shared_ptr<client>(new client(std::move(resolver_host), resolver_port, loop));
    c->wire_up();
    return c;
  }

  core::event<const std::string &> &on_error() { return error_; }

  // How long to wait for a reply before calling back with std::nullopt.
  void set_timeout(std::chrono::milliseconds timeout) { timeout_ = timeout; }

  void query(const std::string &name, std::uint16_t type, answer_handler on_answer) {
    message request;
    request.header.id = next_id();
    request.header.recursion_desired = true;
    request.questions.push_back({name, type, record_class::in});

    auto wire = request.serialize();
    if (!wire) {
      error_.emit("invalid question name: " + name);
      on_answer(std::nullopt);
      return;
    }

    std::uint16_t id = request.header.id;
    auto self = shared_from_this();
    auto timer = loop_.set_timeout(timeout_, [self, id] { self->expire(id); });
    pending_.emplace(id, pending{std::move(on_answer), timer});

    if (!peer_->send(*wire, resolver_port_, resolver_host_)) {
      resolve(id, std::nullopt); // peer already emitted the error
    }
  }

  std::size_t outstanding() const { return pending_.size(); }

private:
  struct pending {
    answer_handler handler;
    core::event_loop::timer_id timer;
  };

  client(std::string host, std::uint16_t port, core::event_loop &loop)
      : loop_(loop), resolver_host_(std::move(host)), resolver_port_(port), peer_(udp::peer::create("udp4", loop)),
        rng_(std::random_device{}()) {}

  void wire_up() {
    auto self = shared_from_this();
    peer_->on_message() += [self](const core::buffer &datagram, const core::socket_address &) {
      auto parsed = message::parse(datagram);
      if (!parsed) {
        self->error_.emit("received a malformed DNS response");
        return;
      }
      self->resolve(parsed->header.id, std::move(parsed));
    };
    peer_->on_error() += [self](const std::string &message) { self->error_.emit(message); };
  }

  void expire(std::uint16_t id) { resolve(id, std::nullopt); }

  void resolve(std::uint16_t id, std::optional<message> answer) {
    auto it = pending_.find(id);
    if (it == pending_.end()) {
      return;
    }
    pending entry = std::move(it->second);
    pending_.erase(it);
    loop_.clear_timeout(entry.timer);
    entry.handler(std::move(answer));
  }

  std::uint16_t next_id() { return static_cast<std::uint16_t>(std::uniform_int_distribution<std::uint32_t>(0, 0xFFFF)(rng_)); }

  core::event_loop &loop_;
  std::string resolver_host_;
  std::uint16_t resolver_port_;
  std::shared_ptr<udp::peer> peer_;
  std::mt19937 rng_;
  std::chrono::milliseconds timeout_{5000};
  std::unordered_map<std::uint16_t, pending> pending_;

  core::event<const std::string &> error_;
};

} // namespace lambdatech::networking::protocol::dns
