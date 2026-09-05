// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

// Standalone backend for core::poller - SRS-008 §4 / M3. Scaffold only: a
// deterministic readiness queue driven by the standalone socket fabric is
// not implemented yet. `wait` blocks on a condition variable so an
// event_loop built on it still parks instead of spinning.

#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>
#include <optional>
#include <vector>

#include <lambdatech/networking/core/descriptor.hpp>
#include <lambdatech/networking/core/poller_events.hpp>

namespace lambdatech::networking::core::poller::impl::standalone {

class state {
public:
  state() = default;
  state(const state &) = delete;
  state &operator=(const state &) = delete;
  state(state &&) = delete;
  state &operator=(state &&) = delete;

  std::mutex mutex;
  std::condition_variable cv;
  bool poked = false;
  std::map<descriptor::native_handle, interest> fds;
};

inline void interrupt(state &p);

inline void add(state &p, descriptor::native_handle h, interest i) {
  {
    std::unique_lock lock(p.mutex);
    p.fds[h] = i;
  }
  interrupt(p);
}
inline void update(state &p, descriptor::native_handle h, interest i) {
  {
    std::unique_lock lock(p.mutex);
    auto it = p.fds.find(h);
    if (it != p.fds.end()) {
      it->second = i;
    }
  }
  interrupt(p);
}
inline void drop(state &p, descriptor::native_handle h) {
  {
    std::unique_lock lock(p.mutex);
    p.fds.erase(h);
  }
  interrupt(p);
}
inline void interrupt(state &p) {
  {
    std::unique_lock lock(p.mutex);
    p.poked = true;
  }
  p.cv.notify_all();
}
inline int wait(state &p, std::vector<event> &out, std::optional<std::chrono::milliseconds> timeout) {
  std::unique_lock lock(p.mutex);
  if (timeout) {
    p.cv.wait_for(lock, *timeout, [&] { return p.poked; });
  } else {
    p.cv.wait(lock, [&] { return p.poked; });
  }
  p.poked = false;
  out.clear();
  return 0;
}

} // namespace lambdatech::networking::core::poller::impl::standalone
