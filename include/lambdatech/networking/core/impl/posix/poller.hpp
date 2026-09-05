// ---------------------------------------------------------------------------
// PROPRIETARY CODE – Arthur de Araújo Farias 2025
// All rights reserved.  No part of this file may be reproduced, stored in a
// retrieval system, or transmitted in any form or by any means—electronic,
// mechanical, photocopying, recording, or otherwise—without the prior written
// permission of the copyright holder.
// ---------------------------------------------------------------------------

#pragma once

// POSIX backend for core::poller (SRS-008 §4.4): poll(2) over the watched
// fds plus one internal eventfd for the cross-thread wake. Included only
// through core/poller.hpp. An epoll variant (M4) replaces this one file.

#include <chrono>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <vector>

#include <poll.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include <lambdatech/networking/core/descriptor.hpp>
#include <lambdatech/networking/core/poller_events.hpp>

namespace lambdatech::networking::core::poller::impl::posix {

inline short to_poll_mask(interest i) {
  short m = 0;
  if (has(i, interest::read)) {
    m |= POLLIN;
  }
  if (has(i, interest::write)) {
    m |= POLLOUT;
  }
  return m;
}

class state {
public:
  state() : wake_fd_(::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) {}

  state(const state &) = delete;
  state &operator=(const state &) = delete;
  state(state &&) = delete;
  state &operator=(state &&) = delete;

  ~state() {
    if (wake_fd_ >= 0) {
      ::close(wake_fd_);
    }
  }

  void set(int fd, short mask) {
    std::unique_lock lock(mutex_);
    fds_[fd] = mask;
  }
  void set_if_present(int fd, short mask) {
    std::unique_lock lock(mutex_);
    auto it = fds_.find(fd);
    if (it != fds_.end()) {
      it->second = mask;
    }
  }
  void erase(int fd) {
    std::unique_lock lock(mutex_);
    fds_.erase(fd);
  }

  void poke() const {
    std::uint64_t one = 1;
    [[maybe_unused]] auto n = ::write(wake_fd_, &one, sizeof(one));
  }

  int run(std::vector<event> &out, std::optional<std::chrono::milliseconds> timeout) {
    std::vector<pollfd> pfds;
    {
      std::unique_lock lock(mutex_);
      pfds.reserve(fds_.size() + 1);
      pfds.push_back(pollfd{wake_fd_, POLLIN, 0});
      for (const auto &[fd, mask] : fds_) {
        pfds.push_back(pollfd{fd, mask, 0});
      }
    }

    int ms = timeout ? static_cast<int>(timeout->count()) : -1;
    int n = ::poll(pfds.data(), pfds.size(), ms);
    if (n < 0) {
      return errno == EINTR ? 0 : -1;
    }

    if (pfds[0].revents & POLLIN) {
      std::uint64_t sink = 0;
      while (::read(wake_fd_, &sink, sizeof(sink)) > 0) {
      }
    }

    out.clear();
    for (std::size_t i = 1; i < pfds.size(); ++i) {
      short re = pfds[i].revents;
      if (re == 0) {
        continue;
      }
      out.push_back(event{pfds[i].fd,
                          ready{
                              .readable = (re & POLLIN) != 0,
                              .writable = (re & POLLOUT) != 0,
                              .error = (re & POLLERR) != 0,
                              .hangup = (re & POLLHUP) != 0,
                          }});
    }
    return static_cast<int>(out.size());
  }

private:
  int wake_fd_ = -1;
  mutable std::mutex mutex_;
  std::map<int, short> fds_;
};

// add / update / drop take effect on the next wait(), so each pokes the
// wake channel to make a blocked wait() re-poll (SRS-008 §4.3).
inline void add(state &p, descriptor::native_handle h, interest i) {
  p.set(h, to_poll_mask(i));
  p.poke();
}
inline void update(state &p, descriptor::native_handle h, interest i) {
  p.set_if_present(h, to_poll_mask(i));
  p.poke();
}
inline void drop(state &p, descriptor::native_handle h) {
  p.erase(h);
  p.poke();
}
inline void interrupt(state &p) { p.poke(); }
inline int wait(state &p, std::vector<event> &out, std::optional<std::chrono::milliseconds> timeout) {
  return p.run(out, timeout);
}

} // namespace lambdatech::networking::core::poller::impl::posix
