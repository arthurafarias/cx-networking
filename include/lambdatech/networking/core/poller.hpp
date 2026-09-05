// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

// core::poller - the façade over readiness multiplexing and the cross-thread
// loop wake (SRS-008 §4). `event_loop` holds one `poller::state` and never
// touches poll(2) / eventfd directly. The internal wake channel is owned by
// the poller, not the loop.

#include <chrono>
#include <concepts>
#include <optional>
#include <vector>

#include <lambdatech/networking/core/config.hpp>
#include <lambdatech/networking/core/descriptor.hpp>
#include <lambdatech/networking/core/poller_events.hpp>

#if LNW_NET_BACKEND_POSIX
#  include <lambdatech/networking/core/impl/posix/poller.hpp>
#elif LNW_NET_BACKEND_STANDALONE
#  include <lambdatech/networking/core/impl/standalone/poller.hpp>
#endif

namespace lambdatech::networking::core::poller {

#if LNW_NET_BACKEND_POSIX
namespace backend = impl::posix;
#elif LNW_NET_BACKEND_STANDALONE
namespace backend = impl::standalone;
#endif

// state is the backend's: default-constructs ready to use, owns the wake
// channel, is non-copyable and non-movable.
using state = backend::state;

inline void add(state &p, descriptor::native_handle h, interest i) { backend::add(p, h, i); }
inline void update(state &p, descriptor::native_handle h, interest i) { backend::update(p, h, i); }
inline void drop(state &p, descriptor::native_handle h) { backend::drop(p, h); }
inline void interrupt(state &p) { backend::interrupt(p); }
inline int wait(state &p, std::vector<event> &out, std::optional<std::chrono::milliseconds> timeout) {
  return backend::wait(p, out, timeout);
}

// --- backend contract (SRS-008 §2.3) -----------------------------------

static_assert(
    requires(state &p, descriptor::native_handle h, std::vector<event> out,
             std::optional<std::chrono::milliseconds> t) {
      { backend::add(p, h, interest::read) };
      { backend::update(p, h, interest::read) };
      { backend::drop(p, h) };
      { backend::interrupt(p) };
      { backend::wait(p, out, t) } -> std::same_as<int>;
    },
    "selected core::poller backend is incomplete (SRS-008 §2.3)");
static_assert(!std::copyable<state> && !std::movable<state>,
              "core::poller::state must own its wake channel in place (non-movable)");

} // namespace lambdatech::networking::core::poller
