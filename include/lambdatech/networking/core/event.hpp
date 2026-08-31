// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

// core::event<Args...> is one named event on an emitter, wrapping a
// core::signal. Emitters expose one accessor per event; it gives the
// Node.js EventEmitter surface:
//
//   sock->on_data()  += [](const buffer &chunk) { ... };   // emitter.on('data', cb)
//   sock->on_connect().once([] { ... });                    // emitter.once('connect', cb)
//   sock->on_error() += handler;
//
// operator+= proxies straight to signal::connect. The emitting side (the
// socket internals) calls emit(...). Listeners run synchronously on the
// event-loop thread, in subscription order.

#include <functional>
#include <memory>
#include <utility>

#include <lambdatech/networking/core/signal.hpp>

namespace lambdatech::networking::core {

template <typename... args_types> class event {
public:
  using listener = std::function<void(args_types...)>;
  using subscription = typename signal<args_types...>::connection;

  // emitter.on(name, listener) - proxies straight to signal::connect.
  event &operator+=(listener fn) {
    signal_.connect(std::move(fn));
    return *this;
  }

  // emitter.once(name, listener) - fires at most once, then self-removes.
  subscription once(listener fn) {
    auto slot = std::make_shared<subscription>();
    *slot = signal_.connect([slot, fn = std::move(fn)](args_types... args) {
      slot->disconnect();
      fn(args...);
    });
    return *slot;
  }

  // emitter.removeAllListeners(name)
  void off_all() { signal_.disconnect_all(); }

  std::size_t listener_count() const { return signal_.slot_count(); }

  // The emitting side. Not called by consumers of a socket.
  void emit(args_types... args) const { signal_.emit(args...); }

private:
  signal<args_types...> signal_;
};

} // namespace lambdatech::networking::core
