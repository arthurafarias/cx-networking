// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

// lambdatech::networking::protocol::dns::server - accepts DNS queries over
// UDP and hands each one to a 'query' listener together with a `responder`
// that knows where to send the reply. Events (subscribe with on_<name>() += cb):
//
//   listening    bound and ready
//   query        a parsed request    (dns::message, dns::server::responder)
//   error        a socket error      (std::string)
//   close        stopped
//
//   srv->on_query() += [](const dns::message &req, const dns::server::responder &reply) {
//     dns::message answer;
//     answer.header.rcode = dns::rcode::not_implemented;
//     answer.questions = req.questions;
//     reply.send(answer);            // id + QR bit are filled in for you
//   });

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include <lambdatech/networking/core/address.hpp>
#include <lambdatech/networking/core/event.hpp>
#include <lambdatech/networking/core/event_loop.hpp>
#include <lambdatech/networking/protocol/dns/message.hpp>
#include <lambdatech/networking/protocol/udp/peer.hpp>

namespace lambdatech::networking::protocol::dns {

namespace core = lambdatech::networking::core;

class server : public std::enable_shared_from_this<server> {
public:
  class responder {
  public:
    responder(std::shared_ptr<udp::peer> peer, core::socket_address to, std::uint16_t request_id)
        : peer_(std::move(peer)), to_(std::move(to)), request_id_(request_id) {}

    // Serializes `reply` and sends it back to the querier. The request id
    // and the QR (response) bit are set here so a handler can't forget them.
    bool send(message reply) const {
      reply.header.id = request_id_;
      reply.header.response = true;
      auto wire = reply.serialize();
      if (!wire || !peer_) {
        return false;
      }
      return peer_->send(*wire, to_.port, to_.address);
    }

    const core::socket_address &peer_address() const { return to_; }
    std::uint16_t request_id() const { return request_id_; }

  private:
    std::shared_ptr<udp::peer> peer_;
    core::socket_address to_;
    std::uint16_t request_id_;
  };

  static std::shared_ptr<server> create(core::event_loop &loop = core::event_loop::instance()) {
    return std::shared_ptr<server>(new server(loop));
  }

  core::event<> &on_listening() { return listening_; }
  core::event<const message &, const responder &> &on_query() { return query_; }
  core::event<const std::string &> &on_error() { return error_; }
  core::event<> &on_close() { return close_; }

  void listen(std::uint16_t port = 53, std::string address = "0.0.0.0") {
    auto self = shared_from_this();
    peer_->on_message() += [self](const core::buffer &datagram, const core::socket_address &from) {
      auto request = message::parse(datagram);
      if (!request) {
        self->error_.emit("dropped a malformed query datagram");
        return;
      }
      responder reply(self->peer_, from, request->header.id);
      self->query_.emit(*request, reply);
    };
    peer_->on_error() += [self](const std::string &message) { self->error_.emit(message); };
    peer_->on_listening() += [self] { self->listening_.emit(); };
    peer_->bind(port, std::move(address));
  }

  void close() {
    peer_->close();
    close_.emit();
  }

  const core::socket_address &address() const { return peer_->address(); }

private:
  explicit server(core::event_loop &loop) : loop_(loop), peer_(udp::peer::create("udp4", loop)) {}

  core::event_loop &loop_;
  std::shared_ptr<udp::peer> peer_;

  core::event<> listening_;
  core::event<const message &, const responder &> query_;
  core::event<const std::string &> error_;
  core::event<> close_;
};

} // namespace lambdatech::networking::protocol::dns
